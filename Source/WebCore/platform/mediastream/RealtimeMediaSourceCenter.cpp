/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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


RealtimeMediaSourceCenter& RealtimeMediaSourceCenter::singleton()
{
    return RealtimeMediaSourceCenter::platformCenter();
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

void RealtimeMediaSourceCenter::createMediaStream(NewMediaStreamHandler&& completionHandler, String&& hashSalt, CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, const MediaStreamRequest& request)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;
    String invalidConstraint;

    if (audioDevice) {
        auto audioSource = audioFactory().createAudioCaptureSource(WTFMove(audioDevice), String { hashSalt }, &request.audioConstraints);
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
        CaptureSourceOrError videoSource;
        if (videoDevice.type() == CaptureDevice::DeviceType::Camera)
            videoSource = videoFactory().createVideoCaptureSource(WTFMove(videoDevice), WTFMove(hashSalt), &request.videoConstraints);
        else
            videoSource = displayCaptureFactory().createDisplayCaptureSource(WTFMove(videoDevice), &request.videoConstraints);

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
    for (auto& device : audioFactory().audioCaptureDeviceManager().captureDevices()) {
        if (device.enabled())
            result.append(device);
    }
    for (auto& device : videoFactory().videoCaptureDeviceManager().captureDevices()) {
        if (device.enabled())
            result.append(device);
    }
    for (auto& device : displayCaptureFactory().displayCaptureDeviceManager().captureDevices()) {
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

void RealtimeMediaSourceCenter::setDevicesChangedObserver(std::function<void()>&& observer)
{
    ASSERT(isMainThread());
    ASSERT(!m_deviceChangedObserver);
    m_deviceChangedObserver = WTFMove(observer);
}

void RealtimeMediaSourceCenter::captureDevicesChanged()
{
    ASSERT(isMainThread());
    if (m_deviceChangedObserver)
        m_deviceChangedObserver();
}

void RealtimeMediaSourceCenter::getDisplayMediaDevices(const MediaStreamRequest& request, Vector<DeviceInfo>& diaplayDeviceInfo, String& firstInvalidConstraint)
{
    if (!request.videoConstraints.isValid)
        return;

    String invalidConstraint;
    for (auto& device : displayCaptureFactory().displayCaptureDeviceManager().captureDevices()) {
        if (!device.enabled())
            return;

        auto sourceOrError = displayCaptureFactory().createDisplayCaptureSource(device, { });
        if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.videoConstraints, invalidConstraint))
            diaplayDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});

        if (!invalidConstraint.isEmpty() && firstInvalidConstraint.isEmpty())
            firstInvalidConstraint = invalidConstraint;
    }
}

void RealtimeMediaSourceCenter::getUserMediaDevices(const MediaStreamRequest& request, String&& hashSalt, Vector<DeviceInfo>& audioDeviceInfo, Vector<DeviceInfo>& videoDeviceInfo, String& firstInvalidConstraint)
{
    String invalidConstraint;
    if (request.audioConstraints.isValid) {
        for (auto& device : audioFactory().audioCaptureDeviceManager().captureDevices()) {
            if (!device.enabled())
                continue;

            auto sourceOrError = audioFactory().createAudioCaptureSource(device, String { hashSalt }, { });
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.audioConstraints, invalidConstraint))
                audioDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});

            if (!invalidConstraint.isEmpty() && firstInvalidConstraint.isEmpty())
                firstInvalidConstraint = invalidConstraint;
        }
    }

    if (request.videoConstraints.isValid) {
        for (auto& device : videoFactory().videoCaptureDeviceManager().captureDevices()) {
            if (!device.enabled())
                continue;

            auto sourceOrError = videoFactory().createVideoCaptureSource(device, String { hashSalt }, { });
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.videoConstraints, invalidConstraint))
                videoDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});

            if (!invalidConstraint.isEmpty() && firstInvalidConstraint.isEmpty())
                firstInvalidConstraint = invalidConstraint;
        }
    }
}

void RealtimeMediaSourceCenter::validateRequestConstraints(ValidConstraintsHandler&& validHandler, InvalidConstraintsHandler&& invalidHandler, const MediaStreamRequest& request, String&& deviceIdentifierHashSalt)
{
    struct {
        bool operator()(const DeviceInfo& a, const DeviceInfo& b)
        {
            return a.fitnessScore < b.fitnessScore;
        }
    } sortBasedOnFitnessScore;

    Vector<DeviceInfo> audioDeviceInfo;
    Vector<DeviceInfo> videoDeviceInfo;
    String firstInvalidConstraint;

    if (request.type == MediaStreamRequest::Type::DisplayMedia)
        getDisplayMediaDevices(request, videoDeviceInfo, firstInvalidConstraint);
    else
        getUserMediaDevices(request, String { deviceIdentifierHashSalt }, audioDeviceInfo, videoDeviceInfo, firstInvalidConstraint);

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
        return videoFactory().videoCaptureDeviceManager().captureDeviceWithPersistentID(type, id);
        break;
    case CaptureDevice::DeviceType::Microphone:
        return audioFactory().audioCaptureDeviceManager().captureDeviceWithPersistentID(type, id);
        break;
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Application:
    case CaptureDevice::DeviceType::Window:
    case CaptureDevice::DeviceType::Browser:
        return displayCaptureFactory().displayCaptureDeviceManager().captureDeviceWithPersistentID(type, id);
        break;
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return std::nullopt;
}

static AudioCaptureFactory* audioFactoryOverride;
void RealtimeMediaSourceCenter::setAudioFactory(AudioCaptureFactory& factory)
{
    audioFactoryOverride = &factory;
}
void RealtimeMediaSourceCenter::unsetAudioFactory(AudioCaptureFactory& oldOverride)
{
    ASSERT_UNUSED(oldOverride, audioFactoryOverride == &oldOverride);
    if (&oldOverride == audioFactoryOverride)
        audioFactoryOverride = nullptr;
}

AudioCaptureFactory& RealtimeMediaSourceCenter::audioFactory()
{
    return audioFactoryOverride ? *audioFactoryOverride : RealtimeMediaSourceCenter::singleton().audioFactoryPrivate();
}

static VideoCaptureFactory* videoFactoryOverride;
void RealtimeMediaSourceCenter::setVideoFactory(VideoCaptureFactory& factory)
{
    videoFactoryOverride = &factory;
}
void RealtimeMediaSourceCenter::unsetVideoFactory(VideoCaptureFactory& oldOverride)
{
    ASSERT_UNUSED(oldOverride, videoFactoryOverride == &oldOverride);
    if (&oldOverride == videoFactoryOverride)
        videoFactoryOverride = nullptr;
}

VideoCaptureFactory& RealtimeMediaSourceCenter::videoFactory()
{
    return videoFactoryOverride ? *videoFactoryOverride : RealtimeMediaSourceCenter::singleton().videoFactoryPrivate();
}

static DisplayCaptureFactory* displayCaptureFactoryOverride;
void RealtimeMediaSourceCenter::setDisplayCaptureFactory(DisplayCaptureFactory& factory)
{
    displayCaptureFactoryOverride = &factory;
}
void RealtimeMediaSourceCenter::unsetDisplayCaptureFactory(DisplayCaptureFactory& oldOverride)
{
    ASSERT_UNUSED(oldOverride, displayCaptureFactoryOverride == &oldOverride);
    if (&oldOverride == displayCaptureFactoryOverride)
        displayCaptureFactoryOverride = nullptr;
}

DisplayCaptureFactory& RealtimeMediaSourceCenter::displayCaptureFactory()
{
    return displayCaptureFactoryOverride ? *displayCaptureFactoryOverride : RealtimeMediaSourceCenter::singleton().displayCaptureFactoryPrivate();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
