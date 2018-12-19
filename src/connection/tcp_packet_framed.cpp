#include "connection/tcp_packet_framed.h"
#include "pitaya/connection/error.h"
#include "pitaya/exception.h"
#include <algorithm>
#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;
using tcp = boost::asio::ip::tcp;
using system_error = boost::system::system_error;
using error_code = boost::system::error_code;
using boost::optional;
using pitaya::connection::ConnectionError;
using std::vector;
namespace boost_errc = boost::system::errc;

namespace pitaya {
namespace connection {

static constexpr int kMaxQueuedPackets = 50;

TcpPacketFramed::TcpPacketFramed(std::shared_ptr<boost::asio::io_context> ioContext,
                                 ReadBufferMaxSize readBufferMaxSize)
    : _ioContext(std::move(ioContext))
    , _socket(*_ioContext)
    , _writeId(0)
    , _readBufferMaxSize(readBufferMaxSize)
{
    _readBuffer.resize(_readBufferMaxSize(), 0);
    std::cout << "TCP Packet Framed created\n";
}

void
TcpPacketFramed::Connect(const std::string& host, const std::string& port, ConnectHandler handler)
{
    tcp::resolver resolver(*_ioContext);

    try {
        auto endpointIterator = resolver.resolve({ host, port });

        asio::async_connect(
            _socket,
            endpointIterator,
            [handler = std::move(handler)](error_code ec, tcp::endpoint endpoint) {
                if (ec) {
                    handler(ec);
                    return;
                }

                handler(boost::system::errc::make_error_code(boost::system::errc::success));
            });
    } catch (const system_error& exc) {
        throw Exception("Error resolving host " + host);
    }
}

void
TcpPacketFramed::SendPacket(protocol::Packet packet, SendHandler handler)
{
    if (_packetSendQueue.size() < kMaxQueuedPackets) {
        _packetSendQueue.push_back(std::move(packet));
        _ioContext->post([this]() { this->SendNextPacket(); });
    } else {
        std::cout << "Ignoring packet, since queue is already full\n";
    }
}

void
TcpPacketFramed::ReceivePackets(ReceiveHandler handler)
{
    ReadToBuffer(std::move(handler));
}

void
TcpPacketFramed::ReadToBuffer(ReceiveHandler handler)
{
    void* bufferStart =
        reinterpret_cast<void*>(reinterpret_cast<char*>(&_readBuffer[0]) + _readBuffer.size());
    size_t bufferSize = _readBufferMaxSize() - _readBuffer.size();

    _socket.async_receive(
        asio::buffer(bufferStart, bufferSize), [this, handler](error_code ec, size_t bytesRead) {
            if (ec) {
                // If an error occured, behave as if no bytes were read.
                // TODO: Broadcast error to client.
                // TODO: Should the connection be closed here?
                std::cerr << "Failed to read data from buffer: " << ec.message() << "\n";
                handler(ec, vector<protocol::Packet>());
                return;
            }

            assert(bytesRead > 0);

            try {
                auto packets = this->ParsePacketsFromBuffer(_readBuffer);

                if (packets.size() == 0) {
                    this->ReadToBuffer(handler);
                } else {
                    handler(ec, std::move(packets));
                }
            } catch (const pitaya::Exception& exc) {
                std::cerr << "Parsing server packets failed: " << exc.what() << "\n";
                handler(ConnectionError::InvalidPacketType, vector<protocol::Packet>());
            }
        });
}

void
TcpPacketFramed::SendNextPacket()
{
    assert(!_packetSendQueue.empty());

    // Take packet from the front of the queue.
    auto pkt = std::move(_packetSendQueue.front());
    _packetSendQueue.pop_front();

    // What is this buffer write id
    auto id = _writeId++;

    // Serialize packet into a buffer.
    vector<uint8_t> buf;
    pkt.SerializeInto(buf);

    // Send buffer asynchronously
    _socket.async_send(asio::buffer(buf), [this, id](error_code ec, size_t nwritten) {
        if (ec) {
            std::cerr << "Failed to send buffer to server: " << ec.message() << "\n";
            return;
        }

        std::cout << "Successfuly sent packet id " << id << "\n";
        this->_writeBuffers.erase(id);
    });

    // Put the buffer into the write set.
    assert(_writeBuffers.find(_writeId) == _writeBuffers.end());
    _writeBuffers[id] = std::move(buf);
}

vector<protocol::Packet>
TcpPacketFramed::ParsePacketsFromBuffer(vector<uint8_t>& buf)
{
    size_t packetStartOffset = 0;

    vector<protocol::Packet> packets;

    for (;;) {
        size_t sizeLeft = buf.size() - packetStartOffset;

        if (sizeLeft < protocol::kPacketHeaderSize) {
            std::rotate(buf.begin(), buf.begin() + packetStartOffset, buf.end());
            buf.resize(sizeLeft);
            return packets;
        }

        // Get the type of the packet.
        auto packetType = protocol::PacketTypeFromByte(buf[packetStartOffset]);

        if (!packetType) {
            throw pitaya::Exception("Received an invalid packet type from the server.");
        }

        // Read the length of the packet.
        int length = (buf[packetStartOffset + 1] << 16) | (buf[packetStartOffset + 2] << 8) |
                     (buf[packetStartOffset + 3]);

        if (sizeLeft < protocol::kPacketHeaderSize + length) {
            std::rotate(buf.begin(), buf.begin() + packetStartOffset, buf.end());
            buf.resize(sizeLeft);
            return packets;
        }

        switch (packetType.value()) {
            case protocol::PacketType::Handshake:
                assert(length == 0);
                packets.push_back(protocol::NewHandshake());
                packetStartOffset += protocol::kPacketHeaderSize;
                break;
            case protocol::PacketType::Heartbeat:
                assert(length == 0);
                packets.push_back(protocol::NewHeartbeat());
                packetStartOffset += protocol::kPacketHeaderSize;
                break;
            case protocol::PacketType::Data: {
                uint8_t* dataStart = const_cast<uint8_t*>(buf.data()) + packetStartOffset +
                                     protocol::kPacketHeaderSize;
                packets.push_back(protocol::NewData(dataStart, length));
                packetStartOffset += protocol::kPacketHeaderSize + length;
            } break;
            case protocol::PacketType::Kick:
                assert(length == 0);
                packets.push_back(protocol::NewKick());
                packetStartOffset += protocol::kPacketHeaderSize;
                break;
            default:
                assert(false && "Unexpected packet type from server");
                break;
        }
    }
}

} // namespace connection
} // namespace pitaya
