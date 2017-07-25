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
    
    return RealtimeMediaSourceCenter::platformCenter();
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

RealtimeMediaSourceCenter::~RealtimeMediaSourceCenter()
{
}

void RealtimeMediaSourceCenter::createMediaStream(NewMediaStreamHandler&& completionHandler, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints* audioConstraints, const MediaConstraints* videoConstraints)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;
    String invalidConstraint;

    if (!audioDeviceID.isEmpty()) {
        auto audioSource = audioFactory().createAudioCaptureSource(audioDeviceID, audioConstraints);
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
    if (!videoDeviceID.isEmpty()) {
        auto videoSource = videoFactory().createVideoCaptureSource(videoDeviceID, videoConstraints);
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

    result.appendVector(audioCaptureDeviceManager().getAudioSourcesInfo());
    result.appendVector(videoCaptureDeviceManager().getVideoSourcesInfo());

    return result;
}

void RealtimeMediaSourceCenter::setAudioFactory(RealtimeMediaSource::AudioCaptureFactory& factory)
{
    m_audioFactory = &factory;
}

void RealtimeMediaSourceCenter::unsetAudioFactory(RealtimeMediaSource::AudioCaptureFactory& factory)
{
    if (m_audioFactory == &factory)
        m_audioFactory = nullptr;
}

RealtimeMediaSource::AudioCaptureFactory& RealtimeMediaSourceCenter::audioFactory()
{
    return m_audioFactory ? *m_audioFactory : defaultAudioFactory();
}

void RealtimeMediaSourceCenter::setVideoFactory(RealtimeMediaSource::VideoCaptureFactory& factory)
{
    m_videoFactory = &factory;
}

void RealtimeMediaSourceCenter::unsetVideoFactory(RealtimeMediaSource::VideoCaptureFactory& factory)
{
    if (m_videoFactory == &factory)
        m_videoFactory = nullptr;
}

RealtimeMediaSource::VideoCaptureFactory& RealtimeMediaSourceCenter::videoFactory()
{
    return m_videoFactory ? *m_videoFactory : defaultVideoFactory();
}

void RealtimeMediaSourceCenter::setAudioCaptureDeviceManager(CaptureDeviceManager& deviceManager)
{
    m_audioCaptureDeviceManager = &deviceManager;
}

void RealtimeMediaSourceCenter::unsetAudioCaptureDeviceManager(CaptureDeviceManager& deviceManager)
{
    if (m_audioCaptureDeviceManager == &deviceManager)
        m_audioCaptureDeviceManager = nullptr;
}

CaptureDeviceManager& RealtimeMediaSourceCenter::audioCaptureDeviceManager()
{
    return m_audioCaptureDeviceManager ? *m_audioCaptureDeviceManager : defaultAudioCaptureDeviceManager();
}

void RealtimeMediaSourceCenter::setVideoCaptureDeviceManager(CaptureDeviceManager& deviceManager)
{
    m_videoCaptureDeviceManager = &deviceManager;
}

void RealtimeMediaSourceCenter::unsetVideoCaptureDeviceManager(CaptureDeviceManager& deviceManager)
{
    if (m_videoCaptureDeviceManager == &deviceManager)
        m_videoCaptureDeviceManager = nullptr;
}

CaptureDeviceManager& RealtimeMediaSourceCenter::videoCaptureDeviceManager()
{
    return m_videoCaptureDeviceManager ? *m_videoCaptureDeviceManager : defaultVideoCaptureDeviceManager();
}

static void addStringToSHA1(SHA1& sha1, const String& string)
{
    if (string.isEmpty())
        return;

    if (string.is8Bit() && string.containsOnlyASCII()) {
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

std::optional<CaptureDevice> RealtimeMediaSourceCenter::captureDeviceWithUniqueID(const String& uniqueID, const String& idHashSalt)
{
    for (auto& device : getMediaStreamDevices()) {
        if (uniqueID == hashStringWithSalt(device.persistentId(), idHashSalt))
            return device;
    }

    return std::nullopt;
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

void RealtimeMediaSourceCenter::validateRequestConstraints(ValidConstraintsHandler&& validHandler, InvalidConstraintsHandler&& invalidHandler, const MediaConstraints& audioConstraints, const MediaConstraints& videoConstraints, String&& deviceIdentifierHashSalt)
{
    struct DeviceInfo {
        unsigned fitnessScore;
        String id;
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
        if (device.type() == CaptureDevice::DeviceType::Video && videoConstraints.isValid) {
            auto sourceOrError = videoFactory().createVideoCaptureSource(device.persistentId(), nullptr);
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(videoConstraints, invalidConstraint))
                videoDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device.persistentId()});
        } else if (device.type() == CaptureDevice::DeviceType::Audio && audioConstraints.isValid) {
            auto sourceOrError = audioFactory().createAudioCaptureSource(device.persistentId(), nullptr);
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(audioConstraints, invalidConstraint))
                audioDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device.persistentId()});
        }

        if (!invalidConstraint.isEmpty() && firstInvalidConstraint.isEmpty())
            firstInvalidConstraint = invalidConstraint;
    }

    if ((audioConstraints.isValid && audioDeviceInfo.isEmpty()) || (videoConstraints.isValid && videoDeviceInfo.isEmpty())) {
        invalidHandler(firstInvalidConstraint);
        return;
    }

    Vector<String> audioSourceIds;
    if (!audioDeviceInfo.isEmpty()) {
        audioSourceIds.reserveInitialCapacity(audioDeviceInfo.size());
        std::sort(audioDeviceInfo.begin(), audioDeviceInfo.end(), sortBasedOnFitnessScore);
        for (auto& info : audioDeviceInfo)
            audioSourceIds.uncheckedAppend(WTFMove(info.id));
    }

    Vector<String> videoSourceIds;
    if (!videoDeviceInfo.isEmpty()) {
        videoSourceIds.reserveInitialCapacity(videoDeviceInfo.size());
        std::sort(videoDeviceInfo.begin(), videoDeviceInfo.end(), sortBasedOnFitnessScore);
        for (auto& info : videoDeviceInfo)
            videoSourceIds.uncheckedAppend(WTFMove(info.id));
    }

    validHandler(WTFMove(audioSourceIds), WTFMove(videoSourceIds), WTFMove(deviceIdentifierHashSalt));
}

void RealtimeMediaSourceCenter::setVideoCapturePageState(bool interrupted, bool pageMuted)
{
    videoFactory().setVideoCapturePageState(interrupted, pageMuted);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
