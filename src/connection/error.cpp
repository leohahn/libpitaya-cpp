#include "pitaya/connection/error.h"

namespace pitaya {
namespace connection {

const char*
ErrorCategory::name() const noexcept
{
    return "pitaya_connection";
}

std::string
ErrorCategory::message(int ev) const
{
    switch (ev) {
        case static_cast<int>(ConnectionError::InvalidPacketType):
            return "Received invalid packet type from server";
        case static_cast<int>(ConnectionError::TooManyPackets):
            return "Received too many packets from the server";
        case static_cast<int>(ConnectionError::WrongPacket):
            return "Received wrong packet from the server";
        default:
            return "Unknown error";
    }
}

boost::system::error_category&
GetErrorCategory()
{
    static ErrorCategory ec;
    return ec;
}

} // namespace connection
} // namespace pitaya
