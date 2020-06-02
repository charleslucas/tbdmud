#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <optional>
#include <queue>
#include <unordered_set>
#include <events.h>
#include <tbdmud.h>
#include <session.h>
#include <world.h>
#include <tbdmud_server.h>

// Call the tick function of the world approximately once a second (doesn't have to be exact)
void async_tick(const boost::system::error_code& /*e*/, io::steady_timer* t, tbdmud::world* w)
{
    w->tick();

    // Move the expiration time of our timer up by one second and set it again
    t->expires_at(t->expiry() + io::chrono::seconds(1));
    t->async_wait(boost::bind(async_tick, io::placeholders::error, t, w));
}

// Handle events from the queue that precede or are equal to the current time
void async_handle_queue(const boost::system::error_code& /*e*/, io::deadline_timer* t, tbdmud::world* w)
{
    w->process_events();

    // Move the expiration time of our timer up by one millisecond and set it again
    t->expires_from_now(boost::posix_time::milliseconds(1));                              // Run as often as possible
    t->async_wait(boost::bind(async_handle_queue, io::placeholders::error, t, w));
}

int main()
{
    io::io_context io_context;
    io::steady_timer   ticktimer(io_context,  io::chrono::seconds(1));
    io::deadline_timer queuetimer(io_context);
    tbdmud::world world;

    server srv(io_context, 15001, &world);

    // Tasks to be asynchronously run by the server
    srv.async_accept();                                                                           // Asynchronously accept incoming TCP traffic
    ticktimer.async_wait(boost::bind(async_tick, io::placeholders::error, &ticktimer, &world));   // Asynchronously but regularly trigger a tick update
    queuetimer.expires_from_now(boost::posix_time::milliseconds(100));                            // Run as often as possible, it's fine if queue handling longer than this to run
    queuetimer.async_wait(boost::bind(async_handle_queue, io::placeholders::error, &queuetimer, &world));

    io_context.run();  // Invoke the completion handlers

    //world.save_to_disk;   // TODO: The world is a separate top-level object so it can ensure all the data is saved to disk before the program exits

    return 0;
}