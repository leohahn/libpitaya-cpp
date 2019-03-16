#ifndef PITAYA_CONNECTION_TCP_PACKET_STREAM_H
#define PITAYA_CONNECTION_TCP_PACKET_STREAM_H

#include "pitaya/connection/packet_stream.h"
#include <array>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/optional.hpp>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace pitaya {
namespace connection {

class TcpPacketStream : public PacketStream
{
public:
    struct ReadBufferMaxSize
    {
        explicit ReadBufferMaxSize(size_t size)
            : _val(size)
        {}
        size_t operator()() const { return _val; }

    private:
        size_t _val;
    };

    TcpPacketStream(std::shared_ptr<boost::asio::io_context> ioContext,
                    ReadBufferMaxSize readBufferMaxSize);

    void SendPacket(protocol::Packet packet, SendHandler handler) override;
    void ReceivePackets(ReceiveHandler handler) override;
    void Connect(const std::string& host, const std::string& port, ConnectHandler handler) override;
    void Disconnect() override;

    // We cannot copy a TcpPacketFramed object.
    TcpPacketStream& operator=(const TcpPacketStream&) = delete;
    TcpPacketStream(const TcpPacketStream&) = delete;

private:
    void SendNextPacket(SendHandler handler);
    void ReadToBuffer(ReceiveHandler handler);
    std::vector<protocol::Packet> ParsePacketsFromReadBuffer();

private:
    std::shared_ptr<boost::asio::io_context> _ioContext;

    // The raw tcp socket
    boost::asio::ip::tcp::socket _socket;

    std::deque<protocol::Packet> _packetSendQueue;
    std::unordered_map<uint32_t, std::vector<uint8_t>> _writeBuffers;
    uint32_t _writeId;

    ReadBufferMaxSize _readBufferMaxSize;
    size_t _readBufferHead;
    std::vector<uint8_t> _readBuffer;
    std::vector<uint8_t> _incompletePacketBuffer;
};

} // namespace connection
} // namespace pitaya

#endif // PITAYA_CONNECTION_TCP_PACKET_STREAM_H