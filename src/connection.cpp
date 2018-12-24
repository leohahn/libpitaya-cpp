#include "pitaya/connection.h"
#include "connection/tcp_packet_framed.h"
#include "pitaya/exception.h"
#include "pitaya/protocol/packet.h"
#include "utils/net.h"
#include <assert.h>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>

namespace asio = boost::asio;
using error_code = boost::system::error_code;
using system_error = boost::system::system_error;
using tcp = boost::asio::ip::tcp;
using boost::optional;
using std::cerr;
using std::cout;
using std::string;

namespace pitaya {
namespace connection {

Connection::Connection()
    : _state(Inited())
    , _ioContext(std::make_shared<asio::io_context>())
    , _work(std::make_shared<asio::io_context::work>(*_ioContext))
    , _packetFramed(new TcpPacketFramed(_ioContext, TcpPacketFramed::ReadBufferMaxSize{ 2048 }))
    , _workerThread(&Connection::StartWorkerThread, this)
{}

Connection::~Connection()
{
    // Destroy the worker object, telling the worker thread,
    // that after the jobs are done it should exit.
    _work.reset();

    // TODO: if jobs are taking long to finish, consider forcefully
    // shutting the thread down by using _ioContext.cancel().

    if (_workerThread.joinable()) {
        _workerThread.join();
    }
}

void
Connection::Start(const std::string& address)
{
    assert(std::this_thread::get_id() != _workerThreadId);

    cout << "Connection started!\n";

    auto hostAndPort = utils::SplitHostAndPort(address);

    if (!hostAndPort) {
        cerr << "Invalid address " << address << "\n";
        return;
    }

    const auto& host = hostAndPort.value().first;
    const auto& port = hostAndPort.value().second;

    cout << "Will connect to host " << host << " in port " << port << "\n";

    try {
        _packetFramed->Connect(host, port, [this, host, port](error_code ec) {
            assert(std::this_thread::get_id() == this->_workerThreadId);

            if (ec) {
                this->TcpConnectionFailed();
                return;
            }

            cout << "Connected to endpoint: " << host << ":" << port << "\n";
            this->TcpConnectionDone();
        });
    } catch (const system_error& exc) {
        throw Exception("Error starting the connection: " + string(exc.what()));
    }
}

void
Connection::TcpConnectionDone()
{
    assert(std::this_thread::get_id() == _workerThreadId);
    _eventListeners.Broadcast(Event::Connected, "Client successfully connected to server");

    // The client is connected to the server, now start the
    // pitaya protocol (handshake).
    SendHandshake();
}

void
Connection::TcpConnectionFailed()
{
    assert(std::this_thread::get_id() == _workerThreadId);
    _eventListeners.Broadcast(Event::ConnectionFailed, "Error making initial TCP connection");
    cerr << "Error making connection\n";
    // TODO: consider calling a reconnect function here.
}

void
Connection::SendHandshake()
{
    assert(std::this_thread::get_id() == _workerThreadId);
    cout << "Sending handshake\n";

    // TODO: consider making a packet just a simple byte arreay instead
    // of a struct. This will avoid unnecessary copies like the one below.
    auto handshakePacket = protocol::NewHandshake(
        "{\"sys\": {\"platform\": \"mac\", \"libVersion\": \"0.3.5-release\", "
        "\"clientBuildNumber\": \"20\", \"clientVersion\": \"2.1\"}, \"user\": {}}");
    std::vector<uint8_t> handshakeBuf;
    handshakePacket.SerializeInto(handshakeBuf);

    _packetFramed->SendPacket(std::move(handshakePacket), [this](error_code ec) {
        if (ec) {
            HandshakeFailed(ec);
            return;
        }

        ReceiveHandshakeResponse();
    });
}

void
Connection::ReceiveHandshakeResponse()
{
    assert(std::this_thread::get_id() == _workerThreadId);

    std::cout << "Will receive handshake response\n";

    _packetFramed->ReceivePackets([this](error_code ec, std::vector<protocol::Packet> packets) {
        if (ec) {
            this->_eventListeners.Broadcast(Event::ConnectionFailed, ec.message());
            return;
        }

        assert(packets.size() > 0 &&
               "If no error was returned, packets should be at least of size 1");

        std::cout << "Received response from server\n";

        if (packets.size() > 1) {
            // TODO: consider calling a reconnect function here.
            std::cerr << "Received more than one packet from the server in the handshake response, "
                         "closing connection\n";

            this->_eventListeners.Broadcast(
                Event::ConnectionFailed,
                "Received more than one packet from the server in the handshake response");

            return;
        }

        const auto& packet = packets[0];

        if (packet.type != protocol::PacketType::Handshake) {
            this->_eventListeners.Broadcast(
                Event::ConnectionFailed,
                "Did not get a hanshake response from the server, closing connection\n");
            return;
        }

        // TODO: parse handshake response into a JSON object and store it in the connection state.
        std::cout << "Response is: " << string((char*)packet.body.data(), packet.body.size())
                  << "\n";

        // TODO: Send handshake ack
        SendHandshakeAck();
    });
}

void
Connection::SendHandshakeAck()
{
    _packetFramed->SendPacket(protocol::NewHandshakeAck(), [](error_code ec) {
        if (ec) {
            std::cerr << "Failed to send hanshake ack\n";
            return;
        }

        std::cout << "Sent handshake ack successfuly\n";
        // TODO: Start heartbeat timer.
    });
}

void
Connection::HandshakeFailed(error_code ec)
{
    cerr << "Failed to send handshake packet: " << ec.message() << "\n";

    std::lock_guard<State> lock(_state);
    _state.SetConnectionFailed(ec, ec.message());
    _eventListeners.Broadcast(Event::ConnectionFailed, ec.message());

    // TODO: consider calling a reconnect function here.
}

void
Connection::StartWorkerThread()
{
    _workerThreadId = std::this_thread::get_id();
    std::cout << "Running worker thread\n";
    _ioContext->run();
    std::cout << "No more work to do, exiting thread\n";
}

} // namespace connection
} // namespace pitaya
