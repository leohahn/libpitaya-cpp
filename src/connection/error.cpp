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
