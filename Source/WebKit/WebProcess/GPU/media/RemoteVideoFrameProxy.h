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

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "MessageReceiver.h"
#include "RemoteVideoFrameIdentifier.h"
#include <WebCore/VideoFrame.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebKit {

class GPUProcessConnection;

class RemoteVideoFrameProxy final : public WebCore::VideoFrame, private IPC::MessageReceiver, private GPUProcessConnection::Client {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RemoteVideoFrameProxy);
public:
    // Creates a promise of a MediaSample that exists in GPU process. The object can be used as out parameter of the creation message.
    static RefPtr<RemoteVideoFrameProxy> create(GPUProcessConnection&);

    struct Properties {
        MediaTime presentationTime;
        bool videoIsMirrored { false };
        VideoRotation videoRotation { VideoRotation::None };

        template<typename Encoder> void encode(Encoder&) const;
        template<typename Decoder> static std::optional<Properties> decode(Decoder&);
    };

    static Properties properties(const WebCore::MediaSample&);

    ~RemoteVideoFrameProxy() final;

    RemoteVideoFrameIdentifier identifier() const;
    RemoteVideoFrameWriteReference write() const;
    RemoteVideoFrameReadReference read() const;

    // WebCore::VideoFrame overrides.
    MediaTime presentationTime() const final;
    VideoRotation videoRotation() const final;
    bool videoMirrored() const final;
    std::optional<WebCore::MediaSampleVideoFrame> videoFrame() const final;
    // FIXME: When VideoFrame is not MediaSample, these will not be needed.
    WebCore::PlatformSample platformSample() const final;
    uint32_t videoPixelFormat() const final;

    static bool handleMessageToRemovedDestination(IPC::Connection&, IPC::Decoder&);
private:
    RemoteVideoFrameProxy(GPUProcessConnection&, RemoteVideoFrameIdentifier);
    RemoteVideoFrameProxy();
    void disconnectGPUProcessIfNeeded() const;
    bool waitForWasInitialized() const WARN_UNUSED_RETURN;

    // IPC::MessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    // GPUProcessConnection::Client overrides.
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    // Messages.
    void wasInitialized(std::optional<Properties>&&);
    static inline Seconds defaultSendTimeout = 30_s;

    mutable GPUProcessConnection* m_gpuProcessConnection { nullptr };
    RemoteVideoFrameReferenceTracker m_referenceTracker;
    mutable bool m_wasInitialized { false };
    MediaTime m_presentationTime;
    bool m_videoIsMirrored { false };
    VideoRotation m_videoRotation { VideoRotation::None };
};

template<typename Encoder> void RemoteVideoFrameProxy::Properties::encode(Encoder& encoder) const
{
    encoder << presentationTime
        << videoIsMirrored
        << videoRotation;
}

template<typename Decoder> std::optional<RemoteVideoFrameProxy::Properties> RemoteVideoFrameProxy::Properties::decode(Decoder& decoder)
{
    std::optional<MediaTime> presentationTime;
    std::optional<bool> videoIsMirrored;
    std::optional<VideoRotation> videoRotation;
    decoder >> presentationTime
        >> videoIsMirrored
        >> videoRotation;
    if (!decoder.isValid())
        return std::nullopt;
    return Properties { WTFMove(*presentationTime), *videoIsMirrored, *videoRotation };
}

}
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteVideoFrameProxy)
    static bool isType(const WebCore::MediaSample& mediaSample) { return mediaSample.platformSample().type == WebCore::PlatformSample::RemoteVideoFrameProxyType; }
SPECIALIZE_TYPE_TRAITS_END()
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoFrame)
    static bool isType(const WebCore::MediaSample& mediaSample) { return mediaSample.platformSample().type == WebCore::PlatformSample::RemoteVideoFrameProxyType; }
SPECIALIZE_TYPE_TRAITS_END()
#endif
