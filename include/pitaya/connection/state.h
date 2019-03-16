#ifndef PITAYA_CONNECTION_STATE_H
#define PITAYA_CONNECTION_STATE_H

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/system_timer.hpp>
#include <boost/system/error_code.hpp>
#include <boost/variant.hpp>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

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
    std::chrono::seconds heartbeatInterval;
    std::chrono::seconds heartbeatTimeoutSeconds;

    std::unordered_map<std::string, int> routeDict;
    boost::asio::steady_timer heartbeatTimer;
    boost::asio::system_timer heartbeatTimeout;
    std::function<void()> heartbeatTick;

    Connected(boost::asio::io_context& ioContext,
              std::chrono::seconds heartbeatInterval,
              std::chrono::seconds heartbeatTimeoutSeconds,
              std::unordered_map<std::string, int> routeDict,
              std::function<void()> heartbeatTick)
        : heartbeatInterval(heartbeatInterval)
        , heartbeatTimeoutSeconds(heartbeatTimeoutSeconds)
        , routeDict(std::move(routeDict))
        , heartbeatTimer(ioContext)
        , heartbeatTimeout(ioContext)
        , heartbeatTick(std::move(heartbeatTick))
    {}
};

class State
{
    using StateType = boost::variant<Inited, Connected>;

public:
    void lock() { _mutex.lock(); }
    void unlock() { _mutex.unlock(); }

    StateType& Val() { return _val; }
    const StateType& Val() const { return _val; }

    void SetInited();
    void SetConnected(boost::asio::io_context& ioContext,
                      std::chrono::seconds heartbeatInterval,
                      std::unordered_map<std::string, int> routeDict,
                      std::function<void()> heartbeatTick,
                      std::function<void()> heartbeatTimeoutCb);

    void ExtendHeartbeatTimeout();

    bool IsConnected() const;

    State(StateType val)
        : _val(std::move(val))
    {}

private:
    void HeartbeatTick(boost::system::error_code ec);

private:
    mutable std::mutex _mutex;
    StateType _val;
};

} // namespace connection
} // namespace pitaya

#endif // PITAYA_CONNECTION_STATE_H