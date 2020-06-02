#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <optional>
#include <queue>
#include <unordered_set>
#include <events.h>
#include <tbdmud.h>

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;

using command_handler = std::function<void (std::string)>;
using error_handler = std::function<void ()>;

// Create shared-pointer session objects for each connected client
class session : public std::enable_shared_from_this<session>
{
public:

    std::shared_ptr<tbdmud::player>  player;     // Once a client has been authenticated they will populate the player data from file
    //tbdmud::character character;  // Once a player has been populated they will choose their character for this session

    // Constructor - initialize our internal socket from the passed-in socket
    session(tcp::socket&& socket, uint sid, std::shared_ptr<tbdmud::world> w) : socket(std::move(socket))
    {
        session_id = sid;
        world = w;
    }

    // Register the passed-in message and error handler functions to the session object, start asynchronous socket reads
    void start(command_handler&& on_command, error_handler&& on_error)
    {
        this->on_command = std::move(on_command);
        this->on_error = std::move(on_error);
        //async_read();
        async_login_username();
    }

    // Message handler - put a message in the outgoing queue - if we're idle and not sending a message already, start the write process
    void post(std::string const& message)
    {
        bool idle = outgoing.empty();
        outgoing.push(message);

        if(idle)
        {
            async_write();
        }
    }

    void login() {
        async_login_username();  // Get username or "new" from user
        //login_password();  // TODO
    }

private:

    uint session_id = 0;

    void async_login_username() {
        const std::string login_prompt = "Enter username or \"new\": --> ";

        post(login_prompt);
        io::async_read_until(socket, streambuf, "\n", [self = shared_from_this()] (error_code error, std::size_t bytes_transferred)
        {
            self->on_username(error, bytes_transferred);
        });
    }

    // Asynchronously receive data from the TCP socket
    void async_read()
    {
        // Continue reading from the socket until we encounter a Return key
        // Pass in the receive buffer and a function to run afterwards to process intputs and handle errors
        // Capture self's shared pointer into the completion handler's lambda to keep the session object alive until the handler is invoked
        io::async_read_until(socket, streambuf, "\n", [self = shared_from_this()] (error_code error, std::size_t bytes_transferred)
        {
            self->on_read(error, bytes_transferred);
        });
    }

    // Validate user login and create the player object
    void on_username(error_code error, std::size_t bytes_transferred) {
        const std::string delimiters = ": ";
        std::vector<std::string> substrings;
        
        if(!error)
        {
            // Create a message from received data, call the message handler
            std::stringstream username;
            std::stringstream client_ip;

            client_ip << socket.remote_endpoint(error);    // Grab and store the client's IP address and port
            username << std::istream(&streambuf).rdbuf();  // Grab the name input from the connected client
            streambuf.consume(bytes_transferred);

            boost::split(substrings, client_ip.str(), boost::is_any_of(delimiters), boost::token_compress_on);  // Split the IP address and port

            //player = std::shared_ptr<tbdmud::player>(new tbdmud::player(username.str(), session_id));
            player = std::shared_ptr<tbdmud::player>(new tbdmud::player(username.str(), session_id, true, substrings[0], std::stoi(substrings[1])));
            
            //player->username = username.str();
            //player->connected = true;
            //player->ip_address = substrings[0];
            //player->port = std::stoi(substrings[1]);

            std::cout << "User " << player->username << " has connected from " << player->ip_address << ":" << player->port << std::endl << std::endl;
            post("User " + player->username + " has connected.\n");

            std::cout << "Pre-World->Register Character  " << player->get_pc()->get_name() << std::endl;
            world->register_character(player->get_pc());

            async_read();               // Start handling asynchronous command inputs
        }
        else
        {
            // Call the error handler, then exit
            player->connected = false;
            socket.close(error);
            on_error();
        }
    }

    // Call the command or error handlers depending on input
    void on_read(error_code error, std::size_t bytes_transferred)
    {
        if(!error)
        {
            std::stringstream line;
            //line << player->username << ":  " << std::istream(&streambuf).rdbuf();
            line << std::istream(&streambuf).rdbuf();
            streambuf.consume(bytes_transferred);

            on_command(line.str());     // Pass the received line string to the command handler to decode commands and create events
            async_read();               // Recursive call to do the next read
        }
        else
        {
            // Call the error handler, then exit
            socket.close(error);
            on_error();
        }
    }

    // Write the current contents of the outgoing buffer to the socket
    void async_write()
    {
        // Pass in the front of the message queue and a function to run afterwards to clean up and handle errors
        io::async_write(socket, io::buffer(outgoing.front()), [self = shared_from_this()] (error_code error, std::size_t bytes_transferred)
        {
            self->on_write(error, bytes_transferred);
        });
    }

    // Pop the sent data from the outgoing message queue
    void on_write(error_code error, std::size_t bytes_transferred)
    {
        if(!error)
        {
            outgoing.pop();

            // Do a write if we still have messages in the queue
            if(!outgoing.empty())
            {
                async_write();
            }
        }
        else
        {
            socket.close(error);
            on_error();
        }
    }

    tcp::socket socket;                      // The socket for this client
    io::streambuf streambuf;                 // Incoming data
    std::queue<std::string> outgoing;        // Outgoing messages
    command_handler on_command;              // Client command handler
    error_handler   on_error;                // Client error handler
    std::shared_ptr<tbdmud::world> world;    // Pointer to the world object in the server
};

class server
{
public:

    uint num_connections = 0;

    // Class Constructor
    server(io::io_context& io_context, std::uint16_t port) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
    }

    // Class Constructor
    server(io::io_context& io_context, std::uint16_t port, std::shared_ptr<tbdmud::world> world_ptr) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
        world = world_ptr;
    }

    void async_accept()
    {
        socket.emplace(io_context);

        // Anytime a new client connects, run this lambda
        acceptor.async_accept(*socket, [&] (error_code error)
        {
            const std::string welcome_msg = "\n\rWelcome to TBDMud!\n\r\n\r";
            const std::string connect_notice = "has connected to TBDMud!\n\r";

            num_connections++;
            std::cout << "Number of connections:  " << num_connections << std::endl;

            // Create a reference to the new client's session
            std::shared_ptr<session> client = std::make_shared<session>(std::move(*socket), num_connections, world);

            // Write our welcome message to the new client
            client->post(welcome_msg);

            // Post a notice to the already-connected clients
            post("Someone else " + connect_notice);

            // Add our new client to the full list of connected clients
            clients.insert(client);

            // Start the asynchronous command handler for this client entering the game
            client->start
            (
                // Pass in the command handler
                //std::bind(&server::post, this, std::placeholders::_1),
                std::bind(&server::command_parse, this, client, std::placeholders::_1),

                // Pass in the error handler (runs on disconnect)
                [&, client]
                {
                    const std::string username = client->player->username;  // Copy the name before we delete the client

                    if(clients.erase(client))
                    {
                        post(username + " has disconnected.\n\r");
                        num_connections--;
                        std::cout << "Number of connections:  " << num_connections << std::endl;
                    }
                }
            );

            async_accept();
        });
    }

    // Send a message to all connected clients
    void post(std::string const& message)
    {
        for(auto& client : clients)
        {
            client->post(message);
        }
    }

    // The commands that are valid for a player to use
    enum valid_commands {
        HELP,             // Help message
        WHO,              // See who is currently logged into the server
        LOOK,             // Look at the current room and what is in it
        TELL,             // Send a message to a specific player
        SAY,              // Say something to everyone in the current room
        SHOUT,            // Shout to everyone in the zone
        BROADCAST         // Broadcast a message to everyone in the world
    };

    // Decode commands given by the client, create events and put them in the queue
    // Multiple commands can be given on a line, separated by ;
    // Individual command arguments are separated by spaces: <command> <arg1> <arg2>, etc
    // After all explicit commands are checked for, we will check to see if the command matches valid exits from the room
    void command_parse(std::shared_ptr<session> client, std::string line)
    {
        const std::string command_delimiters = ";";
        const std::string parameter_delimiters = " ";
        std::vector<std::string> commands;
        std::shared_ptr<tbdmud::event_queue> eq = client->player->get_pc()->get_event_queue();

        line.pop_back();  // Remove the CR that comes with the line
        boost::split(commands, line, boost::is_any_of(command_delimiters), boost::token_compress_on);  // Split the commands

        // Iterate over each command
        for(std::string c : commands) {
            std::cout << "Parsing command:  " << c << std::endl;
            std::vector<std::string> parameters;

            boost::split(parameters, c, boost::is_any_of(parameter_delimiters), boost::token_compress_on);  // Split the parameters
            std::cout << "split" << std::endl;

            if (boost::iequals(parameters.front(), "?")) {
                std::cout << "Help Requested" << std::endl;
                client->post("Valid Commands:");
            }
            else if (boost::iequals(parameters.front(), "tell")) {
                std::shared_ptr<tbdmud::event_item> say_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                say_event->set_type(tbdmud::event_type::SPEAK);
                say_event->set_message(tbdmud::event_scope::TARGET, "blah");

                std::cout << "say event:  " << eq->name << std::endl;
                eq->add_event(say_event);
            }
            else if (boost::iequals(parameters.front(), "say")) {
                std::shared_ptr<tbdmud::event_item> say_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                say_event->set_type(tbdmud::event_type::SPEAK);
                say_event->set_message(tbdmud::event_scope::ROOM, "blah");

                std::cout << "say event:  " << eq->name << std::endl;
                eq->add_event(say_event);
            }
            else {

            }
        }
    }

private:
    io::io_context& io_context;                             // The main I/O service provider that handles executing asynchronous scheduled tasks
    tcp::acceptor acceptor;                                 // An object that accepts incoming connections
    std::optional<tcp::socket> socket;                      // A socket object that can be "null"
    std::unordered_set<std::shared_ptr<session>> clients;   // A set of connected clients
    std::shared_ptr<tbdmud::world> world;                   // Pointer to the world object in the server
};

// Call the tick function of the world approximately once a second (doesn't have to be exact)
void async_tick(const boost::system::error_code& /*e*/, io::steady_timer* t, tbdmud::world* w)
{
    w->tick();

    // Move the expiration time of our timer up by one second and set it again
    t->expires_at(t->expiry() + io::chrono::seconds(1));
    t->async_wait(boost::bind(async_tick, io::placeholders::error, t, w));
}

// Handle an event from the queue as soon the current time matches the event time
void async_handle_queue(const boost::system::error_code& /*e*/, io::deadline_timer* t, tbdmud::world* w)
{
    w->process_events();
    //std::cout << "blah" << std::endl;

    // Move the expiration time of our timer up by one millisecond and set it again
    t->expires_from_now(boost::posix_time::milliseconds(1));                              // Run as often as possible
    t->async_wait(boost::bind(async_handle_queue, io::placeholders::error, t, w));
}

int main()
{
    tbdmud::world world;
    io::io_context io_context;
    io::steady_timer   ticktimer(io_context,  io::chrono::seconds(1));
    io::deadline_timer queuetimer(io_context);
    server srv(io_context, 15001, std::make_shared<tbdmud::world>(world));

    // Tasks to be asynchronously run by the server
    srv.async_accept();                                                                           // Asynchronously accept incoming TCP traffic
    ticktimer.async_wait(boost::bind(async_tick, io::placeholders::error, &ticktimer, &world));   // Asynchronously but regularly trigger a tick update
    queuetimer.expires_from_now(boost::posix_time::milliseconds(100));                              // Run as often as possible
    queuetimer.async_wait(boost::bind(async_handle_queue, io::placeholders::error, &queuetimer, &world));

    io_context.run();  // Invoke the completion handlers

    return 0;
}