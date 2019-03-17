#include "pitaya/connection/state.h"

#include "logger.h"
#include <iostream>

using error_code = boost::system::error_code;

namespace pitaya {
namespace connection {

void
State::HeartbeatTick(boost::system::error_code ec)
{
    if (ec) {
        LOG(Error) << "Heartbeat timer failed: " << ec.message();
        return;
    }

    Connected* conn = boost::get<Connected>(&_val);
    if (!conn) {
        LOG(Error) << "Ignoring tick, state is not connected";
        return;
    }

    // Call tick function
    conn->heartbeatTick();

    conn->heartbeatTimer.expires_after(conn->heartbeatInterval);
    conn->heartbeatTimer.async_wait(std::bind(&State::HeartbeatTick, this, std::placeholders::_1));
}

void
State::SetInited()
{
    _val = Inited();
}

bool
State::IsConnected() const
{
    const Connected* conn = boost::get<Connected>(&_val);
    return conn != nullptr;
}

static constexpr int kHeartbeatFactor = 3;

void
State::SetConnected(boost::asio::io_context& ioContext,
                    std::chrono::seconds heartbeatInterval,
                    std::string serializer,
                    std::unordered_map<std::string, int> routeDict,
                    std::function<void()> heartbeatTick,
                    std::function<void()> heartbeatTimeoutCb)
{
    Connected connected(ioContext,
                        std::move(heartbeatInterval),
                        heartbeatInterval * kHeartbeatFactor,
                        std::move(serializer),
                        std::move(routeDict),
                        std::move(heartbeatTick));

    connected.heartbeatTimer.expires_after(heartbeatInterval);
    connected.heartbeatTimer.async_wait(
        std::bind(&State::HeartbeatTick, this, std::placeholders::_1));

    //
    // TODO(lhahn): Should the initial timeout be after 2 times the heartbeat interval??
    //
    connected.heartbeatTimeout.expires_at(std::chrono::system_clock::now() +
                                          connected.heartbeatTimeoutSeconds);
    connected.heartbeatTimeout.async_wait([heartbeatTimeoutCb](error_code ec) {
        if (!ec) {
            heartbeatTimeoutCb();
        }
    });

    _val = std::move(connected);
}

void
State::ExtendHeartbeatTimeout()
{
    Connected* conn = boost::get<Connected>(&_val);
    if (!conn) {
        LOG(Error) << "Cannot extend heartbeat timeout, incorrect state";
        return;
    }

    conn->heartbeatTimeout.expires_at(std::chrono::system_clock::now() +
                                      conn->heartbeatTimeoutSeconds);
}

} // namespace connection
} // namespace pitaya