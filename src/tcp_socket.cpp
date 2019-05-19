#include "pitaya/tcp_socket.h"

#include <boost/asio.hpp>

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
using error_code = boost::system::error_code;

namespace pitaya {

void
AsioTcpSocket::Connect(EndpointIterator it,
                       std::function<void(const error_code&, EndpointIterator)> onConnectionDone)
{
    asio::async_connect(_rawSocket, it, std::move(onConnectionDone));
}

void
AsioTcpSocket::Shutdown()
{
    try {
        _rawSocket.shutdown(tcp::socket::shutdown_both);
    } catch (const boost::exception& exc) {
        // NOTE(lhahn): Boost throws an error if the socket was already shutdown.
        // We just ignore it and continue execution normally.
        (void)exc;
    }
}

void
AsioTcpSocket::Close()
{
    if (_rawSocket.is_open()) {
        _rawSocket.close();
    }
}

void
AsioTcpSocket::Receive(void* bufferStart,
                       size_t bufferSize,
                       std::function<void(const boost::system::error_code&, size_t)> onComplete)
{
    _rawSocket.async_receive(asio::buffer(bufferStart, bufferSize), std::move(onComplete));
}

void
AsioTcpSocket::Send(const std::vector<uint8_t> buffer,
                    std::function<void(const boost::system::error_code&, size_t)> onComplete)
{
    _rawSocket.async_send(asio::buffer(buffer), std::move(onComplete));
}

} // namespace pitaya
