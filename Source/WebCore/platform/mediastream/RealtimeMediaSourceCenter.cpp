/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeMediaSourceCenter.h"

#if ENABLE(MEDIA_STREAM)

// FIXME: GTK to implement its own RealtimeMediaSourceCenter.
#if PLATFORM(GTK)
#include "MockRealtimeMediaSourceCenter.h"
#endif

#include "CaptureDeviceManager.h"
#include "Logging.h"
#include "MediaStreamPrivate.h"
#include <wtf/SHA1.h>

namespace WebCore {

static RealtimeMediaSourceCenter*& mediaStreamCenterOverride()
{
    static RealtimeMediaSourceCenter* override;
    return override;
}

static HashMap<unsigned, std::function<void()>>& observerMap()
{
    static NeverDestroyed<HashMap<unsigned, std::function<void()>>> map;
    return map;
}

RealtimeMediaSourceCenter& RealtimeMediaSourceCenter::singleton()
{
    RealtimeMediaSourceCenter* override = mediaStreamCenterOverride();
    if (override)
        return *override;
#if PLATFORM(GTK)
    ASSERT(isMainThread());
    notImplemented(); // Return MockRealtimeMediaSourceCenter to avoid crash.
    static NeverDestroyed<MockRealtimeMediaSourceCenter> center;
    return center;
#else
    return RealtimeMediaSourceCenter::platformCenter();
#endif
}

void RealtimeMediaSourceCenter::setSharedStreamCenterOverride(RealtimeMediaSourceCenter* center)
{
    mediaStreamCenterOverride() = center;
}

RealtimeMediaSourceCenter::RealtimeMediaSourceCenter()
{
    m_supportedConstraints.setSupportsWidth(true);
    m_supportedConstraints.setSupportsHeight(true);
    m_supportedConstraints.setSupportsAspectRatio(true);
    m_supportedConstraints.setSupportsFrameRate(true);
    m_supportedConstraints.setSupportsFacingMode(true);
    m_supportedConstraints.setSupportsVolume(true);
    m_supportedConstraints.setSupportsDeviceId(true);
}

RealtimeMediaSourceCenter::~RealtimeMediaSourceCenter() = default;

void RealtimeMediaSourceCenter::createMediaStream(NewMediaStreamHandler&& completionHandler, CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, const MediaStreamRequest& request)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;
    String invalidConstraint;

    if (audioDevice) {
        auto audioSource = audioFactory().createAudioCaptureSource(WTFMove(audioDevice), &request.audioConstraints);
        if (audioSource)
            audioSources.append(audioSource.source());
        else {
#if !LOG_DISABLED
            if (!audioSource.errorMessage.isEmpty())
                LOG(Media, "RealtimeMediaSourceCenter::createMediaStream(%p), audio constraints failed to apply: %s", this, audioSource.errorMessage.utf8().data());
#endif
            completionHandler(nullptr);
            return;
        }
    }

    if (videoDevice) {
        auto videoSource = videoFactory().createVideoCaptureSource(WTFMove(videoDevice), &request.videoConstraints);
        if (videoSource)
            videoSources.append(videoSource.source());
        else {
#if !LOG_DISABLED
            if (!videoSource.errorMessage.isEmpty())
                LOG(Media, "RealtimeMediaSourceCenter::createMediaStream(%p), video constraints failed to apply: %s", this, videoSource.errorMessage.utf8().data());
#endif
            completionHandler(nullptr);
            return;
        }
    }

    completionHandler(MediaStreamPrivate::create(audioSources, videoSources));
}

Vector<CaptureDevice> RealtimeMediaSourceCenter::getMediaStreamDevices()
{
    Vector<CaptureDevice> result;
    for (auto& device : audioCaptureDeviceManager().captureDevices()) {
        if (device.enabled())
            result.append(device);
    }
    for (auto& device : videoCaptureDeviceManager().captureDevices()) {
        if (device.enabled())
            result.append(device);
    }
    for (auto& device : displayCaptureDeviceManager().captureDevices()) {
        if (device.enabled())
            result.append(device);
    }

    return result;
}

static void addStringToSHA1(SHA1& sha1, const String& string)
{
    if (string.isEmpty())
        return;

    if (string.is8Bit() && string.isAllASCII()) {
        const uint8_t nullByte = 0;
        sha1.addBytes(string.characters8(), string.length());
        sha1.addBytes(&nullByte, 1);
        return;
    }

    auto utf8 = string.utf8();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(utf8.data()), utf8.length() + 1); // Include terminating null byte.
}

String RealtimeMediaSourceCenter::hashStringWithSalt(const String& id, const String& hashSalt)
{
    if (id.isEmpty() || hashSalt.isEmpty())
        return emptyString();

    SHA1 sha1;

    addStringToSHA1(sha1, id);
    addStringToSHA1(sha1, hashSalt);
    
    SHA1::Digest digest;
    sha1.computeHash(digest);
    
    return SHA1::hexDigest(digest).data();
}

CaptureDevice RealtimeMediaSourceCenter::captureDeviceWithUniqueID(const String& uniqueID, const String& idHashSalt)
{
    for (auto& device : getMediaStreamDevices()) {
        if (uniqueID == hashStringWithSalt(device.persistentId(), idHashSalt))
            return device;
    }

    return { };
}

ExceptionOr<void> RealtimeMediaSourceCenter::setDeviceEnabled(const String& id, bool enabled)
{
    for (auto& captureDevice : getMediaStreamDevices()) {
        if (id == captureDevice.persistentId()) {
            if (enabled != captureDevice.enabled()) {
                captureDevice.setEnabled(enabled);
                captureDevicesChanged();
            }

            return { };
        }
    }

    return Exception { NotFoundError };
}

RealtimeMediaSourceCenter::DevicesChangedObserverToken RealtimeMediaSourceCenter::addDevicesChangedObserver(std::function<void()>&& observer)
{
    static DevicesChangedObserverToken nextToken = 0;
    observerMap().set(++nextToken, WTFMove(observer));
    return nextToken;
}

void RealtimeMediaSourceCenter::removeDevicesChangedObserver(DevicesChangedObserverToken token)
{
    bool wasRemoved = observerMap().remove(token);
    ASSERT_UNUSED(wasRemoved, wasRemoved);
}

void RealtimeMediaSourceCenter::captureDevicesChanged()
{
    // Copy the hash map because the observer callback may call back in and modify the map.
    auto callbacks = observerMap();
    for (auto& it : callbacks)
        it.value();
}

void RealtimeMediaSourceCenter::validateRequestConstraints(ValidConstraintsHandler&& validHandler, InvalidConstraintsHandler&& invalidHandler, const MediaStreamRequest& request, String&& deviceIdentifierHashSalt)
{
    struct DeviceInfo {
        unsigned fitnessScore;
        CaptureDevice device;
    };

    struct {
        bool operator()(const DeviceInfo& a, const DeviceInfo& b)
        {
            return a.fitnessScore < b.fitnessScore;
        }
    } sortBasedOnFitnessScore;

    Vector<DeviceInfo> audioDeviceInfo;
    Vector<DeviceInfo> videoDeviceInfo;

    String firstInvalidConstraint;
    for (auto& device : getMediaStreamDevices()) {
        if (!device.enabled())
            continue;

        String invalidConstraint;
        CaptureSourceOrError sourceOrError;
        switch (device.type()) {
        case CaptureDevice::DeviceType::Camera: {
            if (request.type != MediaStreamRequest::Type::UserMedia || !request.videoConstraints.isValid)
                continue;

            auto sourceOrError = videoFactory().createVideoCaptureSource(device, { });
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.videoConstraints, invalidConstraint))
                videoDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});
            break;
        }
        case CaptureDevice::DeviceType::Microphone:
            if (request.audioConstraints.isValid) {
                auto sourceOrError = audioFactory().createAudioCaptureSource(device, { });
                if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.audioConstraints, invalidConstraint))
                    audioDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});
            }
            break;
        case CaptureDevice::DeviceType::Screen:
        case CaptureDevice::DeviceType::Application:
        case CaptureDevice::DeviceType::Window:
        case CaptureDevice::DeviceType::Browser: {
            if (request.type != MediaStreamRequest::Type::DisplayMedia)
                continue;
            ASSERT(request.audioConstraints.mandatoryConstraints.isEmpty());
            ASSERT(request.videoConstraints.advancedConstraints.isEmpty());
            if (!request.videoConstraints.isValid || !request.videoConstraints.advancedConstraints.isEmpty() || !request.videoConstraints.mandatoryConstraints.isEmpty())
                continue;

            auto sourceOrError = videoFactory().createVideoCaptureSource(device, { });
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.videoConstraints, invalidConstraint))
                videoDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});
            break;
        }
        case CaptureDevice::DeviceType::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }

        if (!invalidConstraint.isEmpty() && firstInvalidConstraint.isEmpty())
            firstInvalidConstraint = invalidConstraint;
    }

    if ((request.audioConstraints.isValid && audioDeviceInfo.isEmpty()) || (request.videoConstraints.isValid && videoDeviceInfo.isEmpty())) {
        invalidHandler(firstInvalidConstraint);
        return;
    }

    Vector<CaptureDevice> audioDevices;
    if (!audioDeviceInfo.isEmpty()) {
        std::sort(audioDeviceInfo.begin(), audioDeviceInfo.end(), sortBasedOnFitnessScore);
        audioDevices = WTF::map(audioDeviceInfo, [] (auto& info) {
            return info.device;
        });
    }

    Vector<CaptureDevice> videoDevices;
    if (!videoDeviceInfo.isEmpty()) {
        std::sort(videoDeviceInfo.begin(), videoDeviceInfo.end(), sortBasedOnFitnessScore);
        videoDevices = WTF::map(videoDeviceInfo, [] (auto& info) {
            return info.device;
        });
    }

    validHandler(WTFMove(audioDevices), WTFMove(videoDevices), WTFMove(deviceIdentifierHashSalt));
}

void RealtimeMediaSourceCenter::setVideoCapturePageState(bool interrupted, bool pageMuted)
{
    videoFactory().setVideoCapturePageState(interrupted, pageMuted);
}

std::optional<CaptureDevice> RealtimeMediaSourceCenter::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& id)
{
    switch (type) {
    case CaptureDevice::DeviceType::Camera:
        return videoCaptureDeviceManager().captureDeviceWithPersistentID(type, id);
        break;
    case CaptureDevice::DeviceType::Microphone:
        return audioCaptureDeviceManager().captureDeviceWithPersistentID(type, id);
        break;
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Application:
    case CaptureDevice::DeviceType::Window:
    case CaptureDevice::DeviceType::Browser:
        return displayCaptureDeviceManager().captureDeviceWithPersistentID(type, id);
        break;
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return std::nullopt;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
