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
#include "RemoteRealtimeAudioSource.h"

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)

#include "GPUProcessConnection.h"
#include "UserMediaCaptureManager.h"
#include "WebProcess.h"

namespace WebKit {
using namespace WebCore;

Ref<RealtimeMediaSource> RemoteRealtimeAudioSource::create(const CaptureDevice& device, const MediaConstraints* constraints, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, PageIdentifier pageIdentifier)
{
    auto source = adoptRef(*new RemoteRealtimeAudioSource(RealtimeMediaSourceIdentifier::generate(), device, constraints, WTFMove(hashSalts), manager, shouldCaptureInGPUProcess, pageIdentifier));
    manager.addSource(source.copyRef());
    manager.remoteCaptureSampleManager().addSource(source.copyRef());
    source->createRemoteMediaSource();
    return source;
}

RemoteRealtimeAudioSource::RemoteRealtimeAudioSource(RealtimeMediaSourceIdentifier identifier, const CaptureDevice& device, const MediaConstraints* constraints, MediaDeviceHashSalts&& hashSalts, UserMediaCaptureManager& manager, bool shouldCaptureInGPUProcess, PageIdentifier pageIdentifier)
    : RemoteRealtimeMediaSource(identifier, device, constraints, WTFMove(hashSalts), manager, shouldCaptureInGPUProcess, pageIdentifier)
{
    ASSERT(device.type() == CaptureDevice::DeviceType::Microphone);
}

RemoteRealtimeAudioSource::~RemoteRealtimeAudioSource()
{
    removeAsClient();
}

void RemoteRealtimeAudioSource::remoteAudioSamplesAvailable(const MediaTime& time, const PlatformAudioData& data, const AudioStreamDescription& description, size_t size)
{
    ASSERT(!isMainRunLoop());
    audioSamplesAvailable(time, data, description, size);
}

void RemoteRealtimeAudioSource::setIsInBackground(bool value)
{
    connection().send(Messages::UserMediaCaptureManagerProxy::SetIsInBackground { identifier(), value }, 0);
}

}

#endif
