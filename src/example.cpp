#include "logger.h"
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
#include <condition_variable>
#include <mutex>

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;

using std::cerr;
using std::cout;
using std::endl;

class Semaphore
{
public:
    void Notify() 
    {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        ++_count;
        _condition.notify_one();
    }

    void Wait() 
    {
        std::unique_lock<decltype(_mutex)> lock(_mutex);
        while(!_count) // Handle spurious wake-ups.
            _condition.wait(lock);
        --_count;
    }
private:
    std::mutex _mutex;
    std::condition_variable _condition;
    uint64_t _count = 0; // Initialized as locked.
};

int
main()
{
    Semaphore semaphore;

    const char* address =
        "a1d127034f31611e8858512b1bea90da-838011280.us-east-1.elb.amazonaws.com:3251";

    try {
        pitaya::Client client;
        client.Connect(address);
        client.AddEventListener([&semaphore](pitaya::connection::Event ev, const std::string& msg) {
            LOG(Info) << "===> Got event: (" << ev << ") => " << msg;

            if (ev == pitaya::connection::Event::Connected) {
                semaphore.Notify();
            }
        });

        semaphore.Wait();

        client.Request("connector.getsessiondata", [](pitaya::connection::RequestResult result) {
            auto err = boost::get<pitaya::connection::RequestError>(&result);
            if (err) {
                LOG(Error) << "Request failed: code = " << err->code << ", message = " << err->message;
                return;
            }

            auto data = boost::get<pitaya::connection::RequestData>(&result);
            assert(data != nullptr);
            if (data) {
                LOG(Info) << "Received data from server: " << std::string((char*)data->data(), data->size());
            }
        });
    } catch (const pitaya::Exception& exc) {
        LOG(Error) << "Failed: " << exc.what();
        return -1;
    }
}
