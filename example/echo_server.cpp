//
//  echo_server.cpp
//  Boost.GreenThread
//

#include <iostream>
#include <signal.h>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <boost/green_thread.hpp>
#include <boost/green_thread/greenify.hpp>

using namespace boost::green_thread;
std::string address("0::0");

// We don't need atomic or any other kind of synchronization as console and
// main_watchdog threads are always running in the same native thread with main_thread
bool should_exit = false;

// Active connection counter
// Need to be atomic as this counter will be used across multiple threads
boost::atomic<size_t> connections(0);

typedef boost::asio::basic_waitable_timer<boost::chrono::steady_clock> watchdog_timer_t;

void hello(std::ostream& s)
{
    s << "Boost.GreenThread echo_server listening at " << address << std::endl;
}

void help_message()
{
    std::cout << "Available commands:" << std::endl;
    std::cout << "quit:\tquit the application" << std::endl;
    std::cout << "info:\tshow the number of active connections" << std::endl;
    std::cout << "help:\tshow this message" << std::endl;
}

/**
 * console handler, set the exit flag on "quit" command
 */
void console()
{
    // Hello message
    hello(std::cout);

    // Help message
    help_message();

    // Flush `std::cout` before getting input
    std::cin.tie(&std::cout);

    // Read a line from `std::cin` and process commands
    while (!should_exit && std::cin) {
        std::string line;
        // Command line prompt
        std::cout << "> ";
        // `std::cout` is automatically flushed before reading as it's tied to std::cout
        std::getline(std::cin, line);
        // Ignore empty line
        if (line.empty()) continue;
        if (line == "quit") {
            // Set the exit flag on `quit` command
            break;
        } else if (line == "info") {
            // Output the number of active connections
            std::cout << "Active connections: " << connections << std::endl;
        } else if (line == "help") {
            // Show help message
            help_message();
        } else {
            std::cout << "Invalid command" << std::endl;
            help_message();
        }
    }

    // Set the exit flag
    should_exit = true;
}

/**
 * watchdog for servant, close connection on timeout
 */
void servant_watchdog(watchdog_timer_t& timer, boost::green_thread::tcp_stream& s)
{
    boost::system::error_code ignore_ec;
    while (s.is_open()) {
        timer.async_wait(asio::yield[ignore_ec]);
        // close the stream on timeout
        if (timer.expires_from_now() <= boost::chrono::seconds(0)) {
            s.close();
        }
    }
}

/**
 * connection handler thread
 */
void echo_servant(boost::green_thread::tcp_stream& s)
{
    /**
     * Active connection counter
     */
    struct conn_counter {
        conn_counter() { connections += 1; }
        ~conn_counter() { connections -= 1; }
    } counter;

    // Timeout timer
    watchdog_timer_t timer(asio::get_io_service());

    // Set connection timeout
    timer.expires_from_now(boost::chrono::seconds(3));

    // Start watchdog thread, close connection on timeout
    // ASIO sockets are *not* thread-safe, watchdog must not run in
    // different thread with handler
    thread watchdog(thread::attributes(thread::attributes::stick_with_parent),
                   servant_watchdog,
                   std::ref(timer),
                   std::ref(s));
    // Hello message
    hello(s);

    // Read a line and send it back
    std::string line;
    while (std::getline(s, line)) {
        s << line << std::endl;
        // Reset timeout timer on input
        timer.expires_from_now(boost::chrono::seconds(3));
    }

    // Ask watchdog to exit
    timer.expires_from_now(boost::chrono::seconds(0));
    timer.cancel();

    // Make sure watchdog has ended
    watchdog.join();
}

/**
 * Watchdog for main thread, check the exit flag and signals once every second,
 * close main listener when flag is set or received signal
 */
void signal_watchdog(tcp_listener& l)
{
    boost::asio::signal_set ss(asio::get_io_service(), SIGINT, SIGTERM);
#ifdef SIGQUIT
    ss.add(SIGQUIT);
#endif

    // Get a future, the future is ready if there is a signal
    future<int> sig = ss.async_wait(asio::use_future);

    while (!should_exit && sig.wait_for(boost::chrono::seconds(1)) != future_status::ready) {
    }

    // Set the exit flag
    should_exit = true;

    // Stop main listener
    l.stop();
}

/**
 * main thread, create acceptor to accept incoming connections,
 * starts a servant thread to handle connection
 */
int boost::green_thread::main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage:"
                  << "\t" << argv[0] << " [address:]port" << std::endl;
        return 1;
    }
    address = argv[1];

    // Start more work threads
    get_scheduler().add_worker_thread(3);

    // Start console
    thread(console).detach();

    // Listener
    tcp_listener l(address);

    // Start watchdog
    thread(signal_watchdog, std::ref(l)).detach();

    // Start listener
    int r = l(echo_servant).value();

    std::cout << "Echo server existing..." << std::endl;
    return r;
}
