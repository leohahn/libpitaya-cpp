#ifndef PITAYA_CONNECTION_H
#define PITAYA_CONNECTION_H

#include "pitaya/connection/event.h"
#include "pitaya/connection/packet_stream.h"
#include "pitaya/connection/state.h"
#include "pitaya/protocol/message.h"
#include "pitaya/protocol/packet.h"
#include "pitaya/protocol/request.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/variant.hpp>
#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace pitaya {
namespace connection {

using RequestHandler = std::function<void(protocol::RequestStatus, protocol::RequestData)>;

class Connection
{
public:
    Connection();
    ~Connection();

    void Start(const std::string& address);
    void AddEventListener(Listener listener)
    {
        std::lock_guard<EventListeners> lock(_eventListeners);
        _eventListeners.Add(std::move(listener));
    }

    void PostRequest(const std::string& route, std::vector<uint8_t> data, RequestHandler handler);
    // We cannot copy a Connection object.
    Connection& operator=(const Connection&) = delete;
    Connection(const Connection&) = delete;

private:
    void StartWorkerThread();
    void SendHandshake();
    void ConnectionError(boost::system::error_code ec);
    void HandshakeFailed(boost::system::error_code ec);
    void StartReceivingPackets(std::chrono::seconds heartbeatInterval,
                               std::string serializer,
                               std::unordered_map<std::string, int> routeToCode);
    void ReceiveHandshakeResponse();
    void SendHandshakeAck(std::string handshakeResponse);
    void ReceivePackets();
    void ProcessPacket(const protocol::Packet& packet);
    void ProcessMessage(const protocol::Message& msg);
    void KickedFromServer();

private:
    size_t _reqId;

    // The current state of the connection.
    State _state;

    // Boost::asio context and work object, to signal that the main
    // loop should not exit.
    std::shared_ptr<boost::asio::io_context> _ioContext;
    std::shared_ptr<boost::asio::io_context::work> _work;

    // Interface that allows sending and receiving packets.
    std::unique_ptr<PacketStream> _packetStream;

    // Thread that will execute all of the io tasks.
    std::thread _workerThread;
    std::thread::id _workerThreadId;

    // Listeners that will be notified when events happen in the
    // connection.
    EventListeners _eventListeners;

    // This map keeps track of all of the requests that were sent to the
    // server but did not yet received a response from it.
    std::unordered_map<uint64_t, RequestHandler> _inFlightRequests;
};

} // namespace connection
} // namespace pitaya

#endif // PITAYA_CONNECTION_H