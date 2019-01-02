#ifndef PITAYA_CONNECTION_H
#define PITAYA_CONNECTION_H

#include "pitaya/connection/event.h"
#include "pitaya/connection/packet_framed.h"
#include "pitaya/connection/state.h"
#include "pitaya/protocol/packet.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/variant.hpp>
#include <memory>
#include <chrono>
#include <string>
#include <thread>
#include <functional>

namespace pitaya {
namespace connection {

struct RequestError
{
    std::string code;
    std::string message;
    std::string metadata;
};

using RequestData = std::vector<uint8_t>;
using RequestResult = boost::variant<RequestData, RequestError>;
using RequestHandler = std::function<void(RequestResult)>;

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
    void TcpConnectionDone();
    void SendHandshake();
    void ConnectionError(boost::system::error_code ec);
    void HandshakeFailed(boost::system::error_code ec);
    void HandshakeSuccessful(std::chrono::seconds heartbeatInterval);
    void ReceiveHandshakeResponse();
    void SendHandshakeAck(std::string handshakeResponse);
    void ReceivePackets();
    void ProcessPacket(const protocol::Packet& packet);
    void KickedFromServer();

private:
    // The current state of the connection.
    State _state;

    // Boost::asio context and work object, to signal that the main
    // loop should not exit.
    std::shared_ptr<boost::asio::io_context> _ioContext;
    std::shared_ptr<boost::asio::io_context::work> _work;

    // Interface that allows sending and receiving packets.
    std::unique_ptr<PacketFramed> _packetFramed;

    // Thread that will execute all of the io tasks.
    std::thread _workerThread;
    std::thread::id _workerThreadId;

    // Listeners that will be notified when events happen in the
    // connection.
    EventListeners _eventListeners;
};

} // namespace connection
} // namespace pitaya

#endif // PITAYA_CONNECTION_H