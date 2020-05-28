#include <iostream>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <optional>
#include <queue>
#include <unordered_set>
#include <tbdmud.h>

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;

using message_handler = std::function<void (std::string)>;
using error_handler = std::function<void ()>;

// Create shared-pointer session objects for each connected client
class session : public std::enable_shared_from_this<session>
{
public:

    tbdmud::player    player;     // Once a client has been authenticated they will populate the player data from file
    tbdmud::character character;  // Once a player has been populated they will choose their character for this session

    // Constructor - initialize our internal socket from the passed-in socket
    session(tcp::socket&& socket) : socket(std::move(socket))
    {
    }

    // Register the passed-in message and error handler functions to the session object, start asynchronous socket reads
    void start(message_handler&& on_message, error_handler&& on_error)
    {
        this->on_message = std::move(on_message);
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

    void on_username(error_code error, std::size_t bytes_transferred) {
        std::string delimiters = ": ";
        std::vector<std::string> substrings;
        
        if(!error)
        {
            // Create a message from received data, call the message handler
            std::stringstream username;
            std::stringstream client_ip;

            client_ip << socket.remote_endpoint(error);    // Grab and store the client's IP address and port
            username << std::istream(&streambuf).rdbuf();  // Grab the name input from the connected client
            streambuf.consume(bytes_transferred);

            player.connected = true;

            boost::split(substrings, client_ip.str(), boost::is_any_of(delimiters), boost::token_compress_on);  // Split the IP address and port

            player.ip_address = substrings[0];
            player.port = std::stoi(substrings[1]);

            player.username = username.str();
            boost::trim_right(player.username);  // Remove the trailing \n

            std::cout << "User " << player.username << " has connected from " << player.ip_address << ":" << player.port << std::endl << std::endl;
            post("User " + player.username + " has connected.\n");

            //on_message(message.str());  // Call the custom message handler
            async_read();               // Start handling message reads
        }
        else
        {
            // Call the error handler, then exit
            socket.close(error);
            on_error();
        }
    }

    // Call the message or error handlers depending on input
    void on_read(error_code error, std::size_t bytes_transferred)
    {
        if(!error)
        {
            // Create a message from received data, call the message handler
            std::stringstream message;
            message << player.username << ":  " << std::istream(&streambuf).rdbuf();
            streambuf.consume(bytes_transferred);
            on_message(message.str());  // Call the custom message handler
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

    tcp::socket socket;                 // The socket for this client
    io::streambuf streambuf;            // Incoming data
    std::queue<std::string> outgoing;   // Outgoing messages
    message_handler on_message;         // Message handler
    error_handler on_error;             // Error handler
};

class server
{
public:

    int num_connections = 0;

    // Class Constructor
    server(io::io_context& io_context, std::uint16_t port) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
    }

    void async_accept()
    {
        socket.emplace(io_context);

        // Anytime a new client connects, run this lambda
        acceptor.async_accept(*socket, [&] (error_code error)
        {
            const std::string welcome_msg = "\n\rWelcome to TBDMud!\n\r\n\r";
            const std::string connect_notice = "has connected to TBDMud!\n\r";

            // Create a reference to the new client's session
            auto client = std::make_shared<session>(std::move(*socket));

            // Write our welcome message to the new client
            client->post(welcome_msg);

            // Post a notice to the already-connected clients
            post("Someone else " + connect_notice);

            // Add our new client to the full list of connected clients
            clients.insert(client);
            num_connections++;
            std::cout << "Number of connections:  " << num_connections << std::endl;

            // Start the asynchronous command handler for this client entering the game
            client->start
            (
                // Pass in message handler
                std::bind(&server::post, this, std::placeholders::_1),

                // Pass in error handler
                [&, client]
                {
                    const std::string username = client->player.username;  // Copy the name before we delete the client

                    if(clients.erase(client))
                    {
                        post(username + " has disconnected - we are one less\n\r");
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

private:

    io::io_context& io_context;                             // The main I/O service provider that handles executing asynchronous scheduled tasks
    tcp::acceptor acceptor;                                 // An object that accepts incoming connections
    std::optional<tcp::socket> socket;                      // A socket object that can be "null"
    std::unordered_set<std::shared_ptr<session>> clients;   // A set of connected clients
};

int main()
{
    io::io_context io_context;

    server srv(io_context, 15001);
    srv.async_accept();
    io_context.run();  // Invoke the completion handlers

    return 0;
}