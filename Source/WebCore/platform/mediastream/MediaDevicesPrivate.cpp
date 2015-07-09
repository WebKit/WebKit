/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "MediaDevicesPrivate.h"

#if ENABLE(MEDIA_STREAM)

#include "AVCaptureDeviceManager.h"
#include "AVMediaCaptureSource.h"
#include "MediaStreamTrackSourcesRequestClient.h"

namespace WebCore {

MediaDevicesPrivate::MediaDevicesPrivate()
{
}

Vector<RefPtr<MediaDeviceInfo>> MediaDevicesPrivate::availableMediaDevices(ScriptExecutionContext& context)
{
    Vector<RefPtr<TrackSourceInfo>> capturedDevices = AVCaptureDeviceManager::singleton().getSourcesInfo("");
    Vector<RefPtr<MediaDeviceInfo>> mediaDevicesInfo;
    for (auto device : capturedDevices) {
        TrackSourceInfo* trackInfo = device.get();
        String deviceType = trackInfo->kind() == TrackSourceInfo::SourceKind::Audio ? MediaDeviceInfo::audioInputType() : MediaDeviceInfo::videoInputType();
        mediaDevicesInfo.append(MediaDeviceInfo::create(&context, trackInfo->label(), trackInfo->deviceId(), trackInfo->groupId(), deviceType));
    }
    
    return mediaDevicesInfo;
}

}

#endif
