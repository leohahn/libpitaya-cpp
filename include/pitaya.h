#ifndef PITAYA_H
#define PITAYA_H

#include "pitaya/connection.h"

#include <chrono>
#include <functional>

namespace pitaya {

class Client
{
public:
    Client();

    void Connect(const std::string& address);
    void AddEventListener(connection::Listener listener);
    void Request(const std::string& route, std::chrono::seconds timeout, RequestHandler handler);
    void Request(const std::string& route,
                 std::vector<uint8_t> data,
                 std::chrono::seconds timeout,
                 RequestHandler handler);

    // We cannot copy a Pitaya object.
    Client& operator=(const Client&) = delete;
    Client(const Client&) = delete;

private:
    connection::Connection _connection;
};

} // namespace pitaya

#endif // PITAYA_H
