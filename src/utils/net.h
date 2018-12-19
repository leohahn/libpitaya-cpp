#ifndef PITAYA_UTILS_NET_H
#define PITAYA_UTILS_NET_H

#include <boost/optional.hpp>
#include <string>

namespace pitaya {
namespace utils {

inline boost::optional<std::pair<std::string, std::string>>
SplitHostAndPort(const std::string& address)
{
    auto colonPos = address.find_first_of(':');

    if (colonPos == std::string::npos) {
        return boost::none;
    }

    auto host = address.substr(0, colonPos);
    auto port = address.substr(colonPos + 1);

    return std::make_pair(host, port);
}

} // namespace utils
} // namespace pitaya

#endif // PITAYA_UTILS_NET_H