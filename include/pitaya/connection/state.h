#ifndef PITAYA_CONNECTION_STATE_H
#define PITAYA_CONNECTION_STATE_H

#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/variant.hpp>
#include <functional>
#include <mutex>
#include <string>

namespace pitaya {
namespace connection {

struct Inited
{};

struct ConnectionStarted
{};

struct HandshakeStarted
{};

struct Connected
{
    std::string handshakeResponse;
    boost::asio::steady_timer heartbeatTimer;
    std::function<void()> heartbeatTick;

    Connected(boost::asio::io_context& ioContext,
              std::string handshakeResponse,
              std::function<void()> heartbeatTick)
        : handshakeResponse(std::move(handshakeResponse))
        , heartbeatTimer(ioContext)
        , heartbeatTick(std::move(heartbeatTick))
    {}
};

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

    void SetConnectionFailed(boost::system::error_code ec, std::string reason);
    void SetConnected(boost::asio::io_context& ioContext,
                      std::string handshakeResponse,
                      std::function<void()> heartbeatTick);

    template<typename A, typename B, typename C>
    void SetConnectedWithLock(A&& a, B&& b, C&& c)
    {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        SetConnected(std::forward<A>(a), std::forward<B>(b), std::forward<C>(c));
    }

    State(StateType val)
        : _val(std::move(val))
    {}

private:
    void HeartbeatTick(boost::system::error_code ec);

private:
    std::mutex _mutex;
    StateType _val;
};

} // namespace connection
} // namespace pitaya

#endif // PITAYA_CONNECTION_STATE_H