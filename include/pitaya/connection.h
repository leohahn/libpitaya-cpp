#ifndef PITAYA_CONNECTION_H
#define PITAYA_CONNECTION_H

#include "pitaya/connection/event.h"
#include "pitaya/connection/packet_framed.h"
#include "pitaya/connection/state.h"
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <string>
#include <thread>

namespace pitaya {
namespace connection {

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

    // We cannot copy a Pitaya object.
    Connection& operator=(const Connection&) = delete;
    Connection(const Connection&) = delete;

private:
    void StartWorkerThread();
    void TcpConnectionDone();
    void SendHandshake();
    void HandshakeFailed(boost::system::error_code ec);
    void HandshakeSuccessful(std::string handshakeResponse);
    void ReceiveHandshakeResponse();
    void SendHandshakeAck(std::string handshakeResponse);

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