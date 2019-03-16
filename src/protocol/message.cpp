#include "pitaya/protocol/message.h"

namespace pitaya {
namespace protocol {

static inline constexpr bool
IsMessageTypeRoutable(MessageType type)
{
    return type == MessageType::Request || type == MessageType::Notify || type == MessageType::Push;
}

Message
Message::NewRequest(size_t id, std::string route, std::vector<uint8_t> data)
{
    Message message = {};
    message.type = MessageType::Request;
    message.id = id;
    message.route = std::move(route);
    return message;
}

void 
Message::SerializeInto(std::vector<uint8_t>& buf, 
                       const std::unordered_map<std::string, int>& routeDict) const
{
    buf.reserve(kMessageFlagSize + data.size());

    // Save message flag to the buffer
    auto isRouteCompressed = routeDict.find(route) != routeDict.end();
    auto code = routeDict.at(route);

    {
        uint8_t flag = 0;

        if (isRouteCompressed) {
            flag |= kMessageRouteCompressionMask;
        }

        if (error) {
            flag |= kMessageErrorMask;
        }

        buf.push_back(flag);
    }

    if (type == MessageType::Request || type == MessageType::Response) {
        // Now, encode the message id as base 128 varints
        size_t n = id;
        for (;;) {
            uint8_t b = (n % 128);
            n >>= 7;
            if (n != 0) {
                buf.push_back(b + 128);
            } else {
                buf.push_back(b);
                break;
            }
        }
    }

    if (IsMessageTypeRoutable(type)) {
        // Encode the route
        if (isRouteCompressed) {
            buf.push_back((code >> 8) & 255);
            buf.push_back(code & 255);
        } else {
            buf.push_back((uint8_t)route.size());
            buf.insert(buf.end(), route.begin(), route.end());
        }
    }

    // TODO, FIXME: Check if the message should be compressed!
    if (false) {
        // Deflate the data
    }

    buf.insert(buf.end(), data.begin(), data.end());
}

} // namespace protocol
} // namespace pitaya
