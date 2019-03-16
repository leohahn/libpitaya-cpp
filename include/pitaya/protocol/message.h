#ifndef PITAYA_PROTOCOL_MESSAGE_H
#define PITAYA_PROTOCOL_MESSAGE_H

#include <boost/optional.hpp>
#include <vector>
#include <string>
#include <unordered_map>

namespace pitaya {
namespace protocol {

static constexpr int kMessageErrorMask = 32;
static constexpr int kMessageGzipMask = 16;
static constexpr int kMessageTypeMask = 7;
static constexpr int kMessageRouteCompressionMask = 1;
static constexpr int kMessageRouteLengthMask = 255;

static constexpr int kMessageFlagSize = 1;

enum class MessageType : uint8_t
{
    Request = 0,
    Notify = 1,
    Response = 2,
    Push = 3,
};

struct Message
{
    MessageType type;
    uint32_t id;
    std::string route;
    std::vector<uint8_t> data;
    bool error;

    static Message NewRequest(size_t id, std::string route, std::vector<uint8_t> data);
    void SerializeInto(std::vector<uint8_t>& buf, const std::unordered_map<std::string, int>& routeDict) const;
};

} // namespace protocol
} // namespace pitaya

#endif // PITAYA_PROTOCOL_MESSAGE_H