#include "pitaya.h"
#include "pitaya/exception.h"

#include "logger.h"
#include <array>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;
using namespace pitaya;

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
        while (!_count) // Handle spurious wake-ups.
            _condition.wait(lock);
        --_count;
    }

private:
    std::mutex _mutex;
    std::condition_variable _condition;
    uint64_t _count = 0;
};

//
// TODO:
//   - Implement notify
//   - Setup unit testing
//   - Implement interface for boost sockets,
//     in order to mock the SocketStream and do unit testing.
//     (this can also be done without an interface)
//

int
main()
{
    using std::chrono::seconds;

    Semaphore semaphore;

    // const char* address = "localhost:4100";
    // const char* address = "localhost:3251";
    // const char* address = "localhost:4300";
    const char* address =
        "a1d127034f31611e8858512b1bea90da-838011280.us-east-1.elb.amazonaws.com:3251";

    try {
        pitaya::Client client;
        client.AddEventListener([&semaphore](connection::Event ev, const std::string& msg) {
            LOG(Info) << "===> Got event: (" << ev << ") => " << msg;

            if (ev == pitaya::connection::Event::Connected) {
                semaphore.Notify();
            }
        });
        client.Connect(address);

        semaphore.Wait();

        client.Request(
            "connector.getsessiondata", seconds(5), [](RequestStatus status, RequestData data) {
                if (status != RequestStatus::Ok) {
                    LOG(Error) << "Request failed: status = " << status;
                    return;
                }

                LOG(Info) << "Received data from server: "
                          << std::string((char*)data.data(), data.size());
            });
    } catch (const pitaya::Exception& exc) {
        LOG(Error) << "Failed: " << exc.what();
        return -1;
    }
}
