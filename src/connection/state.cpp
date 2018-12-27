#include "pitaya/connection/state.h"
#include <iostream>

using error_code = boost::system::error_code;

namespace pitaya {
namespace connection {

void
State::HeartbeatTick(boost::system::error_code ec)
{
    if (ec) {
        std::cerr << "Heartbeat timer failed: " << ec.message() << "\n";
        return;
    }

    Connected* conn = boost::get<Connected>(&_val);
    if (!conn) {
        std::cerr << "Ignoring tick, state is not connected\n";
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
                    std::string handshakeResponse,
                    std::function<void()> heartbeatTick,
                    std::function<void()> heartbeatTimeoutCb)
{
    Connected connected(ioContext, std::move(handshakeResponse), std::move(heartbeatTick));
    // TODO, FIXME: do not hardcode expire time
    connected.heartbeatTimer.expires_after(std::chrono::seconds(4));
    connected.heartbeatTimer.async_wait(
        std::bind(&State::HeartbeatTick, this, std::placeholders::_1));

    connected.heartbeatTimeout.expires_at(std::chrono::system_clock::now() + std::chrono::seconds(8));
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
        std::cerr << "Cannot extend heartbeat timeout, incorrect state\n";
        return;
    }

    conn->heartbeatTimeout.expires_at(std::chrono::system_clock::now() + std::chrono::seconds(4));
}

} // namespace connection
} // namespace pitaya