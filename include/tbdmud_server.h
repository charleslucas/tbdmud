#ifndef TBDMUD_SERVER_H_INCLUDED
#define TBDMUD_SERVER_H_INCLUDED

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <queue>
#include <entities.h>

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
    uint num_connections = 0;

    tbdmud::world* world;                                   // Pointer to the world object in the server
    std::function<std::shared_ptr<tbdmud::character>(session*, std::string)> create_character;    // Create a function pointer to the world's create_character() function
    std::function<void(session*, std::shared_ptr<tbdmud::character>)>        register_character;  // Create a function pointer to the world's register_character() function
    std::function<void(std::string)>                                         remove_character;    // Create a function pointer to the world's remove_character() function
    std::function<bool(std::string)>                                         player_exists;       // Create a function pointer to the server's does_player_exist() function

public:

    // Class Constructor
    server(io::io_context& io_context, std::uint16_t port) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
    }

    // Class Constructor that accepts a world object pointer
    server(io::io_context& io_context, std::uint16_t port, tbdmud::world* world_ptr) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
        world = world_ptr;  // Store a point to the world object

        // Create a function pointer to the world's character handling functions that can be used here or passed to session objects
          create_character = std::bind(&tbdmud::world::create_character  , world, std::placeholders::_1, std::placeholders::_2);
        register_character = std::bind(&tbdmud::world::register_character, world, std::placeholders::_1, std::placeholders::_2);
          remove_character = std::bind(&tbdmud::world::remove_character,   world, std::placeholders::_1);
          player_exists    = std::bind(&server::does_player_exist, this, std::placeholders::_1);
    }

    bool does_player_exist(std::string name) {
        if (boost::iequals(name, "world")) return true;  // TODO:  Fix that if someone names themselves "world" the server crashes

        // Test to see if that player name (or a case-insensitive version of it) already exists on the server
        for (std::shared_ptr<session> client : clients) {
            if (client->get_player() != nullptr) {
                if (boost::iequals(name, client->get_player()->get_name())) return true;
            }
        }

        return false;
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

            // Create the new client's session
            std::shared_ptr<session> client = std::make_shared<session>(std::move(*socket), num_connections, create_character, player_exists);

            // Write our welcome message to the new client
            client->post(welcome_msg);

            // Post a notice to the already-connected clients
            post("Someone else " + connect_notice);

            // Add our new client to the full list of connected clients
            clients.insert(client);

            // Start the asynchronous command handler for this client entering the game
            client->start
            (
                // Pass in the command handler in the world object
                //std::bind(&server::post, this, std::placeholders::_1),
                std::bind(&tbdmud::world::command_parse, world, client, std::placeholders::_1),

                // Pass in the error handler (runs on disconnect)
                [&, client]
                {
                    const std::string character_name = client->get_player()->get_character()->get_name();  // Copy the name before we delete the client

                    if(clients.erase(client))
                    {
                        post(character_name + " has disconnected.\n\r");
                        remove_character(character_name);  // Remove the character from the world

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

};

#endif
