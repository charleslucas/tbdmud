#include "connection.hpp"
#include <serverpp/tcp_server.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>
#include "player.h"

namespace {

serverpp::byte_storage rot13_encode(serverpp::bytes data)
{
    auto const &encode_byte = [](serverpp::byte datum)
    {
        if (datum >= 'A' && datum <= 'Z')
        {
            return serverpp::byte('A' + (((datum - 'A') + 13) % 26));
        }
        else if (datum >= 'a' && datum <= 'z')
        {
            return serverpp::byte('a' + (((datum - 'a') + 13) % 26));
        }
        else if (datum == '\0')
        {
            return serverpp::byte('\n');
        }
        else
        {
            return datum;
        }
        
    };

    auto const encoded_data = data | boost::adaptors::transformed(encode_byte);
    return serverpp::byte_storage{encoded_data.begin(), encoded_data.end()};
}

serverpp::byte_storage command_echo(serverpp::bytes data)
{
    //auto const &encode_byte = [](serverpp::byte datum)
    //{
    //    if (datum >= 'A' && datum <= 'Z')
    //    {
    //        return serverpp::byte('A' + (((datum - 'A') + 13) % 26));
    //    }
    //    else if (datum >= 'a' && datum <= 'z')
    //    {
    //        return serverpp::byte('a' + (((datum - 'a') + 13) % 26));
    //    }
    //    else if (datum == '\0')
    //    {
    //        return serverpp::byte('\n');
    //    }
    //    else
    //    {
    //        return datum;
    //    }
    //    
    //};
    //
    //auto const encoded_data = data | boost::adaptors::transformed(encode_byte);
    //return serverpp::byte_storage{encoded_data.begin(), encoded_data.end()};

    return serverpp::byte_storage{data.begin(), data.end()};
}

}

namespace tbdmud {

class server
{
public:
    server()
      : work_(boost::asio::make_work_guard(io_context_)),
        tcp_server_(
            io_context_, 
            0, 
            [this](auto &&new_socket) 
            { 
                this->new_connection(std::forward<decltype(new_socket)>(new_socket));
            })
    {
        std::cout << "TCP Server started up on port " << tcp_server_.port() << "\n";
    }

    void run()
    {
        io_context_.run();
    }

private:

    std::stringstream command_buffer;

    void new_connection(serverpp::tcp_socket &&new_socket)
    {

        std::cout << "Accepted new socket\n";

        connections_.emplace_back(new connection(std::move(new_socket)));
        auto &connection = connections_.back();

        connection->async_get_terminal_type(
            [](std::string const &ttype)
            {
                std::cout << "Terminal type = " << ttype << "\n";
            });

        connection->on_window_size_changed(
            [](std::uint16_t width, std::uint16_t height)
            {
                std::cout << "Window size is now " << width << "x" << height << "\n";
            });

        schedule_read(*connection);
    }

    void schedule_read(connection &cnx)
    {
        cnx.async_read(
            [this, &cnx](serverpp::bytes data)
            {
                read_handler(cnx, data);
            },
            [this, &cnx]()
            {
                if (cnx.is_alive())
                {
                    schedule_read(cnx);
                }
                else
                {
                    connection_death_handler(cnx);
                }
            });
    }

    void read_handler(connection &cnx, serverpp::bytes data)
    {
        //cnx.write(rot13_encode(data));
        cnx.write(data);

        // Iterate over each character (byte) in the input.
        // Our command delineators are CRs and ;
        for (serverpp::byte b : data) {
            if (b == '\r') {  // CR is our cue to process the line
                const serverpp::byte cr = '\n';

                std::cout << command_buffer.str() << std::endl;
                command_buffer.str(""); // Clear the buffer
                cnx.write(cr);          // Move to the next terminal line
            }
            else if (b == '\0') {
                // Ignore Null characters
            }
            else {
                command_buffer << b;
            }
        }
        //command_buffer << std::endl;
    }

    void connection_death_handler(connection &dead_connection)
    {
        // TODO:  Handle player disconnection
        std::cout << "Connection died\n";

        const auto is_dead_connection = [&dead_connection](auto const &connection)
        {
            return connection.get() == &dead_connection;
        };

        boost::for_each(
            connections_ | boost::adaptors::filtered(is_dead_connection),
            [](auto &connection)
            {
                connection.reset();
            });

        boost::remove_erase_if(
            connections_,
            [](auto const &connection)
            {
                return !connection;
            });
    }

    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<
    boost::asio::io_context::executor_type> work_;

    serverpp::tcp_server tcp_server_;

    std::vector<std::unique_ptr<connection>> connections_;
};

}

int main()
{
    tbdmud::player player;
    tbdmud::server server;
    server.run();
}
