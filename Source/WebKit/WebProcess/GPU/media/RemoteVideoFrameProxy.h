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
#include "RemoteVideoFrameProxyProperties.h"
#include <WebCore/VideoFrame.h>
#include <wtf/ArgumentCoder.h>

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
    using Properties = RemoteVideoFrameProxyProperties;
    static Properties properties(WebKit::RemoteVideoFrameReference, const WebCore::VideoFrame&);

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

    enum CloneConstructor { cloneConstructor };
    RemoteVideoFrameProxy(CloneConstructor, RemoteVideoFrameProxy&);

    Ref<VideoFrame> clone() final;

    static inline Seconds defaultTimeout = 10_s;

    const RefPtr<RemoteVideoFrameProxy> m_baseVideoFrame;
    const RefPtr<IPC::Connection> m_connection;
    std::optional<RemoteVideoFrameReferenceTracker> m_referenceTracker;
    const WebCore::IntSize m_size;
    uint32_t m_pixelFormat { 0 };
    // FIXME: Remove this.
    mutable RefPtr<RemoteVideoFrameObjectHeapProxy> m_videoFrameObjectHeapProxy;
#if PLATFORM(COCOA)
    mutable Lock m_pixelBufferLock;
    mutable RetainPtr<CVPixelBufferRef> m_pixelBuffer;
#endif
};

TextStream& operator<<(TextStream&, const RemoteVideoFrameProxy::Properties&);

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteVideoFrameProxy)
    static bool isType(const WebCore::VideoFrame& videoFrame) { return videoFrame.isRemoteProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
