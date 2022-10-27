/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "GPUProcessConnection.h"
#include "RemoteVideoFrameIdentifier.h"
#include <WebCore/VideoFrame.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
#if PLATFORM(COCOA)
class VideoFrameCV;
#endif
}

namespace WebKit {

class GPUProcessConnection;
class RemoteVideoFrameObjectHeapProxy;

// A WebCore::VideoFrame class that points to a concrete WebCore::VideoFrame instance
// in another process, GPU process.
class RemoteVideoFrameProxy final : public WebCore::VideoFrame {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RemoteVideoFrameProxy);
public:
    struct Properties {
        // The receiver owns the reference, so it must be released via either adoption to
        // `RemoteVideoFrameProxy::create()` or via `RemoteVideoFrameProxy::releaseUnused()`.
        WebKit::RemoteVideoFrameReference reference;
        MediaTime presentationTime;
        bool isMirrored { false };
        Rotation rotation { Rotation::None };
        WebCore::IntSize size;
        uint32_t pixelFormat { 0 };
        WebCore::PlatformVideoColorSpace colorSpace;

        template<typename Encoder> void encode(Encoder&) const;
        template<typename Decoder> static std::optional<Properties> decode(Decoder&);
    };

    static Properties properties(WebKit::RemoteVideoFrameReference&&, const WebCore::VideoFrame&);

    static Ref<RemoteVideoFrameProxy> create(IPC::Connection&, RemoteVideoFrameObjectHeapProxy&, Properties&&);

    // Called by the end-points that capture creation messages that are sent from GPUP but
    // whose destinations were released in WP before message was processed.
    static void releaseUnused(IPC::Connection&, Properties&&);

    ~RemoteVideoFrameProxy() final;

    RemoteVideoFrameIdentifier identifier() const;
    RemoteVideoFrameReadReference newReadReference() const;

    WebCore::IntSize size() const { return m_size; }

    // WebCore::VideoFrame overrides.
    WebCore::FloatSize presentationSize() const final { return m_size; }
    uint32_t pixelFormat() const final;
    bool isRemoteProxy() const final { return true; }
#if PLATFORM(COCOA)
    CVPixelBufferRef pixelBuffer() const final;
#endif

private:
    RemoteVideoFrameProxy(IPC::Connection&, RemoteVideoFrameObjectHeapProxy&, Properties&&);
    static inline Seconds defaultTimeout = 10_s;

    const Ref<IPC::Connection> m_connection;
    RemoteVideoFrameReferenceTracker m_referenceTracker;
    const WebCore::IntSize m_size;
    uint32_t m_pixelFormat { 0 };
    // FIXME: Remove this.
    mutable RefPtr<RemoteVideoFrameObjectHeapProxy> m_videoFrameObjectHeapProxy;
#if PLATFORM(COCOA)
    mutable Lock m_pixelBufferLock;
    mutable RetainPtr<CVPixelBufferRef> m_pixelBuffer;
#endif
};

template<typename Encoder> void RemoteVideoFrameProxy::Properties::encode(Encoder& encoder) const
{
    encoder << reference << presentationTime << isMirrored << rotation << size << pixelFormat << colorSpace;
}

template<typename Decoder> std::optional<RemoteVideoFrameProxy::Properties> RemoteVideoFrameProxy::Properties::decode(Decoder& decoder)
{
    auto reference = decoder.template decode<RemoteVideoFrameReference>();
    auto presentationTime = decoder.template decode<MediaTime>();
    auto isMirrored = decoder.template decode<bool>();
    auto rotation = decoder.template decode<Rotation>();
    auto size = decoder.template decode<WebCore::IntSize>();
    auto pixelFormat = decoder.template decode<uint32_t>();
    auto colorSpace = decoder.template decode<WebCore::PlatformVideoColorSpace>();
    if (!decoder.isValid())
        return std::nullopt;
    return Properties { WTFMove(*reference), WTFMove(*presentationTime), *isMirrored, *rotation, *size, *pixelFormat, *colorSpace };
}

TextStream& operator<<(TextStream&, const RemoteVideoFrameProxy::Properties&);

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteVideoFrameProxy)
    static bool isType(const WebCore::VideoFrame& videoFrame) { return videoFrame.isRemoteProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
