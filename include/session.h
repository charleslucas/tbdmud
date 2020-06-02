#ifndef TBDMUD_SESSION_H_INCLUDED
#define TBDMUD_SESSION_H_INCLUDED

#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <queue>
#include <vector>
#include <tbdmud.h>

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;

using command_handler = std::function<void (std::string)>;
using error_handler = std::function<void ()>;

// Create shared-pointer session objects for each connected client
class session : public std::enable_shared_from_this<session>
{
private:
    tcp::socket socket;                          // The socket for this client
    io::streambuf streambuf;                     // Incoming data
    std::queue<std::string> outgoing;            // Outgoing messages
    command_handler on_command;                  // Client command handler
    error_handler   on_error;                    // Client error handler
    uint session_id = 0;
    std::shared_ptr<tbdmud::player>  player;     // Once a client has been authenticated they will populate the player data from file
                                                 // This session creates the player object, but the server will own it
    //std::function<void(session*, std::shared_ptr<tbdmud::character>)> register_character;  // A function pointer to the world's register_character function to pass to session objects
    std::function<std::shared_ptr<tbdmud::character>(session*, std::string)> create_character;  // A function pointer to the world's register_character function to pass to session objects

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

            // This session creates the player object, but the server will own it
            player = std::shared_ptr<tbdmud::player>(new tbdmud::player(username.str(), session_id, true, substrings[0], std::stoi(substrings[1])));
            
            std::cout << "User " << player->get_name() << " has connected from " << player->get_ip() << ":" << player->get_port() << std::endl << std::endl;
            post("User " + player->get_name() + " has connected.\n");

            std::cout << "Session->Creating new character " << player->get_name() << std::endl;
            //register_character(this, player->get_character());
            player->set_character(create_character(this, player->get_name()));

            async_read();               // Start handling asynchronous command inputs
        }
        else
        {
            // Call the error handler, then exit
            player->set_connected(false);
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

public:

    // Constructor - initialize our internal socket from the passed-in socket
    session(tcp::socket&& socket, uint sid, std::function<std::shared_ptr<tbdmud::character>(session*, std::string)> cc) : socket(std::move(socket))
    {
        session_id = sid;
        create_character = cc;
    }

    // Register the passed-in message and error handler functions to the session object, start asynchronous socket reads
    void start(command_handler&& on_command, error_handler&& on_error)
    {
        this->on_command = std::move(on_command);
        this->on_error = std::move(on_error);
        
        // Call async_login_username() first, then async_login_username() will asynchronously call the command handler by calling async_read()
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

    // Return a shared pointer to the player object
    std::shared_ptr<tbdmud::player> get_player() {
        return player;
    }
};

#endif
