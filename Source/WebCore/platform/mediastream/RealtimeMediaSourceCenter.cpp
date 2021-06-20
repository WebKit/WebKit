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
#include <wtf/CallbackAggregator.h>
#include <wtf/SHA1.h>

namespace WebCore {

static const Seconds deviceChangeDebounceTimerInterval { 200_ms };

RealtimeMediaSourceCenter& RealtimeMediaSourceCenter::singleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<RealtimeMediaSourceCenter> center;
    return center;
}

RealtimeMediaSourceCenter::RealtimeMediaSourceCenter()
    : m_debounceTimer(RunLoop::main(), this, &RealtimeMediaSourceCenter::triggerDevicesChangedObservers)
{
    m_supportedConstraints.setSupportsEchoCancellation(true);
    m_supportedConstraints.setSupportsWidth(true);
    m_supportedConstraints.setSupportsHeight(true);
    m_supportedConstraints.setSupportsAspectRatio(true);
    m_supportedConstraints.setSupportsFrameRate(true);
    m_supportedConstraints.setSupportsFacingMode(true);
    m_supportedConstraints.setSupportsVolume(true);
    m_supportedConstraints.setSupportsDeviceId(true);
}

RealtimeMediaSourceCenter::~RealtimeMediaSourceCenter() = default;

RealtimeMediaSourceCenter::Observer::~Observer() = default;

void RealtimeMediaSourceCenter::createMediaStream(Ref<const Logger>&& logger, NewMediaStreamHandler&& completionHandler, String&& hashSalt, CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, const MediaStreamRequest& request)
{
    Vector<Ref<RealtimeMediaSource>> audioSources;
    Vector<Ref<RealtimeMediaSource>> videoSources;
    String invalidConstraint;

    RefPtr<RealtimeMediaSource> audioSource;
    if (audioDevice) {
        auto source = audioCaptureFactory().createAudioCaptureSource(WTFMove(audioDevice), String { hashSalt }, &request.audioConstraints);
        if (!source) {
            completionHandler(makeUnexpected(makeString("Failed to create MediaStream audio source: ", source.errorMessage)));
            return;
        }
        audioSource = source.source();
    }

    RefPtr<RealtimeMediaSource> videoSource;
    if (videoDevice) {
        CaptureSourceOrError source;
        if (videoDevice.type() == CaptureDevice::DeviceType::Camera)
            source = videoCaptureFactory().createVideoCaptureSource(WTFMove(videoDevice), WTFMove(hashSalt), &request.videoConstraints);
        else
            source = displayCaptureFactory().createDisplayCaptureSource(WTFMove(videoDevice), &request.videoConstraints);

        if (!source) {
            completionHandler(makeUnexpected(makeString("Failed to create MediaStream video source: ", source.errorMessage)));
            return;
        }
        videoSource = source.source();
    }

    CompletionHandler<void(String&&)> whenAudioSourceReady = [audioSource, videoSource = WTFMove(videoSource), logger = WTFMove(logger), completionHandler = WTFMove(completionHandler)](auto&& errorMessage) mutable {
        if (!errorMessage.isEmpty())
            return completionHandler(makeUnexpected(makeString("Failed to create MediaStream audio source: ", errorMessage)));
        if (!videoSource)
            return completionHandler(MediaStreamPrivate::create(WTFMove(logger), WTFMove(audioSource), WTFMove(videoSource)));

        CompletionHandler<void(String&&)> whenVideoSourceReady = [audioSource = WTFMove(audioSource), videoSource, logger = WTFMove(logger), completionHandler = WTFMove(completionHandler)](auto&& errorMessage) mutable {
            if (!errorMessage.isEmpty())
                return completionHandler(makeUnexpected(makeString("Failed to create MediaStream video source: ", errorMessage)));
            completionHandler(MediaStreamPrivate::create(WTFMove(logger), WTFMove(audioSource), WTFMove(videoSource)));
        };
        videoSource->whenReady(WTFMove(whenVideoSourceReady));
    };
    if (!audioSource)
        return whenAudioSourceReady({ });
    audioSource->whenReady(WTFMove(whenAudioSourceReady));
}

void RealtimeMediaSourceCenter::getMediaStreamDevices(CompletionHandler<void(Vector<CaptureDevice>&&)>&& completion)
{
    enumerateDevices(true, true, true, true, [this, completion = WTFMove(completion)]() mutable {
        Vector<CaptureDevice> results;

        results.appendVector(audioCaptureFactory().audioCaptureDeviceManager().captureDevices());
        results.appendVector(videoCaptureFactory().videoCaptureDeviceManager().captureDevices());
        results.appendVector(displayCaptureFactory().displayCaptureDeviceManager().captureDevices());
        results.appendVector(audioCaptureFactory().speakerDevices());

        completion(WTFMove(results));
    });
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
    sha1.addBytes(utf8.dataAsUInt8Ptr(), utf8.length() + 1); // Include terminating null byte.
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

void RealtimeMediaSourceCenter::addDevicesChangedObserver(Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.add(observer);
}

void RealtimeMediaSourceCenter::removeDevicesChangedObserver(Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.remove(observer);
}

void RealtimeMediaSourceCenter::captureDevicesChanged()
{
    ASSERT(isMainThread());

    // When a device with camera and microphone is attached or detached, the CaptureDevice notification for
    // the different devices won't arrive at the same time so delay a bit so we can coalesce the callbacks.
    if (!m_debounceTimer.isActive())
        m_debounceTimer.startOneShot(deviceChangeDebounceTimerInterval);
}

void RealtimeMediaSourceCenter::triggerDevicesChangedObservers()
{
    auto protectedThis = makeRef(*this);
    m_observers.forEach([](auto& observer) {
        observer.devicesChanged();
    });
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
        for (auto& device : audioCaptureFactory().audioCaptureDeviceManager().captureDevices()) {
            if (!device.enabled())
                continue;

            auto sourceOrError = audioCaptureFactory().createAudioCaptureSource(device, String { hashSalt }, { });
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.audioConstraints, invalidConstraint))
                audioDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});

            if (!invalidConstraint.isEmpty() && firstInvalidConstraint.isEmpty())
                firstInvalidConstraint = invalidConstraint;
        }
    }

    if (request.videoConstraints.isValid) {
        for (auto& device : videoCaptureFactory().videoCaptureDeviceManager().captureDevices()) {
            if (!device.enabled())
                continue;

            auto sourceOrError = videoCaptureFactory().createVideoCaptureSource(device, String { hashSalt }, { });
            if (sourceOrError && sourceOrError.captureSource->supportsConstraints(request.videoConstraints, invalidConstraint))
                videoDeviceInfo.append({sourceOrError.captureSource->fitnessScore(), device});

            if (!invalidConstraint.isEmpty() && firstInvalidConstraint.isEmpty())
                firstInvalidConstraint = invalidConstraint;
        }
    }
}

void RealtimeMediaSourceCenter::enumerateDevices(bool shouldEnumerateCamera, bool shouldEnumerateDisplay, bool shouldEnumerateMicrophone, bool shouldEnumerateSpeakers, CompletionHandler<void()>&& callback)
{
    auto callbackAggregator = CallbackAggregator::create(WTFMove(callback));
    if (shouldEnumerateCamera)
        videoCaptureFactory().videoCaptureDeviceManager().computeCaptureDevices([callbackAggregator] { });
    if (shouldEnumerateDisplay)
        displayCaptureFactory().displayCaptureDeviceManager().computeCaptureDevices([callbackAggregator] { });
    if (shouldEnumerateMicrophone)
        audioCaptureFactory().audioCaptureDeviceManager().computeCaptureDevices([callbackAggregator] { });
    if (shouldEnumerateSpeakers)
        audioCaptureFactory().computeSpeakerDevices([callbackAggregator] { });
}

void RealtimeMediaSourceCenter::validateRequestConstraints(ValidConstraintsHandler&& validHandler, InvalidConstraintsHandler&& invalidHandler, const MediaStreamRequest& request, String&& deviceIdentifierHashSalt)
{
    bool shouldEnumerateCamera = request.videoConstraints.isValid;
    bool shouldEnumerateDisplay = request.type == MediaStreamRequest::Type::DisplayMedia;
    bool shouldEnumerateMicrophone = request.audioConstraints.isValid;
    bool shouldEnumerateSpeakers = false;
    enumerateDevices(shouldEnumerateCamera, shouldEnumerateDisplay, shouldEnumerateMicrophone, shouldEnumerateSpeakers, [this, validHandler = WTFMove(validHandler), invalidHandler = WTFMove(invalidHandler), request, deviceIdentifierHashSalt = WTFMove(deviceIdentifierHashSalt)]() mutable {
        validateRequestConstraintsAfterEnumeration(WTFMove(validHandler), WTFMove(invalidHandler), request, WTFMove(deviceIdentifierHashSalt));
    });
}

void RealtimeMediaSourceCenter::validateRequestConstraintsAfterEnumeration(ValidConstraintsHandler&& validHandler, InvalidConstraintsHandler&& invalidHandler, const MediaStreamRequest& request, String&& deviceIdentifierHashSalt)
{
    struct {
        bool operator()(const DeviceInfo& a, const DeviceInfo& b)
        {
            return a.fitnessScore > b.fitnessScore;
        }
    } sortBasedOnFitnessScore;

    Vector<DeviceInfo> audioDeviceInfo;
    Vector<DeviceInfo> videoDeviceInfo;
    String firstInvalidConstraint;

    if (request.type == MediaStreamRequest::Type::DisplayMedia)
        getDisplayMediaDevices(request, videoDeviceInfo, firstInvalidConstraint);
    else
        getUserMediaDevices(request, String { deviceIdentifierHashSalt }, audioDeviceInfo, videoDeviceInfo, firstInvalidConstraint);

    if (request.audioConstraints.isValid && audioDeviceInfo.isEmpty()) {
        WTFLogAlways("Audio capture was requested but no device was found amongst %zu devices", audioCaptureFactory().audioCaptureDeviceManager().captureDevices().size());
        request.audioConstraints.mandatoryConstraints.forEach([](auto& constraint) { constraint.log(); });

        invalidHandler(firstInvalidConstraint);
        return;
    }

    if (request.videoConstraints.isValid && videoDeviceInfo.isEmpty()) {
        WTFLogAlways("Video capture was requested but no device was found amongst %zu devices", videoCaptureFactory().videoCaptureDeviceManager().captureDevices().size());
        request.videoConstraints.mandatoryConstraints.forEach([](auto& constraint) { constraint.log(); });

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

void RealtimeMediaSourceCenter::setAudioCaptureFactory(AudioCaptureFactory& factory)
{
    m_audioCaptureFactoryOverride = &factory;
}

void RealtimeMediaSourceCenter::unsetAudioCaptureFactory(AudioCaptureFactory& oldOverride)
{
    ASSERT_UNUSED(oldOverride, m_audioCaptureFactoryOverride == &oldOverride);
    if (&oldOverride == m_audioCaptureFactoryOverride)
        m_audioCaptureFactoryOverride = nullptr;
}

AudioCaptureFactory& RealtimeMediaSourceCenter::audioCaptureFactory()
{
    return m_audioCaptureFactoryOverride ? *m_audioCaptureFactoryOverride : defaultAudioCaptureFactory();
}

void RealtimeMediaSourceCenter::setVideoCaptureFactory(VideoCaptureFactory& factory)
{
    m_videoCaptureFactoryOverride = &factory;
}
void RealtimeMediaSourceCenter::unsetVideoCaptureFactory(VideoCaptureFactory& oldOverride)
{
    ASSERT_UNUSED(oldOverride, m_videoCaptureFactoryOverride == &oldOverride);
    if (&oldOverride == m_videoCaptureFactoryOverride)
        m_videoCaptureFactoryOverride = nullptr;
}

VideoCaptureFactory& RealtimeMediaSourceCenter::videoCaptureFactory()
{
    return m_videoCaptureFactoryOverride ? *m_videoCaptureFactoryOverride : defaultVideoCaptureFactory();
}

void RealtimeMediaSourceCenter::setDisplayCaptureFactory(DisplayCaptureFactory& factory)
{
    m_displayCaptureFactoryOverride = &factory;
}

void RealtimeMediaSourceCenter::unsetDisplayCaptureFactory(DisplayCaptureFactory& oldOverride)
{
    ASSERT_UNUSED(oldOverride, m_displayCaptureFactoryOverride == &oldOverride);
    if (&oldOverride == m_displayCaptureFactoryOverride)
        m_displayCaptureFactoryOverride = nullptr;
}

DisplayCaptureFactory& RealtimeMediaSourceCenter::displayCaptureFactory()
{
    return m_displayCaptureFactoryOverride ? *m_displayCaptureFactoryOverride : defaultDisplayCaptureFactory();
}

#if !PLATFORM(COCOA)
bool RealtimeMediaSourceCenter::shouldInterruptAudioOnPageVisibilityChange()
{
    return false;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
