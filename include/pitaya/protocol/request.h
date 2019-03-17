#ifndef PITAYA_PROTOCOL_REQUEST_H
#define PITAYA_PROTOCOL_REQUEST_H

#include <ostream>
#include <vector>

namespace pitaya {
namespace protocol {

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

} // namespace protocol
} // namespace pitaya

#endif // PITAYA_PROTOCOL_REQUEST_H