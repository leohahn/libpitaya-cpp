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

} // namespace pitaya