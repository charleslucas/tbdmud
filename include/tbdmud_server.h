#ifndef TBDMUD_SERVER_H_INCLUDED
#define TBDMUD_SERVER_H_INCLUDED

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <queue>
#include <tbdmud.h>

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;

class server
{
private:
    io::io_context& io_context;                             // The main I/O service provider that handles executing asynchronous scheduled tasks
    tcp::acceptor acceptor;                                 // An object that accepts incoming connections
    std::optional<tcp::socket> socket;                      // A socket object that can be "null"
    std::unordered_set<std::shared_ptr<session>> clients;   // A set of connected clients
    tbdmud::world* world;                                   // Pointer to the world object in the server
    std::vector<tbdmud::player> players;                    // Server tracks a list of players, but the session adds/removes from the vector
    std::function<void(session*, std::shared_ptr<tbdmud::character>)> register_character;  // Create a function pointer to the world's register_character function to pass to session objects

public:

    uint num_connections = 0;

    // Class Constructor
    server(io::io_context& io_context, std::uint16_t port) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
    }

    // Class Constructor
    server(io::io_context& io_context, std::uint16_t port, tbdmud::world* world_ptr) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
        world = world_ptr;  // Store a point to the world object

        // Create a function pointer to the world's register_character function to pass to session objects
        register_character = std::bind(&tbdmud::world::register_character, world, std::placeholders::_1, std::placeholders::_2);
    }

    void async_accept()
    {
        socket.emplace(io_context);

        // Anytime a new client connects
        acceptor.async_accept(*socket, [&] (error_code error)
        {
            const std::string welcome_msg = "\n\rWelcome to TBDMud!\n\r\n\r";
            const std::string connect_notice = "has connected to TBDMud!\n\r";

            num_connections++;
            std::cout << "Number of connections:  " << num_connections << std::endl;

            // Create a reference to the new client's session
            std::shared_ptr<session> client = std::make_shared<session>(std::move(*socket), num_connections, register_character);

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
                    const std::string username = client->get_player()->username;  // Copy the name before we delete the client

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
        std::shared_ptr<tbdmud::event_queue> eq = client->get_player()->get_character()->get_event_queue();

        line.pop_back();  // Remove the CR that comes with the line
        boost::split(commands, line, boost::is_any_of(command_delimiters), boost::token_compress_on);  // Split the commands

        // Iterate over each command
        for(std::string c : commands) {
            std::vector<std::string>           parameters;
            std::vector<std::string>::iterator i;

            std::cout << "Parsing command:  " << c << std::endl;

            boost::split(parameters, c, boost::is_any_of(parameter_delimiters), boost::token_compress_on);  // Split the parameters

            if (boost::iequals(parameters.front(), "?")) {
                std::cout << "Help Requested" << std::endl;
                client->post("Valid Commands:");
            }
            else if (boost::iequals(parameters.front(), "who")) {
            }
            else if (boost::iequals(parameters.front(), "look")) {
            }
            else if (boost::iequals(parameters.front(), "tell")) {
                std::shared_ptr<tbdmud::event_item> tell_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                tell_event->set_origin(client->get_player()->get_character()->get_name());
                tell_event->set_name("TELL");
                tell_event->set_type(tbdmud::event_type::SPEAK);
                tell_event->set_scope(tbdmud::event_scope::TARGET);

                //TODO:  Check if the target player is connected
                std::shared_ptr<session> target;
                bool match = false;
                for(auto& c : clients)
                {
                    if(c->get_player()->get_character()->get_name() == parameters[1]) {
                        tell_event->set_target(parameters[1]);
                        match = true;
                    }
                }

                if (match == false) {
                    std::string error = "Player " + parameters[1] + " is not connected.\n"; 
                    client->post(error);
                    return;
                }

                // Chop the first and second words off the string, keep the rest as the message
                for (int j = 0; j < 2; j++) {
                    size_t space = c.find(" ");    
                    if (space != std::string::npos) {
                      c = c.substr(space + 1);
                    }
                }

                tell_event->set_message(tbdmud::event_scope::TARGET, c);

                std::cout << "tell event to :  " << tell_event->get_target() << " : " << tell_event->get_message(tbdmud::event_scope::TARGET) << std::endl;
                eq->add_event(tell_event);
            }
            else if (boost::iequals(parameters.front(), "say")) {
                std::shared_ptr<tbdmud::event_item> say_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                say_event->set_origin(client->get_player()->get_character()->get_name());
                say_event->set_name("SAY");
                say_event->set_type(tbdmud::event_type::SPEAK);
                say_event->set_scope(tbdmud::event_scope::ROOM);

                // Chop the first word off the string, keep the rest as the message
                size_t space = c.find(" ");    
                if (space != std::string::npos) {
                  c = c.substr(space + 1);
                }
                say_event->set_message(tbdmud::event_scope::ROOM, c);

                std::cout << "say event:  " << say_event->get_name() << std::endl;
                eq->add_event(say_event);
            }
            // TODO:  Remove temporary command "say with delay" for testing event delays
            else if (boost::iequals(parameters.front(), "dsay")) {
                std::shared_ptr<tbdmud::event_item> dsay_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                dsay_event->set_origin(client->get_player()->get_character()->get_name());
                dsay_event->set_name("DSAY");
                dsay_event->set_rtick(5);
                dsay_event->set_type(tbdmud::event_type::SPEAK);
                dsay_event->set_scope(tbdmud::event_scope::ROOM);

                // Chop the first word off the string, keep the rest as the message
                size_t space = c.find(" ");    
                if (space != std::string::npos) {
                  c = c.substr(space + 1);
                }
                dsay_event->set_message(tbdmud::event_scope::ROOM, c);

                std::cout << "dsay event:  " << dsay_event->get_name() << std::endl;
                eq->add_event(dsay_event);
            }
            else if (boost::iequals(parameters.front(), "shout")) {
                std::shared_ptr<tbdmud::event_item> shout_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                shout_event->set_origin(client->get_player()->get_character()->get_name());
                shout_event->set_name("SHOUT");
                shout_event->set_type(tbdmud::event_type::SPEAK);
                shout_event->set_scope(tbdmud::event_scope::ZONE);

                // Chop the first word off the string, keep the rest as the message
                size_t space = c.find(" ");    
                if (space != std::string::npos) {
                  c = c.substr(space + 1);
                }
                shout_event->set_message(tbdmud::event_scope::ZONE, c);

                std::cout << "shout event:  " << shout_event->get_name() << std::endl;
                eq->add_event(shout_event);
            }
            else if (boost::iequals(parameters.front(), "broadcast")) {
                std::shared_ptr<tbdmud::event_item> broadcast_event = std::shared_ptr<tbdmud::event_item>(new tbdmud::event_item());
                broadcast_event->set_origin(client->get_player()->get_character()->get_name());
                broadcast_event->set_name("BROADCAST");
                broadcast_event->set_type(tbdmud::event_type::SPEAK);
                broadcast_event->set_scope(tbdmud::event_scope::WORLD);

                // Chop the first word off the string, keep the rest as the message
                size_t space = c.find(" ");    
                if (space != std::string::npos) {
                  c = c.substr(space + 1);
                }
                broadcast_event->set_message(tbdmud::event_scope::ZONE, c);

                std::cout << "broadcast event:  " << broadcast_event->get_name() << std::endl;
                eq->add_event(broadcast_event);
            }
            else {
                // TODO:  See if parameters.front() equals one of the defined exits from the room
            }
        }
    }
};

#endif
