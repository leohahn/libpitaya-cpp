#include "pitaya.h"
#include "pitaya/exception.h"
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;

using std::cerr;
using std::cout;
using std::endl;

int
main()
{
    try {
        pitaya::Client client;
        client.Connect("127.0.0.1:4100");
        client.AddEventListener([](pitaya::connection::Event ev, const std::string& msg) {
            cout << "===> Got event: (" << ev << ") => " << msg << "\n";
        });
    } catch (const pitaya::Exception& exc) {
        cerr << "Failed: " << exc.what() << "\n";
        return -1;
    }
}
