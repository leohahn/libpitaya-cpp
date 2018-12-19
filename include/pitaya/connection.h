#ifndef PITAYA_CONNECTION_H
#define PITAYA_CONNECTION_H

#include "pitaya/connection/event.h"
#include "pitaya/connection/packet_framed.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/variant.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pitaya {
namespace connection {

struct Inited
{};

struct ConnectionStarted
{};

struct HandshakeStarted
{};

struct Connected
{};

struct ConnectionFailed
{
    boost::system::error_code ec;
    std::string reason;
};

class State
{
    using StateType =
        boost::variant<Inited, ConnectionStarted, HandshakeStarted, Connected, ConnectionFailed>;

public:
    void lock() { _mutex.lock(); }
    void unlock() { _mutex.unlock(); }

    StateType& Val() { return _val; }
    const StateType& Val() const { return _val; }

    void SetConnectionFailed(boost::system::error_code ec, std::string reason)
    {
        ConnectionFailed s;
        s.ec = ec;
        s.reason = std::move(reason);
        _val = std::move(s);
    }

    State(StateType val)
        : _val(val)
    {}

private:
    std::mutex _mutex;
    StateType _val;
};

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
    void TcpConnectionFailed();
    void SendHandshake();
    void HandshakeFailed(boost::system::error_code ec);
    void ReceiveHandshakeResponse();

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