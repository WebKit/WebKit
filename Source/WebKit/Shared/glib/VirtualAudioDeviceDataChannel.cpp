/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "VirtualAudioDeviceDataChannel.h"

#if ENABLE(VIRTUAL_AUDIO_DEVICES)

#include "Logging.h"
#include <array>
#include <glib-unix.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/RunLoop.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
using namespace WebCore;

// Every packet starts with the timestamp of the audio buffer, followed by the audio data.
// Both processes run on the same machine, so the header uses the native byte order.
struct PacketHeader {
    uint64_t timestamp;
};

// Audio buffers are usually around 10ms and limited to 64 KiB by WPEVirtualAudioDevice. This
// leaves room for about 41ms of audio in the most demanding format (64 channels of F64 samples
// at 48 kHz), and for much longer buffers in usual formats.
static constexpr size_t maximumAudioDataSize = 1 * MB;

// Dropped buffers and invalid packets are reported at most once per interval.
static constexpr Seconds droppedBuffersLogInterval = 5_s;

std::optional<unsigned> RateLimitedLogCounter::takeIfDue()
{
    if (!m_count)
        return std::nullopt;

    auto now = MonotonicTime::now();
    if (now - m_lastLogTime < droppedBuffersLogInterval)
        return std::nullopt;

    m_lastLogTime = now;
    return std::exchange(m_count, 0);
}

// Maximum number of packets read in one main loop iteration, so that a web process sending
// packets faster than they are consumed can't starve the UI process main loop.
static constexpr unsigned maximumPacketsPerDispatch = 16;

static size_t bytesPerSample(VirtualAudioSampleFormat format)
{
    switch (format) {
    case VirtualAudioSampleFormat::S8:
    case VirtualAudioSampleFormat::U8:
        return 1;
    case VirtualAudioSampleFormat::S16LittleEndian:
    case VirtualAudioSampleFormat::S16BigEndian:
    case VirtualAudioSampleFormat::U16LittleEndian:
    case VirtualAudioSampleFormat::U16BigEndian:
        return 2;
    case VirtualAudioSampleFormat::S24LittleEndian:
    case VirtualAudioSampleFormat::S24BigEndian:
        return 3;
    case VirtualAudioSampleFormat::S24In32LittleEndian:
    case VirtualAudioSampleFormat::S24In32BigEndian:
    case VirtualAudioSampleFormat::S32LittleEndian:
    case VirtualAudioSampleFormat::S32BigEndian:
    case VirtualAudioSampleFormat::U32LittleEndian:
    case VirtualAudioSampleFormat::U32BigEndian:
    case VirtualAudioSampleFormat::F32LittleEndian:
    case VirtualAudioSampleFormat::F32BigEndian:
        return 4;
    case VirtualAudioSampleFormat::F64LittleEndian:
    case VirtualAudioSampleFormat::F64BigEndian:
        return 8;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<VirtualAudioDeviceDataSender> VirtualAudioDeviceDataSender::create(UnixFileDescriptor&& socket)
{
    if (!socket)
        return nullptr;

    return adoptRef(*new VirtualAudioDeviceDataSender(WTF::move(socket)));
}

VirtualAudioDeviceDataSender::VirtualAudioDeviceDataSender(UnixFileDescriptor&& socket)
    : m_socket(WTF::move(socket))
{
}

void VirtualAudioDeviceDataSender::renderAudioData(std::span<const uint8_t> data, uint64_t timestamp)
{
    if (data.empty())
        return;

    Locker locker { m_lock };
    if (m_isClosed)
        return;

    if (data.size() > maximumAudioDataSize) {
        dropBuffer();
        return;
    }

    PacketHeader header { timestamp };
    std::array<struct iovec, 2> iov { {
        { &header, sizeof(header) },
        { const_cast<uint8_t*>(data.data()), data.size() }
    } };
    struct msghdr message { };
    message.msg_iov = iov.data();
    message.msg_iovlen = iov.size();

    ssize_t result;
    do {
        result = sendmsg(m_socket.value(), &message, MSG_DONTWAIT | MSG_NOSIGNAL);
    } while (result == -1 && errno == EINTR);

    if (result != -1) {
        logDroppedBuffersIfNeeded();
        return;
    }

    if (errno == EMSGSIZE) {
        // The buffer doesn't fit in the socket buffer, every buffer of this size will be dropped.
        if (!m_didLogMessageTooBig) {
            RELEASE_LOG_ERROR(Media, "Virtual audio device buffer of %zu bytes is too big for the socket buffer, dropping it", data.size());
            m_didLogMessageTooBig = true;
        }
        dropBuffer();
        return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS) {
        // The UI process is not keeping up: drop the buffer.
        dropBuffer();
        return;
    }

    // The UI process closed its end, stop sending.
    RELEASE_LOG_ERROR(Media, "Failed to send virtual audio device data: %" PUBLIC_LOG_STRING, safeStrerror(errno));
    m_isClosed = true;
}

void VirtualAudioDeviceDataSender::dropBuffer()
{
    m_droppedBuffers.increment();
    logDroppedBuffersIfNeeded();
}

void VirtualAudioDeviceDataSender::logDroppedBuffersIfNeeded()
{
    // Called with m_lock held, from the GStreamer streaming thread.
    if (auto droppedBuffers = m_droppedBuffers.takeIfDue())
        RELEASE_LOG(Media, "Virtual audio device dropped %u buffers because the UI process didn't consume them in time", *droppedBuffers);
}

RefPtr<VirtualAudioDeviceDataReceiver> VirtualAudioDeviceDataReceiver::create(const VirtualAudioDeviceConfiguration& configuration, Handler&& handler, CloseHandler&& closeHandler)
{
    CheckedSize expectedPayloadSize = configuration.framesPerBuffer;
    expectedPayloadSize *= configuration.numberOfChannels;
    expectedPayloadSize *= bytesPerSample(configuration.format);
    if (expectedPayloadSize.hasOverflowed() || !expectedPayloadSize.value() || expectedPayloadSize.value() > maximumAudioDataSize) {
        RELEASE_LOG_ERROR(Media, "Invalid buffer size for virtual audio device");
        return nullptr;
    }

    std::array<int, 2> sockets;
    if (socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, sockets.data()) == -1) {
        RELEASE_LOG_ERROR(Media, "Failed to create virtual audio device socket: %" PUBLIC_LOG_STRING, safeStrerror(errno));
        return nullptr;
    }

    UnixFileDescriptor socket { sockets[0], UnixFileDescriptor::Adopt };
    UnixFileDescriptor senderSocket { sockets[1], UnixFileDescriptor::Adopt };

    // Ask for room for half a second of audio, which leaves plenty of time to the UI process
    // main thread to consume the buffers. The kernel caps it to the system maximum.
    uint64_t bytesPerSecond = static_cast<uint64_t>(configuration.sampleRate) * configuration.numberOfChannels * bytesPerSample(configuration.format);
    int bufferSize = static_cast<int>(std::clamp<uint64_t>(bytesPerSecond / 2, 64 * KB, 16 * MB));
    setsockopt(senderSocket.value(), SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
    setsockopt(socket.value(), SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));

    return adoptRef(*new VirtualAudioDeviceDataReceiver(WTF::move(socket), WTF::move(senderSocket), expectedPayloadSize.value(), WTF::move(handler), WTF::move(closeHandler)));
}

VirtualAudioDeviceDataReceiver::VirtualAudioDeviceDataReceiver(UnixFileDescriptor&& socket, UnixFileDescriptor&& senderSocket, size_t expectedPayloadSize, Handler&& handler, CloseHandler&& closeHandler)
    : m_socket(WTF::move(socket))
    , m_senderSocket(WTF::move(senderSocket))
    , m_expectedPayloadSize(expectedPayloadSize)
    , m_handler(WTF::move(handler))
    , m_closeHandler(WTF::move(closeHandler))
    , m_source(adoptGRef(g_unix_fd_source_new(m_socket.value(), static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR))))
{
    // One more byte than a valid packet, so that bigger packets are detected even without MSG_TRUNC.
    m_buffer.grow(sizeof(PacketHeader) + m_expectedPayloadSize + 1);

    g_source_set_name(m_source.get(), "[WebKit] VirtualAudioDevice");
    g_source_set_callback(m_source.get(), reinterpret_cast<GSourceFunc>(reinterpret_cast<GCallback>(+[](int, GIOCondition condition, gpointer userData) -> gboolean {
        Ref receiver = *static_cast<VirtualAudioDeviceDataReceiver*>(userData);
        // Pending data is read before handling the end of the stream, the source is dispatched
        // again while there is data left.
        if (condition & G_IO_IN) {
            receiver->receive();
            return G_SOURCE_CONTINUE;
        }
        if (condition & (G_IO_HUP | G_IO_ERR)) {
            // The web process closed its end, there won't be more data.
            receiver->close();
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_CONTINUE;
    })), this, nullptr);
    g_source_attach(m_source.get(), RunLoop::mainSingleton().mainContext());
}

VirtualAudioDeviceDataReceiver::~VirtualAudioDeviceDataReceiver()
{
    invalidate();
}

void VirtualAudioDeviceDataReceiver::close()
{
    invalidate();
    if (auto closeHandler = std::exchange(m_closeHandler, nullptr))
        closeHandler(*this);
}

void VirtualAudioDeviceDataReceiver::invalidate()
{
    // The source must be destroyed, not only released: while it's attached, the main context keeps
    // its own reference to it, so it would keep dispatching the callback with a dangling `this`
    // after the receiver is destroyed.
    if (GRefPtr<GSource> source = std::exchange(m_source, nullptr))
        g_source_destroy(source.get());
}

void VirtualAudioDeviceDataReceiver::receive()
{
    for (unsigned i = 0; i < maximumPacketsPerDispatch && m_source; ++i) {
        // With MSG_TRUNC the real size of the packet is returned, even when it doesn't fit in the buffer.
        ssize_t result = recv(m_socket.value(), m_buffer.mutableSpan().data(), m_buffer.size(), MSG_DONTWAIT | MSG_TRUNC);
        if (result == -1) {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                RELEASE_LOG_ERROR(Media, "Failed to receive virtual audio device data: %" PUBLIC_LOG_STRING, safeStrerror(errno));
                close();
            }
            break;
        }

        // The peer closed its end.
        if (!result) {
            close();
            break;
        }

        // The web process is not trusted: the embedder is promised buffers of exactly the
        // configured number of frames, drop any other packet.
        if (static_cast<size_t>(result) != sizeof(PacketHeader) + m_expectedPayloadSize) {
            m_invalidPackets.increment();
            continue;
        }

        auto packet = m_buffer.span().first(sizeof(PacketHeader) + m_expectedPayloadSize);
        PacketHeader header;
        memcpySpan(asMutableByteSpan(header), packet.first(sizeof(PacketHeader)));
        m_handler(packet.subspan(sizeof(PacketHeader)), header.timestamp);
    }

    if (auto invalidPackets = m_invalidPackets.takeIfDue())
        RELEASE_LOG_ERROR(Media, "Virtual audio device dropped %u packets of invalid size received from the web process", *invalidPackets);
}

} // namespace WebKit

#endif // ENABLE(VIRTUAL_AUDIO_DEVICES)
