#ifndef PITAYA_REQUEST_H
#define PITAYA_REQUEST_H

#include <boost/asio/system_timer.hpp>
#include <chrono>
#include <functional>
#include <ostream>
#include <vector>

namespace pitaya {

using RequestData = std::vector<uint8_t>;

enum class RequestStatus
{
    Ok = 0,
    ServerError,
    Timeout,
    Reset,
    InternalError,
    NotConnectedError,
};

using RequestHandler = std::function<void(RequestStatus, RequestData)>;

struct Request
{
    RequestHandler handler;
    boost::asio::system_timer timeout;

    Request(boost::asio::io_context& ioContext,
            RequestHandler handler,
            std::chrono::seconds timeoutSeconds,
            std::function<void(boost::system::error_code)> onTimeout)
        : handler(std::move(handler))
        , timeout(ioContext)
    {
        timeout.expires_after(timeoutSeconds);
        timeout.async_wait(std::move(onTimeout));
    }

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    Request& operator=(Request&& req)
    {
        handler = std::move(req.handler);
        timeout = std::move(req.timeout);
        return *this;
    }
};

inline std::ostream&
operator<<(std::ostream& os, RequestStatus s)
{
    switch (s) {
        case RequestStatus::Ok:
            os << "Ok";
            return os;
        case RequestStatus::ServerError:
            os << "ServerError";
            return os;
        case RequestStatus::Timeout:
            os << "Timeout";
            return os;
        case RequestStatus::Reset:
            os << "Reset";
            return os;
        case RequestStatus::InternalError:
            os << "InternalError";
            return os;
        case RequestStatus::NotConnectedError:
            os << "NotConnectedError";
            return os;
    }
}

} // namespace pitaya

#endif // PITAYA_PROTOCOL_REQUEST_H