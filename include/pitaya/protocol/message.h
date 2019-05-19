#pragma once

#include <boost/optional.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace pitaya {
namespace protocol {

static constexpr int kMessageErrorMask = 32;
static constexpr int kMessageGzipMask = 16;
static constexpr int kMessageTypeMask = 7;
static constexpr int kMessageRouteCompressionMask = 1;
static constexpr int kMessageRouteLengthMask = 255;
static constexpr int kNotifyMessageId = 0;

static constexpr int kMessageFlagSize = 1;
static constexpr int kMessageHeaderLength = 2;

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
    uint64_t id;
    std::string route;
    std::vector<uint8_t> data;
    bool error;

    static Message NewRequest(uint64_t id, std::string route, std::vector<uint8_t> data);
    void SerializeInto(std::vector<uint8_t>& buf,
                       const std::unordered_map<std::string, int>& routeToCode) const;

    static boost::optional<Message> Deserialize(
        const std::vector<uint8_t>& buf,
        const std::unordered_map<int, std::string>& codeToRoute);
};

} // namespace protocol
} // namespace pitaya
