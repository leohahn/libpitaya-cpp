#ifndef PITAYA_CONNECTION_ERROR_H
#define PITAYA_CONNECTION_ERROR_H

#include <boost/system/error_code.hpp>

namespace pitaya {
namespace connection {

enum class ConnectionError
{
    InvalidPacketType = 1,
    TooManyPackets = 2,
    WrongPacket = 3,
};

class ErrorCategory : public boost::system::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int ev) const override;
};

boost::system::error_category& GetErrorCategory();

inline boost::system::error_code
make_error_code(ConnectionError e)
{
    return { static_cast<int>(e), GetErrorCategory() };
}

} // namespace connection
} // namespace pitaya

namespace boost {
namespace system {
template<>
struct ::boost::system::is_error_code_enum<pitaya::connection::ConnectionError>
{
    BOOST_STATIC_CONSTANT(bool, value = true);
};
} // namespace system
} // namespace boost

#endif // PITAYA_CONNECTION_ERROR_H