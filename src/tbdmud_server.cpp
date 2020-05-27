#include <boost/asio.hpp>
#include <optional>
#include <queue>
#include <unordered_set>

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;

using message_handler = std::function<void (std::string)>;
using error_handler = std::function<void ()>;

// Create shared-pointer session objects for each connected client
class session : public std::enable_shared_from_this<session>
{
public:

    // Constructor - initialize our internal socket from the passed-in socket
    session(tcp::socket&& socket) : socket(std::move(socket))
    {
    }

    // Make the passed-in message and error handler functions part of the session object, do an asynchronous socket read
    void start(message_handler&& on_message, error_handler&& on_error)
    {
        this->on_message = std::move(on_message);
        this->on_error = std::move(on_error);
        async_read();
    }

    // Put a message in the outgoing queue - if we're idle and not sending a message already, start the write process
    void post(std::string const& message)
    {
        bool idle = outgoing.empty();
        outgoing.push(message);

        if(idle)
        {
            async_write();
        }
    }

private:

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

    // Call the message or error handlers depending on input
    void on_read(error_code error, std::size_t bytes_transferred)
    {
        if(!error)
        {
            // Create a message from received data, call the message handler
            std::stringstream message;
            message << socket.remote_endpoint(error) << ": " << std::istream(&streambuf).rdbuf();
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

    // Constructor
    server(io::io_context& io_context, std::uint16_t port) : io_context(io_context), acceptor(io_context, tcp::endpoint(tcp::v4(), port))
    {
    }

    void async_accept()
    {
        socket.emplace(io_context);

        // Anytime a new client connects, run this lambda
        acceptor.async_accept(*socket, [&] (error_code error)
        {
            // Create a reference to the new client's session
            auto client = std::make_shared<session>(std::move(*socket));

            // Write our welcome message to the new client
            client->post("Welcome to chat\n\r");

            // Post a notice to the already-connected clients
            post("We have a newcomer\n\r");

            // Add our new client to the full list of connected clients
            clients.insert(client);

            // Start the asynchronous traffic handling for this client's session
            client->start
            (
                // Error handler runs if an error or the client disconnects
                std::bind(&server::post, this, std::placeholders::_1),
                [&, client]
                {
                    if(clients.erase(client))
                    {
                        post("We are one less\n\r");
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