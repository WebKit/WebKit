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

#pragma once

#if ENABLE(VIRTUAL_AUDIO_DEVICES)

#include <WebCore/VirtualAudioDevice.h>
#include <optional>
#include <span>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebKit {

// Counts events that are logged at most once per interval.
class RateLimitedLogCounter {
public:
    void increment() { ++m_count; }
    // Returns the number of events since the last log if one is due, and resets it.
    std::optional<unsigned> takeIfDue();

private:
    unsigned m_count { 0 };
    MonotonicTime m_lastLogTime;
};

// Delivers the audio rendered on a virtual audio device from the web process (sender) to the
// UI process (receiver) through a SOCK_SEQPACKET socket pair: every audio buffer is sent as a
// single packet, preceded by its timestamp, so the kernel keeps the buffer boundaries. Buffers are dropped by the sender
// when the receiver doesn't keep up and the socket buffer is full.

// Web process side: receives the audio from the GStreamer virtual device sink.
class VirtualAudioDeviceDataSender final : public WebCore::VirtualAudioDeviceSink {
    WTF_MAKE_TZONE_ALLOCATED(VirtualAudioDeviceDataSender);
public:
    static RefPtr<VirtualAudioDeviceDataSender> create(UnixFileDescriptor&& socket);

private:
    explicit VirtualAudioDeviceDataSender(UnixFileDescriptor&& socket);

    void renderAudioData(std::span<const uint8_t>, uint64_t timestamp) final;
    void dropBuffer() WTF_REQUIRES_LOCK(m_lock);
    void logDroppedBuffersIfNeeded() WTF_REQUIRES_LOCK(m_lock);

    Lock m_lock;
    const UnixFileDescriptor m_socket;
    RateLimitedLogCounter m_droppedBuffers WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isClosed WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_didLogMessageTooBig WTF_GUARDED_BY_LOCK(m_lock) { false };
};

// UI process side: reads the audio from the socket on the main thread. Only packets containing
// exactly the number of frames per buffer of the device are delivered to the handler. The receiver
// is closed when the sender socket is closed (the web process page is closed, the web process
// exits, or the sender socket is discarded before reaching the web process), and the close handler
// is called. The close handler is not called when the receiver is destroyed while still open,
// for instance with its page.
class VirtualAudioDeviceDataReceiver final : public RefCounted<VirtualAudioDeviceDataReceiver> {
    WTF_MAKE_TZONE_ALLOCATED(VirtualAudioDeviceDataReceiver);
public:
    using Handler = Function<void(std::span<const uint8_t>, uint64_t timestamp)>;
    using CloseHandler = Function<void(VirtualAudioDeviceDataReceiver&)>;
    static RefPtr<VirtualAudioDeviceDataReceiver> create(const WebCore::VirtualAudioDeviceConfiguration&, Handler&&, CloseHandler&&);
    ~VirtualAudioDeviceDataReceiver();

    UnixFileDescriptor takeSenderSocket() { return WTF::move(m_senderSocket); }

    // Stops delivering the audio, without calling the close handler.
    void invalidate();

private:
    VirtualAudioDeviceDataReceiver(UnixFileDescriptor&& socket, UnixFileDescriptor&& senderSocket, size_t expectedPayloadSize, Handler&&, CloseHandler&&);

    void receive();
    void close();

    const UnixFileDescriptor m_socket;
    UnixFileDescriptor m_senderSocket;
    const size_t m_expectedPayloadSize;
    Handler m_handler;
    CloseHandler m_closeHandler;
    Vector<uint8_t> m_buffer;
    GRefPtr<GSource> m_source;
    RateLimitedLogCounter m_invalidPackets;
};

} // namespace WebKit

#endif // ENABLE(VIRTUAL_AUDIO_DEVICES)
