#include "pitaya/connection.h"
#include "connection/tcp_packet_stream.h"
#include "logger.h"
#include "pitaya/connection/error.h"
#include "pitaya/exception.h"
#include "utils/gzip.h"
#include "utils/net.h"
#include <assert.h>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/system/error_code.hpp>
#include <iostream>
#include <rapidjson/document.h>

namespace asio = boost::asio;
using error_code = boost::system::error_code;
using system_error = boost::system::system_error;
using tcp = boost::asio::ip::tcp;
using boost::optional;
using std::cerr;
using std::cout;
using std::string;
using ConnectionError = pitaya::connection::ConnectionError;

namespace pitaya {
namespace connection {

Connection::Connection()
    : _state(Inited())
    , _ioContext(std::make_shared<asio::io_context>())
    , _work(std::make_shared<asio::io_context::work>(*_ioContext))
    , _packetStream(new TcpPacketStream(_ioContext, TcpPacketStream::ReadBufferMaxSize{ 2048 }))
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
Connection::PostRequest(const std::string& route, std::vector<uint8_t> data, RequestHandler handler)
{
    LOG(Debug) << "Posting request for route " << route;

    _ioContext->post([this, route, data = std::move(data), handler = std::move(handler)]() {
        // If the state is not connected, it means that the request cannot be sent, therefore
        // just call the handler with an error.
        if (!_state.IsConnected()) {
            // TODO: consider doing like libpitaya, where if the connection is not in the connected
            // state, it will store the request in a queue and retry when the conenction was
            // finished.
            RequestError err;
            err.code = "PIT-RESET";
            err.message = "Cannot send request, connection is not in the connected state";
            handler(std::move(err));
            return;
        }

        assert(std::this_thread::get_id() == _workerThreadId);

        LOG(Debug) << "Sending request for route " << route;

        auto packet = protocol::NewData(std::move(data));
        _packetStream->SendPacket(packet, [handler = std::move(handler)](error_code ec) {
            if (ec) {
                LOG(Error) << "Failed to send request: " << ec.message();
                RequestError err;
                err.code = "PIT-500";
                err.message = ec.message();
                handler(std::move(err));
                return;
            }

            LOG(Debug) << "Request successfully sent!";
        });
    });
}

void
Connection::Start(const std::string& address)
{
    assert(std::this_thread::get_id() != _workerThreadId);

    LOG(Debug) << "Connection started!";

    auto hostAndPort = utils::SplitHostAndPort(address);

    if (!hostAndPort) {
        LOG(Error) << "Invalid address " << address;
        return;
    }

    const auto& host = hostAndPort.value().first;
    const auto& port = hostAndPort.value().second;

    LOG(Debug) << "Will connect to host " << host << " in port " << port;

    try {
        _packetStream->Connect(host, port, [this, host, port](error_code ec) {
            assert(std::this_thread::get_id() == this->_workerThreadId);

            if (ec) {
                HandshakeFailed(ec);
                return;
            }

            LOG(Debug) << "Connected to endpoint: " << host << ":" << port;
            TcpConnectionDone();
        });
    } catch (const system_error& exc) {
        throw Exception("Error starting the connection: " + string(exc.what()));
    }
}

void
Connection::TcpConnectionDone()
{
    assert(std::this_thread::get_id() == _workerThreadId);
    // The client is connected to the server, now start the
    // pitaya protocol (handshake).
    SendHandshake();
}

void
Connection::SendHandshake()
{
    assert(std::this_thread::get_id() == _workerThreadId);
    LOG(Debug) << "Sending handshake";

    // TODO: consider making a packet just a simple byte arreay instead
    // of a struct. This will avoid unnecessary copies like the one below.
    auto handshakePacket = protocol::NewHandshake(
        "{\"sys\": {\"platform\": \"mac\", \"libVersion\": \"0.3.5-release\", "
        "\"clientBuildNumber\": \"20\", \"clientVersion\": \"2.1\"}, \"user\": {}}");
    std::vector<uint8_t> handshakeBuf;
    handshakePacket.SerializeInto(handshakeBuf);

    _packetStream->SendPacket(std::move(handshakePacket), [this](error_code ec) {
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

    LOG(Debug) << "Will receive handshake response";

    _packetStream->ReceivePackets([this](error_code ec, std::vector<protocol::Packet> packets) {
        if (ec) {
            HandshakeFailed(ec);
            return;
        }

        assert(packets.size() > 0 &&
               "If no error was returned, packets should be at least of size 1");

        LOG(Debug) << "Received response from server";

        if (packets.size() > 1) {
            // TODO: consider calling a reconnect function here.
            LOG(Error)
                << "Received more than one packet from the server in the handshake response, "
                   "closing connection";
            HandshakeFailed(ConnectionError::TooManyPackets);
            return;
        }

        const auto& packet = packets[0];

        if (packet.type != protocol::PacketType::Handshake) {
            HandshakeFailed(ConnectionError::WrongPacket);
            return;
        }

        std::string handshakeResponse;

        if (utils::IsCompressed(packet.body.data(), packet.body.size())) {
            LOG(Info) << "Handshake response is compressed";

            uint8_t* output = nullptr;
            size_t size = 0;

            int rc = utils::Decompress(
                &output, &size, const_cast<uint8_t*>(packet.body.data()), packet.body.size());

            if (rc) {
                LOG(Error) << "Failed to decompress handshake response, closing connection";
                ConnectionError(ConnectionError::InvalidHandshakeCompression);
                return;
            }

            handshakeResponse = string((char*)output, size);
            free(output);
        } else {
            handshakeResponse = string((char*)packet.body.data(), packet.body.size());
        }

        SendHandshakeAck(std::move(handshakeResponse));
    });
}

void
Connection::SendHandshakeAck(string handshakeResponse)
{
    rapidjson::Document doc;
    doc.Parse(handshakeResponse.c_str());

    if (doc.HasParseError()) {
        ConnectionError(ConnectionError::InvalidHeartbeatJson);
        return;
    }

    LOG(Info) << handshakeResponse;

    if (!doc.HasMember("sys")) {
        ConnectionError(ConnectionError::InvalidHeartbeatJson);
        return;
    }

    if (!doc["sys"].HasMember("heartbeat")) {
        ConnectionError(ConnectionError::InvalidHeartbeatJson);
        return;
    }

    if (!doc["sys"]["heartbeat"].IsUint64()) {
        ConnectionError(ConnectionError::InvalidHeartbeatJson);
        return;
    }

    std::unordered_map<std::string, int> routeDict;

    if (doc["sys"].HasMember("dict") && doc["sys"]["dict"].IsObject()) {
        for (const auto& m : doc["sys"]["dict"].GetObject()) {
            if (m.name.IsString() && m.value.IsInt()) {
                routeDict.insert(std::make_pair(m.name.GetString(), m.value.GetInt()));
            }
        }
    }

    if (!routeDict.empty()) {
        LOG(Debug) << "Route dict:";
        for (const auto& r : routeDict) {
            LOG(Debug) << "    " << r.first << " -> " << r.second;
        }
    }

    auto heartbeatInterval = std::chrono::seconds(doc["sys"]["heartbeat"].GetUint64());

    _packetStream->SendPacket(protocol::NewHandshakeAck(), [this, heartbeatInterval, routeDict = std::move(routeDict)](error_code ec) {
        if (ec) {
            LOG(Error) << "Failed to send hanshake ack";
            return;
        }

        LOG(Debug) << "Sent handshake ack successfuly";

        HandshakeSuccessful(heartbeatInterval, routeDict);
    });
}

void
Connection::HandshakeSuccessful(std::chrono::seconds heartbeatInterval, std::unordered_map<std::string, int> routeDict)
{
    LOG(Debug) << "Heartbeat Interval is " << heartbeatInterval.count() << " seconds";

    // Set connection state to connected and broadcast success event
    {
        std::lock_guard<decltype(_state)> lock(_state);
        _state.SetConnected(*this->_ioContext,
                            std::move(heartbeatInterval),
                            std::move(routeDict),
                            [this]() {
                                _packetStream->SendPacket(protocol::NewHeartbeat(),
                                                          [this](error_code ec) {
                                                              if (ec) {
                                                                  ConnectionError(ec);
                                                              }
                                                          });
                            },
                            [this]() {
                                LOG(Info) << "Heartbeat timed out, closing connection";
                                ConnectionError(ConnectionError::HeartbeatTimeout);
                            });
        _eventListeners.Broadcast(Event::Connected, "Connection successful");
    }

    ReceivePackets();
}

void
Connection::HandshakeFailed(error_code ec)
{
    LOG(Error) << "Failed to send handshake packet: " << ec.message();

    std::lock_guard<State> lock(_state);
    _state.SetInited();
    _eventListeners.Broadcast(Event::ConnectionFailed, ec.message());

    // TODO: consider calling a reconnect function here.
}

void
Connection::ConnectionError(error_code ec)
{
    std::lock_guard<State> lock(_state);
    if (_state.IsConnected()) {
        LOG(Error) << "Error in connection: " << ec.message();
        _state.SetInited();
        _eventListeners.Broadcast(Event::ConnectionError, ec.message());
        _packetStream->Disconnect();
    }
}

void
Connection::KickedFromServer()
{
    std::lock_guard<State> lock(_state);
    if (_state.IsConnected()) {
        _state.SetInited();
        _eventListeners.Broadcast(Event::Kicked, "Kicked from server");
        _packetStream->Disconnect();
    }
}

void
Connection::ReceivePackets()
{
    _packetStream->ReceivePackets([this](error_code ec, std::vector<protocol::Packet> packets) {
        if (ec) {
            ConnectionError(ec);
            return;
        }

        for (const auto& p : packets) {
            ProcessPacket(p);
        }

        ReceivePackets();
    });
}

void
Connection::ProcessPacket(const protocol::Packet& packet)
{
    LOG(Debug) << "Processing packet " << packet.type;

    switch (packet.type) {
        case protocol::PacketType::Heartbeat:
            // Expected
            _state.ExtendHeartbeatTimeout();
            break;
        case protocol::PacketType::Data:
            break;
        case protocol::PacketType::Kick:
            // Expected
            KickedFromServer();
            break;
        default:
            assert(false && "Should not receive packets other than heartbeat, data and kick");
            ConnectionError(ConnectionError::WrongPacket);
            break;
    }
}

void
Connection::StartWorkerThread()
{
    _workerThreadId = std::this_thread::get_id();
    LOG(Info) << "Running worker thread";
    _ioContext->run();
    LOG(Info) << "No more work to do, exiting thread";
}

} // namespace connection
} // namespace pitaya
