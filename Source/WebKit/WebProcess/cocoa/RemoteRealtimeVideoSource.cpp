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

#include "config.h"
#include "RemoteRealtimeVideoSource.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "UserMediaCaptureManager.h"
#include "UserMediaCaptureManagerProxyMessages.h"

namespace WebKit {
using namespace WebCore;

Ref<RealtimeMediaSource> RemoteRealtimeVideoSource::create(const CaptureDevice& device, const MediaConstraints* constraints, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, std::optional<PageIdentifier> pageIdentifier)
{
    auto source = adoptRef(*new RemoteRealtimeVideoSource(RealtimeMediaSourceIdentifier::generate(), device, constraints, WTFMove(hashSalts), manager, shouldCaptureInGPUProcess, pageIdentifier));
    manager.addSource(source.copyRef());
    manager.remoteCaptureSampleManager().addSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeVideoSource::RemoteRealtimeVideoSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, std::optional<PageIdentifier> pageIdentifier)
    : RemoteRealtimeMediaSource(identifier, device, constraints, WTFMove(hashSalts), manager, shouldCaptureInGPUProcess, pageIdentifier)
{
    ASSERT(this->pageIdentifier());
}

RemoteRealtimeVideoSource::RemoteRealtimeVideoSource(RemoteRealtimeMediaSourceProxy&& proxy, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, std::optional<PageIdentifier> pageIdentifier)
    : RemoteRealtimeMediaSource(WTFMove(proxy), WTFMove(hashSalts), manager, pageIdentifier)
{
    ASSERT(this->pageIdentifier());
}

RemoteRealtimeVideoSource::~RemoteRealtimeVideoSource() = default;

void RemoteRealtimeVideoSource::endProducingData()
{
    proxy().endProducingData();
}

bool RemoteRealtimeVideoSource::setShouldApplyRotation(bool shouldApplyRotation)
{
    connection().send(Messages::UserMediaCaptureManagerProxy::SetShouldApplyRotation { identifier(), shouldApplyRotation }, 0);
    return true;
}

Ref<RealtimeMediaSource> RemoteRealtimeVideoSource::clone()
{
    RefPtr<RemoteRealtimeVideoSource> clone;
    callOnMainRunLoopAndWait([this, &clone] {
        if (isEnded() || proxy().isEnded()) {
            clone = this;
            return;
        }

        clone = adoptRef(*new RemoteRealtimeVideoSource(proxy().clone(), MediaDeviceHashSalts { deviceIDHashSalts() }, manager(), *pageIdentifier()));

        clone->m_registerOwnerCallback = m_registerOwnerCallback;
        clone->setSettings(RealtimeMediaSourceSettings { settings() });
        clone->setCapabilities(RealtimeMediaSourceCapabilities { capabilities() });

        manager().addSource(*clone);
        manager().remoteCaptureSampleManager().addSource(*clone);
        proxy().createRemoteCloneSource(clone->identifier(), *pageIdentifier());

        bool isNewClonedSource = true;
        clone->m_registerOwnerCallback(*clone, isNewClonedSource);
    });

    return clone.releaseNonNull();
}

void RemoteRealtimeVideoSource::remoteVideoFrameAvailable(VideoFrame& frame, VideoFrameTimeMetadata metadata)
{
    MediaTime sampleTime = frame.presentationTime();

    auto frameTime = sampleTime.toDouble();
    m_observedFrameTimeStamps.append(frameTime);
    m_observedFrameTimeStamps.removeAllMatching([&](auto time) {
        return time <= frameTime - 2;
    });

    auto interval = m_observedFrameTimeStamps.last() - m_observedFrameTimeStamps.first();
    if (interval > 1)
        m_observedFrameRate = (m_observedFrameTimeStamps.size() / interval);

    setIntrinsicSize(expandedIntSize(frame.presentationSize()));
    videoFrameAvailable(frame, metadata);
}

}

#endif
