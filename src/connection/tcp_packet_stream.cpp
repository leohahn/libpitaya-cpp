#include "connection/tcp_packet_stream.h"

#include "pitaya/connection/error.h"
#include "pitaya/exception.h"

#include "logger.h"
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

TcpPacketStream::TcpPacketStream(std::shared_ptr<boost::asio::io_context> ioContext,
                                 ReadBufferMaxSize readBufferMaxSize)
    : _ioContext(std::move(ioContext))
    , _socket(*_ioContext)
    , _writeId(0)
    , _readBufferMaxSize(readBufferMaxSize)
    , _readBufferHead(0)
{
    _readBuffer.resize(_readBufferMaxSize(), 0);
    LOG(Debug) << "TCP Packet Stream created";
}

void
TcpPacketStream::Connect(const std::string& host, const std::string& port, ConnectHandler handler)
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
TcpPacketStream::Disconnect()
{
    try {
        _socket.shutdown(tcp::socket::shutdown_both);
    } catch (const boost::exception& exc) {
        // NOTE(lhahn): Boost throws an error if the socket was already shutdown.
        // We just ignore it and continue execution normally.
    }
    if (_socket.is_open())
        _socket.close();
}

void
TcpPacketStream::SendPacket(protocol::Packet packet, SendHandler handler)
{
    if (_packetSendQueue.size() < kMaxQueuedPackets) {
        _packetSendQueue.push_back(std::move(packet));
        _ioContext->post([this, h = std::move(handler)]() { this->SendNextPacket(h); });
    } else {
        std::cout << "Ignoring packet, since queue is already full\n";
    }
}

void
TcpPacketStream::ReceivePackets(ReceiveHandler handler)
{
    ReadToBuffer(std::move(handler));
}

void
TcpPacketStream::ReadToBuffer(ReceiveHandler handler)
{
    void* bufferStart =
        reinterpret_cast<void*>(reinterpret_cast<char*>(&_readBuffer[0]) + _readBufferHead);
    size_t bufferSize = _readBuffer.size() - _readBufferHead;

    _socket.async_receive(
        asio::buffer(bufferStart, bufferSize), [this, handler](error_code ec, size_t bytesRead) {
            if (ec) {
                // If an error occured, behave as if no bytes were read.
                // TODO: Broadcast error to client.
                // TODO: Should the connection be closed here?
                LOG(Error) << "Failed to read data from buffer: " << ec.message();
                handler(ec, vector<protocol::Packet>());
                return;
            }

            assert(bytesRead > 0);
            _readBufferHead += bytesRead;
            assert(_readBufferHead <= _readBuffer.size());

            try {
                auto packets = this->ParsePacketsFromReadBuffer();

                if (packets.size() == 0) {
                    this->ReadToBuffer(handler);
                } else {
                    handler(boost_errc::make_error_code(boost_errc::success), std::move(packets));
                }
            } catch (const pitaya::Exception& exc) {
                LOG(Error) << "Parsing server packets failed: " << exc.what();
                handler(ConnectionError::InvalidPacketType, vector<protocol::Packet>());
            }
        });
}

void
TcpPacketStream::SendNextPacket(SendHandler handler)
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
    _socket.async_send(
        asio::buffer(buf), [this, id, h = std::move(handler)](error_code ec, size_t nwritten) {
            if (ec) {
                std::cerr << "Failed to send buffer to server: " << ec.message() << "\n";
                h(ec);
                return;
            }

            h(boost_errc::make_error_code(boost_errc::success));
            this->_writeBuffers.erase(id);
        });

    // Put the buffer into the write set.
    assert(_writeBuffers.find(_writeId) == _writeBuffers.end());
    _writeBuffers[id] = std::move(buf);
}

vector<protocol::Packet>
TcpPacketStream::ParsePacketsFromReadBuffer()
{
    // When this function is called, _readBufferHead points to the first
    // position that does not contain a valid byte.
    size_t packetStartOffset = 0;

    vector<protocol::Packet> packets;

    const size_t bufferSize = _readBufferHead;

    for (;;) {
        size_t sizeLeft = bufferSize - packetStartOffset;

        if (sizeLeft < protocol::kPacketHeaderSize) {
            std::rotate(
                _readBuffer.begin(), _readBuffer.begin() + packetStartOffset, _readBuffer.end());
            _readBufferHead = sizeLeft;
            return packets;
        }

        assert(packetStartOffset < _readBuffer.size());

        // Get the type of the packet.
        auto packetType = protocol::PacketTypeFromByte(_readBuffer[packetStartOffset]);

        if (!packetType) {
            throw pitaya::Exception("Received an invalid packet type from the server.");
        }

        // Read the length of the packet.
        int length = (_readBuffer[packetStartOffset + 1] << 16) |
                     (_readBuffer[packetStartOffset + 2] << 8) |
                     (_readBuffer[packetStartOffset + 3]);

        if (sizeLeft < protocol::kPacketHeaderSize + length) {
            std::rotate(
                _readBuffer.begin(), _readBuffer.begin() + packetStartOffset, _readBuffer.end());
            _readBufferHead = sizeLeft;
            return packets;
        }

        switch (packetType.value()) {
            case protocol::PacketType::Handshake:
                packets.push_back(protocol::NewHandshake(
                    _readBuffer.data() + packetStartOffset + protocol::kPacketHeaderSize, length));
                packetStartOffset += protocol::kPacketHeaderSize + length;
                break;
            case protocol::PacketType::Heartbeat:
                assert(length == 0);
                packets.push_back(protocol::NewHeartbeat());
                packetStartOffset += protocol::kPacketHeaderSize;
                break;
            case protocol::PacketType::Data: {
                uint8_t* dataStart = const_cast<uint8_t*>(_readBuffer.data()) + packetStartOffset +
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
