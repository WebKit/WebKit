/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 */

#include "config.h"
#include "CaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include "MediaConstraints.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"
#include <wtf/MainThread.h>
#include <wtf/text/StringHash.h>

using namespace WebCore;

CaptureDeviceManager::~CaptureDeviceManager()
{
}

Vector<CaptureDevice> CaptureDeviceManager::getAudioSourcesInfo()
{
    Vector<CaptureDevice> sourcesInfo;
    for (auto& captureDevice : captureDevices()) {
        if (!captureDevice.enabled() || captureDevice.type() != CaptureDevice::DeviceType::Audio)
            continue;

        sourcesInfo.append(captureDevice);
    }
    LOG(Media, "CaptureDeviceManager::getSourcesInfo(%p), found %zu active devices", this, sourcesInfo.size());
    return sourcesInfo;
}

Vector<CaptureDevice> CaptureDeviceManager::getVideoSourcesInfo()
{
    Vector<CaptureDevice> sourcesInfo;
    for (auto& captureDevice : captureDevices()) {
        if (!captureDevice.enabled() || captureDevice.type() != CaptureDevice::DeviceType::Video)
            continue;

        sourcesInfo.append(captureDevice);
    }
    LOG(Media, "CaptureDeviceManager::getSourcesInfo(%p), found %zu active devices", this, sourcesInfo.size());
    return sourcesInfo;
}

std::optional<CaptureDevice> CaptureDeviceManager::captureDeviceFromPersistentID(const String& captureDeviceID)
{
    for (auto& device : captureDevices()) {
        if (device.persistentId() == captureDeviceID)
            return device;
    }

    return std::nullopt;
}

std::optional<CaptureDevice> CaptureDeviceManager::deviceWithUID(const String& deviceUID, RealtimeMediaSource::Type type)
{
    for (auto& captureDevice : captureDevices()) {
        CaptureDevice::DeviceType deviceType;

        switch (type) {
        case RealtimeMediaSource::Type::None:
            continue;
        case RealtimeMediaSource::Type::Audio:
            deviceType = CaptureDevice::DeviceType::Audio;
            break;
        case RealtimeMediaSource::Type::Video:
            deviceType = CaptureDevice::DeviceType::Video;
            break;
        }

        if (captureDevice.persistentId() != deviceUID || captureDevice.type() != deviceType)
            continue;

        if (!captureDevice.enabled())
            continue;

        return captureDevice;
    }

    return std::nullopt;
}

static CaptureDeviceManager::ObserverToken nextObserverToken()
{
    static CaptureDeviceManager::ObserverToken nextToken = 0;
    return ++nextToken;
}

CaptureDeviceManager::ObserverToken CaptureDeviceManager::addCaptureDeviceChangedObserver(CaptureDeviceChangedCallback&& observer)
{
    auto token = nextObserverToken();
    m_observers.set(token, WTFMove(observer));
    return token;
}

void CaptureDeviceManager::removeCaptureDeviceChangedObserver(ObserverToken token)
{
    ASSERT(m_observers.contains(token));
    m_observers.remove(token);
}


#endif // ENABLE(MEDIA_STREAM)
