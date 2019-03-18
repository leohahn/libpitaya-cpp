#ifndef PITAYA_SOCKET_H
#define PITAYA_SOCKET_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <functional>

namespace pitaya {

class TcpSocket
{
public:
    using EndpointIterator = boost::asio::ip::tcp::resolver::iterator;

    virtual ~TcpSocket() = default;

    virtual void Connect(EndpointIterator it,
                         std::function<void(const boost::system::error_code&, EndpointIterator)>
                             onConnectionDone) = 0;
    virtual void Receive(
        void* bufferStart,
        size_t bufferSize,
        std::function<void(const boost::system::error_code&, size_t)> onComplete) = 0;
    virtual void Send(const std::vector<uint8_t> buffer,
                      std::function<void(const boost::system::error_code&, size_t)> onComplete) = 0;
    virtual void Shutdown() = 0;
    virtual void Close() = 0;
};

class AsioTcpSocket : public TcpSocket
{
public:
    AsioTcpSocket(boost::asio::io_context& ioContext)
        : _rawSocket(ioContext)
    {}

    void Connect(EndpointIterator it,
                 std::function<void(const boost::system::error_code&, EndpointIterator)>
                     onConnectionDone) override;

    void Shutdown() override;
    void Close() override;
    void Receive(void* bufferStart,
                 size_t bufferSize,
                 std::function<void(const boost::system::error_code&, size_t)> onComplete) override;
    void Send(const std::vector<uint8_t> buffer,
              std::function<void(const boost::system::error_code&, size_t)> onComplete) override;

private:
    boost::asio::ip::tcp::socket _rawSocket;
};

} // namespace pitaya

#endif // PITAYA_SOCKET_H