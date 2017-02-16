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

#import "config.h"
#import "CaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM)

#import "Logging.h"
#import "MediaConstraints.h"
#import "RealtimeMediaSource.h"
#import "RealtimeMediaSourceCenter.h"
#import "RealtimeMediaSourceSettings.h"
#import "UUID.h"
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringHash.h>

using namespace WebCore;

CaptureDeviceManager::~CaptureDeviceManager()
{
}

Vector<CaptureDevice> CaptureDeviceManager::getSourcesInfo()
{
    Vector<CaptureDevice> sourcesInfo;
    for (auto captureDevice : captureDevices()) {
        if (!captureDevice.enabled() || captureDevice.type() == CaptureDevice::DeviceType::Unknown)
            continue;

        sourcesInfo.append(captureDevice);
    }
    LOG(Media, "CaptureDeviceManager::getSourcesInfo(%p), found %zu active devices", this, sourcesInfo.size());
    return sourcesInfo;
}

bool CaptureDeviceManager::captureDeviceFromDeviceID(const String& captureDeviceID, CaptureDevice& foundDevice)
{
    for (auto& device : captureDevices()) {
        if (device.persistentId() == captureDeviceID) {
            foundDevice = device;
            return true;
        }
    }

    return false;
}

Vector<String> CaptureDeviceManager::bestSourcesForTypeAndConstraints(RealtimeMediaSource::Type type, const MediaConstraints& constraints, String& invalidConstraint)
{
    Vector<RefPtr<RealtimeMediaSource>> bestSources;

    struct {
        bool operator()(RefPtr<RealtimeMediaSource> a, RefPtr<RealtimeMediaSource> b)
        {
            return a->fitnessScore() < b->fitnessScore();
        }
    } sortBasedOnFitnessScore;

    CaptureDevice::DeviceType deviceType = type == RealtimeMediaSource::Video ? CaptureDevice::DeviceType::Video : CaptureDevice::DeviceType::Audio;
    for (auto& captureDevice : captureDevices()) {
        if (!captureDevice.enabled() || captureDevice.type() != deviceType)
            continue;

        if (auto captureSource = createMediaSourceForCaptureDeviceWithConstraints(captureDevice, &constraints, invalidConstraint))
            bestSources.append(captureSource.leakRef());
    }

    Vector<String> sourceUIDs;
    if (bestSources.isEmpty())
        return sourceUIDs;

    sourceUIDs.reserveInitialCapacity(bestSources.size());
    std::sort(bestSources.begin(), bestSources.end(), sortBasedOnFitnessScore);
    for (auto& device : bestSources)
        sourceUIDs.uncheckedAppend(device->persistentID());

    return sourceUIDs;
}

RefPtr<RealtimeMediaSource> CaptureDeviceManager::sourceWithUID(const String& deviceUID, RealtimeMediaSource::Type type, const MediaConstraints* constraints, String& invalidConstraint)
{
    for (auto& captureDevice : captureDevices()) {
        if (type == RealtimeMediaSource::None)
            continue;

        CaptureDevice::DeviceType deviceType = type == RealtimeMediaSource::Video ? CaptureDevice::DeviceType::Video : CaptureDevice::DeviceType::Audio;
        if (captureDevice.persistentId() != deviceUID || captureDevice.type() != deviceType)
            continue;

        if (!captureDevice.enabled())
            continue;

        if (auto mediaSource = createMediaSourceForCaptureDeviceWithConstraints(captureDevice, constraints, invalidConstraint))
            return mediaSource;
    }

    return nullptr;
}

#endif // ENABLE(MEDIA_STREAM)
