#ifndef PITAYA_PROTOCOL_PACKET_H
#define PITAYA_PROTOCOL_PACKET_H

#include <boost/optional.hpp>
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
NewData(uint8_t* data, size_t size)
{
    Packet p;
    p.type = PacketType::Data;
    p.body = std::vector<uint8_t>(data, data + size);
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
NewKick()
{
    Packet p;
    p.type = PacketType::Kick;
    // TODO: add data contents
    return p;
}

Packet Deserialize(const std::vector<uint8_t>& buf);

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