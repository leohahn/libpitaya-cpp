#include "pitaya/protocol/message.h"

#include "logger.h"
#include "utils/gzip.h"

using boost::optional;

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

static inline constexpr bool
IsMessageTypeInvalid(uint8_t type)
{
    return type > 3;
}

optional<Message>
Message::Deserialize(const std::vector<uint8_t>& buf,
                     const std::unordered_map<int, std::string>& codeToRoute)
{
    if (buf.size() < kMessageHeaderLength) {
        LOG(Error) << "Received wrong message size: " << buf.size();
        return boost::none;
    }

    Message msg = {};

    uint8_t flag = buf[0];
    size_t offset = 1;

    uint8_t type = (flag >> 1) & kMessageTypeMask;

    if (IsMessageTypeInvalid(type)) {
        LOG(Error) << "Received invalid message type: " << type;
        return boost::none;
    }

    msg.type = static_cast<MessageType>(type);

    if (msg.type == MessageType::Request || msg.type == MessageType::Response) {
        uint64_t id = 0;
        // little end byte order
        // WARNING: have to be stored in 64 bits integer
        // variant length encode
        for (size_t i = offset; i < buf.size(); ++i) {
            uint8_t b = buf[i];
            id += static_cast<uint64_t>(b & 0x7F) << (7 * (i - offset));
            if (b < 128) {
                offset = i + 1;
                break;
            }
        }
        msg.id = id;
    }

    msg.error = (flag & kMessageErrorMask) == kMessageErrorMask;

    if (IsMessageTypeRoutable(msg.type)) {
        if ((flag & kMessageRouteCompressionMask) == 1) {
            uint8_t code = (buf[offset] << 8) | buf[offset + 1];

            if (codeToRoute.find(code) == codeToRoute.end()) {
                return boost::none;
            }

            msg.route = codeToRoute.at(code);
            offset += 2;
        } else {
            uint8_t routeLength = buf[offset++];
            msg.route = std::string(buf[offset], routeLength);
            offset += routeLength;
        }
    }

    if ((flag & kMessageGzipMask) == kMessageGzipMask) {
        uint8_t* output = nullptr;
        size_t size = 0;

        int rc = utils::Decompress(
            &output, &size, const_cast<uint8_t*>(&buf[offset]), buf.size() - offset);

        if (rc) {
            LOG(Error) << "Failed to decompress message body";
            return boost::none;
        }

        msg.data.insert(msg.data.end(), output, output + size);
        free(output);
    } else {
        msg.data.insert(msg.data.end(), buf.begin() + offset, buf.end());
    }

    return msg;
}

} // namespace protocol
} // namespace pitaya
