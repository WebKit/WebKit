/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "RemoteRealtimeMediaSource.h"
#include "RemoteRealtimeMediaSourceProxy.h"
#include <WebCore/CaptureDevice.h>
#include <WebCore/RealtimeMediaSourceIdentifier.h>
#include <WebCore/VideoFrame.h>

namespace WebCore {
struct MediaConstraints;
}

namespace WebKit {

class UserMediaCaptureManager;

class RemoteRealtimeVideoSource final : public RemoteRealtimeMediaSource {
public:
    static Ref<WebCore::RealtimeMediaSource> create(const WebCore::CaptureDevice&, const WebCore::MediaConstraints*, WebCore::MediaDeviceHashSalts&&, UserMediaCaptureManager&, bool shouldCaptureInGPUProcess, WebCore::PageIdentifier);
    ~RemoteRealtimeVideoSource();

    void remoteVideoFrameAvailable(WebCore::VideoFrame&, WebCore::VideoFrameTimeMetadata);

private:
    RemoteRealtimeVideoSource(WebCore::RealtimeMediaSourceIdentifier, const WebCore::CaptureDevice&, const WebCore::MediaConstraints*, WebCore::MediaDeviceHashSalts&&, UserMediaCaptureManager&, bool shouldCaptureInGPUProcess, WebCore::PageIdentifier);
    RemoteRealtimeVideoSource(RemoteRealtimeMediaSourceProxy&&, WebCore::MediaDeviceHashSalts&&, UserMediaCaptureManager&, WebCore::PageIdentifier);

    Ref<RealtimeMediaSource> clone() final;
    void endProducingData() final;
    bool setShouldApplyRotation(bool) final;
};

} // namespace WebKit

#endif
