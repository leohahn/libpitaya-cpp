#ifndef PITAYA_COMMUNICATION_PACKET_FRAMED_H
#define PITAYA_COMMUNICATION_PACKET_FRAMED_H

#include "pitaya/protocol/packet.h"
#include <boost/system/error_code.hpp>
#include <string>
#include <vector>

namespace pitaya {
namespace connection {

class PacketFramed
{
public:
    using SendHandler = std::function<void(boost::system::error_code)>;
    using ReceiveHandler =
        std::function<void(boost::system::error_code, std::vector<protocol::Packet> packet)>;
    using ConnectHandler = std::function<void(boost::system::error_code)>;

    virtual ~PacketFramed() = default;
    virtual void SendPacket(protocol::Packet packet, SendHandler handler) = 0;
    virtual void ReceivePackets(ReceiveHandler handler) = 0;
    virtual void Connect(const std::string& host,
                         const std::string& port,
                         ConnectHandler handler) = 0;
    virtual void Disconnect() = 0;
};

} // namespace connection
} // namespace pitaya

#endif // PITAYA_COMMUNICATION_PACKET_FRAMED_H