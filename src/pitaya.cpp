#include "pitaya.h"

#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace pitaya {

Client::Client() {}

void
Client::Connect(const std::string& address)
{
    _connection.Start(address);
}

void
Client::AddEventListener(connection::Listener listener)
{
    _connection.AddEventListener(std::move(listener));
}

void
Client::Request(const std::string& route, std::chrono::seconds timeout, RequestHandler handler)
{
    _connection.PostRequest(route, std::vector<uint8_t>(), timeout, std::move(handler));
}

void
Client::Request(const std::string& route,
                std::vector<uint8_t> data,
                std::chrono::seconds timeout,
                RequestHandler handler)
{
    _connection.PostRequest(route, std::move(data), timeout, std::move(handler));
}

} // namespace pitaya