#include "pitaya/connection.h"

#include "pitaya/connection/error.h"
#include "pitaya/exception.h"

#include "connection/tcp_packet_stream.h"
#include "logger.h"
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
    : _reqId(1)
    , _state(Inited())
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
Connection::PostRequest(const std::string& route,
                        std::vector<uint8_t> data,
                        std::chrono::seconds timeout,
                        RequestHandler handler)
{
    using namespace pitaya::protocol;

    _ioContext->post(
        [this, route, timeout, data = std::move(data), handler = std::move(handler)]() {
            // If the state is not connected, it means that the request cannot be sent, therefore
            // just call the handler with an error.
            if (!_state.IsConnected()) {
                // TODO: consider doing like libpitaya, where if the connection is not in the
                // connected state, it will store the request in a queue and retry when the
                // conenction was finished.
                LOG(Warn) << "Cannot post request, since the connection is not established yet";
                handler(RequestStatus::NotConnectedError, RequestData());
                return;
            }

            auto* ptr = boost::get<Connected>(&_state.Val());
            assert(ptr);

            assert(std::this_thread::get_id() == _workerThreadId);

            LOG(Debug) << "Sending request for route " << route;

            // TODO: is this the best way to handle request id's?
            auto msg = Message::NewRequest(_reqId++, route, std::move(data));
            auto packet = NewData(msg, ptr->routeToCode);

            _packetStream->SendPacket(
                packet,
                [this, timeout, msg = std::move(msg), handler = std::move(handler)](error_code ec) {
                    if (ec) {
                        LOG(Error) << "Failed to send request: " << ec.message();
                        handler(RequestStatus::InternalError, RequestData());
                        return;
                    }

                    LOG(Debug) << "Request successfully sent!";

                    assert(_inFlightRequests.find(msg.id) == _inFlightRequests.end() &&
                           "The request id should be unique");

                    _inFlightRequests.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(msg.id),
                        std::forward_as_tuple(*_ioContext,
                                              std::move(handler),
                                              timeout,
                                              [this, id = msg.id](error_code ec) {
                                                  if (ec) {
                                                      LOG(Warn) << "Request timeout timer failed: "
                                                                << ec.message();
                                                      return;
                                                  }

                                                  LOG(Warn) << "Request id " << id << " timed out";

                                                  ProcessRequestTimeout(id);
                                              }));

                    // Now, start the request timeout
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
            SendHandshake();
        });
    } catch (const system_error& exc) {
        throw Exception("Error starting the connection: " + string(exc.what()));
    }
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

    std::string serializer;

    if (!doc["sys"].HasMember("serializer") || !doc["sys"]["serializer"].IsString()) {
        LOG(Warn) << "Did not receive serializer from server, assuming JSON";
        serializer = "json";
    } else {
        serializer = doc["sys"]["serializer"].GetString();
        LOG(Info) << "Using serializer: " << serializer;
    }

    std::unordered_map<std::string, int> routeDict;

    if (doc["sys"].HasMember("dict") && doc["sys"]["dict"].IsObject()) {
        for (const auto& m : doc["sys"]["dict"].GetObject()) {
            if (m.name.IsString() && m.value.IsInt()) {
                routeDict.insert(std::make_pair(m.name.GetString(), m.value.GetInt()));
            }
        }
    }

    // NOTE: Print the route dict for debugging purposes. Should this be kept for
    // production code?
    if (!routeDict.empty()) {
        LOG(Debug) << "Route dict:";
        for (const auto& r : routeDict) {
            LOG(Debug) << "    " << r.first << " -> " << r.second;
        }
    }

    auto heartbeatInterval = std::chrono::seconds(doc["sys"]["heartbeat"].GetUint64());

    _packetStream->SendPacket(
        protocol::NewHandshakeAck(),
        [this, heartbeatInterval, serializer, routeDict = std::move(routeDict)](error_code ec) {
            if (ec) {
                LOG(Error) << "Failed to send hanshake ack";
                return;
            }

            LOG(Debug) << "Sent handshake ack successfuly, pitaya connected!";

            // The pitaya connection was finished! Now start receiving packets from the server.
            StartReceivingPackets(heartbeatInterval, std::move(serializer), std::move(routeDict));
        });
}

void
Connection::StartReceivingPackets(std::chrono::seconds heartbeatInterval,
                                  std::string serializer,
                                  std::unordered_map<std::string, int> routeDict)
{
    LOG(Debug) << "Heartbeat Interval is " << heartbeatInterval.count() << " seconds";

    // Set connection state to connected and broadcast success event
    {
        std::lock_guard<decltype(_state)> lock(_state);
        _state.SetConnected(*this->_ioContext,
                            std::move(heartbeatInterval),
                            std::move(serializer),
                            std::move(routeDict),
                            [this]() {
                                LOG(Debug) << "New tick, sending heartbeat!";
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

        LOG(Debug) << "Received " << packets.size() << " packets from the server";

        // TODO: remove this assertion. First we need to figure out if receiving more than one
        // packet is even possible.
        assert(packets.size() == 1);

        for (const auto& p : packets) {
            ProcessPacket(p);
        }

        ReceivePackets();
    });
}

void
Connection::ProcessPacket(const protocol::Packet& packet)
{
    switch (packet.type) {
        case protocol::PacketType::Heartbeat:
            LOG(Debug) << "Received heartbeat from server";
            _state.ExtendHeartbeatTimeout();
            break;
        case protocol::PacketType::Data: {
            LOG(Debug) << "Received data packet from server";

            if (!_state.IsConnected()) {
                ConnectionError(ConnectionError::WrongPacket);
                return;
            }

            auto connectedState = boost::get<Connected>(&_state.Val());
            assert(connectedState);

            auto maybeMsg =
                protocol::Message::Deserialize(packet.body, connectedState->codeToRoute);

            if (!maybeMsg) {
                LOG(Error) << "Received an invalid message from the server";
                ConnectionError(ConnectionError::InvalidMessage);
                return;
            }

            const auto& msg = maybeMsg.value();

            ProcessMessage(std::move(msg));
            break;
        }
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
Connection::ProcessMessage(const protocol::Message& msg)
{
    using namespace pitaya::protocol;

    switch (msg.type) {
        case MessageType::Response: {
            // We got a response from a request that was sent earlier,
            // therefore we need to get the request id and match it against the callback
            // passed to the client.

            LOG(Info) << "Processing response message...";

            if (_inFlightRequests.find(msg.id) == _inFlightRequests.end()) {
                LOG(Error) << "Received a response from the server from a request id " << msg.id
                           << " that was not sent, ignoring it";
                return;
            }

            const auto& request = _inFlightRequests.at(msg.id);

            if (msg.error) {
                request.handler(RequestStatus::ServerError, std::move(msg.data));
            } else {
                request.handler(RequestStatus::Ok, std::move(msg.data));
            }

            _inFlightRequests.erase(msg.id);

            break;
        }
        case MessageType::Push:
            break;
        case MessageType::Request:
            // The client should not receive request messages, just log it and ignore it.
            LOG(Warn) << "Received a request message from the server, ignoring...";
            break;
        case MessageType::Notify:
            // The client should not receive notify messages, just log it and ignore it.
            LOG(Warn) << "Received a notify message from the server, ignoring...";
            break;
    }
}

void
Connection::ProcessRequestTimeout(uint64_t id)
{
    if (_inFlightRequests.find(id) == _inFlightRequests.end()) {
        LOG(Warn) << "Tried to process timeout of request id " << id
                  << ", however the same is not in flight";
        return;
    }

    const auto& req = _inFlightRequests.at(id);
    req.handler(RequestStatus::Timeout, RequestData());
    _inFlightRequests.erase(id);
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
