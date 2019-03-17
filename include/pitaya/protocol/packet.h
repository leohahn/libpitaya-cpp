#ifndef PITAYA_PROTOCOL_PACKET_H
#define PITAYA_PROTOCOL_PACKET_H

#include "pitaya/exception.h"
#include "pitaya/protocol/message.h"

#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <ostream>
#include <string>
#include <vector>

namespace pitaya {
namespace protocol {

static constexpr const int kPacketHeaderSize = 4;

enum class PacketType : uint8_t
{
    Handshake = 0x01,
    HandshakeAck = 0x02,
    Heartbeat = 0x03,
    Data = 0x04,
    Kick = 0x05,
};

enum class RouteCompression
{
    Yes,
    No,
};

inline std::ostream&
operator<<(std::ostream& os, PacketType p)
{
    switch (p) {
        case PacketType::Handshake:
            os << "Handshake";
            break;
        case PacketType::HandshakeAck:
            os << "HandshakeAck";
            break;
        case PacketType::Heartbeat:
            os << "Heartbeat";
            break;
        case PacketType::Data:
            os << "Data";
            break;
        case PacketType::Kick:
            os << "Kick";
            break;
    }
    return os;
}

struct Packet
{
public:
    PacketType type;
    std::vector<uint8_t> body;

    Packet()
        : type(PacketType::Handshake)
    {}

    void SerializeInto(std::vector<uint8_t>& buf) const;
};

inline Packet
NewHandshake(const std::string& json)
{
    Packet p;
    p.type = PacketType::Handshake;
    p.body = std::vector<uint8_t>(json.data(), json.data() + json.size());
    return p;
}

inline Packet
NewHandshake(uint8_t* data, size_t length)
{
    Packet p;
    p.type = PacketType::Handshake;
    p.body = std::vector<uint8_t>(data, data + length);
    return p;
}

inline Packet
NewHandshakeAck()
{
    Packet p;
    p.type = PacketType::HandshakeAck;
    return p;
}

inline Packet
NewHeartbeat()
{
    Packet p;
    p.type = PacketType::Heartbeat;
    return p;
}

inline Packet
NewData(Message message, const std::unordered_map<std::string, int>& routeDict)
{
    Packet p;
    p.type = PacketType::Data;
    message.SerializeInto(p.body, routeDict);
    return p;
}

inline Packet
NewData(std::vector<uint8_t> data)
{
    Packet p;
    p.type = PacketType::Data;
    p.body = std::move(data);
    return p;
}

inline Packet
NewData(uint8_t* start, size_t length)
{
    Packet p;
    p.type = PacketType::Data;
    p.body = std::vector<uint8_t>(start, start + length);
    return p;
}

inline Packet
NewKick()
{
    Packet p;
    p.type = PacketType::Kick;
    return p;
}

inline std::vector<uint8_t>
NewRequestMessage(uint64_t msgId,
                  boost::variant<std::string, int> codeOrRoute,
                  std::vector<uint8_t> data)
{
    assert(false && "invalid function specialization");
    return std::vector<uint8_t>();
}

/**
 * @brief Returns a PacketType given a 1 byte integer. If the number is invalid,
 * returns boost::none.
 *
 * @param packetType the packet type represented as a byte.
 * @return boost::optional<PacketType>
 */
inline boost::optional<PacketType>
PacketTypeFromByte(uint8_t packetType)
{
    if (packetType >= static_cast<uint8_t>(PacketType::Handshake) &&
        packetType <= static_cast<uint8_t>(PacketType::Kick)) {
        return static_cast<PacketType>(packetType);
    }

    return boost::none;
}

} // namespace protocol
} // namespace pitaya

#endif // PITAYA_PROTOCOL_PACKET_H
