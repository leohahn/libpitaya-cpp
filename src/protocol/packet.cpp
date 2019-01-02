#include "pitaya/protocol/packet.h"

#include <string>

namespace pitaya {
namespace protocol {

static constexpr uint8_t kFirstByteMask = 0xff;

static constexpr uint8_t
FirstByte(uint32_t val)
{
    return val & kFirstByteMask;
}

static constexpr uint8_t
SecondByte(uint32_t val)
{
    return (val >> 8) & kFirstByteMask;
}

static constexpr uint8_t
ThirdByte(uint32_t val)
{
    return (val >> 16) & kFirstByteMask;
}

void
Packet::SerializeInto(std::vector<uint8_t>& buf) const
{
    buf.reserve(buf.size() + kPacketHeaderSize + body.size());

    // Write header
    buf.push_back(static_cast<uint8_t>(type));
    // Write length in big endian
    buf.push_back(ThirdByte(body.size()));
    buf.push_back(SecondByte(body.size()));
    buf.push_back(FirstByte(body.size()));
    // Write the body of the packet
    buf.insert(buf.end(), body.begin(), body.end());
}

} // namespace protocol
} // namespace pitaya
