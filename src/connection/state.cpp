#include "pitaya/connection/state.h"
#include <iostream>

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
State::SetConnected(boost::asio::io_context& ioContext,
                    std::string handshakeResponse,
                    std::function<void()> heartbeatTick)
{
    Connected connected(ioContext, std::move(handshakeResponse), std::move(heartbeatTick));
    // TODO, FIXME: do not hardcode expire time
    connected.heartbeatTimer.expires_after(std::chrono::seconds(4));
    connected.heartbeatTimer.async_wait(
        std::bind(&State::HeartbeatTick, this, std::placeholders::_1));
    _val = std::move(connected);
}

void
State::SetConnectionFailed(boost::system::error_code ec, std::string reason)
{
    ConnectionFailed s;
    s.ec = ec;
    s.reason = std::move(reason);
    _val = std::move(s);
}

} // namespace connection
} // namespace pitaya