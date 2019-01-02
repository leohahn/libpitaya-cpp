#include "pitaya/connection/state.h"
#include <iostream>
#include "logger.h"

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

    // TODO, FIXME: do not hardcode expire time
    conn->heartbeatTimer.expires_after(std::chrono::seconds(4));
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

void
State::SetConnected(boost::asio::io_context& ioContext,
                    std::chrono::seconds heartbeatInterval,
                    std::function<void()> heartbeatTick,
                    std::function<void()> heartbeatTimeoutCb)
{
    Connected connected(ioContext, std::move(heartbeatInterval), std::move(heartbeatTick));

    connected.heartbeatTimer.expires_after(heartbeatInterval);
    connected.heartbeatTimer.async_wait(
        std::bind(&State::HeartbeatTick, this, std::placeholders::_1));

    //
    // TODO(lhahn): Should the initial timeout be after 2 times the heartbeat interval??
    //
    connected.heartbeatTimeout.expires_at(std::chrono::system_clock::now() + (heartbeatInterval * 2));
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

    conn->heartbeatTimeout.expires_at(std::chrono::system_clock::now() + std::chrono::seconds(4));
}

} // namespace connection
} // namespace pitaya