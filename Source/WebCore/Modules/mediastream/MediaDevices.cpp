/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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
#include "MediaDevices.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaDeviceInfo.h"
#include "MediaTrackSupportedConstraints.h"
#include "RealtimeMediaSourceSettings.h"
#include "UserGestureIndicator.h"
#include "UserMediaController.h"
#include "UserMediaRequest.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/RandomNumber.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaDevices);

inline MediaDevices::MediaDevices(Document& document)
    : ActiveDOMObject(document)
    , m_scheduledEventTimer(*this, &MediaDevices::scheduledEventTimerFired)
    , m_eventNames(eventNames())
{
    suspendIfNeeded();

    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Monitor) == static_cast<size_t>(RealtimeMediaSourceSettings::DisplaySurfaceType::Monitor), "MediaDevices::DisplayCaptureSurfaceType::Monitor is not equal to RealtimeMediaSourceSettings::DisplaySurfaceType::Monitor as expected");
    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Window) == static_cast<size_t>(RealtimeMediaSourceSettings::DisplaySurfaceType::Window), "MediaDevices::DisplayCaptureSurfaceType::Window is not RealtimeMediaSourceSettings::DisplaySurfaceType::Window as expected");
    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Application) == static_cast<size_t>(RealtimeMediaSourceSettings::DisplaySurfaceType::Application), "MediaDevices::DisplayCaptureSurfaceType::Application is not RealtimeMediaSourceSettings::DisplaySurfaceType::Application as expected");
    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Browser) == static_cast<size_t>(RealtimeMediaSourceSettings::DisplaySurfaceType::Browser), "MediaDevices::DisplayCaptureSurfaceType::Browser is not RealtimeMediaSourceSettings::DisplaySurfaceType::Browser as expected");
}

MediaDevices::~MediaDevices() = default;

void MediaDevices::stop()
{
    if (m_deviceChangeToken) {
        auto* controller = UserMediaController::from(document()->page());
        if (controller)
            controller->removeDeviceChangeObserver(m_deviceChangeToken);
    }
    m_devices.clear();
    m_scheduledEventTimer.stop();
}

Ref<MediaDevices> MediaDevices::create(Document& document)
{
    return adoptRef(*new MediaDevices(document));
}

Document* MediaDevices::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

static MediaConstraints createMediaConstraints(const Variant<bool, MediaTrackConstraints>& constraints)
{
    return WTF::switchOn(constraints,
        [&] (bool isValid) {
            MediaConstraints constraints;
            constraints.isValid = isValid;
            return constraints;
        },
        [&] (const MediaTrackConstraints& trackConstraints) {
            return createMediaConstraints(trackConstraints);
        }
    );
}

bool MediaDevices::computeUserGesturePriviledge(GestureAllowedRequest requestType)
{
    auto* currentGestureToken = UserGestureIndicator::currentUserGesture().get();
    if (m_currentGestureToken.get() != currentGestureToken) {
        m_currentGestureToken = makeWeakPtr(currentGestureToken);
        m_requestTypesForCurrentGesture = { };
    }

    bool isUserGesturePriviledged = m_currentGestureToken && !m_requestTypesForCurrentGesture.contains(requestType);
    m_requestTypesForCurrentGesture.add(requestType);
    return isUserGesturePriviledged;
}

void MediaDevices::getUserMedia(const StreamConstraints& constraints, Promise&& promise)
{
    auto* document = this->document();
    if (!document || !document->isFullyActive()) {
        promise.reject(Exception { InvalidStateError, "Document is not fully active"_s });
        return;
    }

    auto audioConstraints = createMediaConstraints(constraints.audio);
    auto videoConstraints = createMediaConstraints(constraints.video);

    bool isUserGesturePriviledged = false;

    if (audioConstraints.isValid)
        isUserGesturePriviledged |= computeUserGesturePriviledge(GestureAllowedRequest::Microphone);

    if (videoConstraints.isValid) {
        isUserGesturePriviledged |= computeUserGesturePriviledge(GestureAllowedRequest::Camera);
        videoConstraints.setDefaultVideoConstraints();
    }

    auto request = UserMediaRequest::create(*document, { MediaStreamRequest::Type::UserMedia, WTFMove(audioConstraints), WTFMove(videoConstraints), isUserGesturePriviledged }, WTFMove(promise));
    request->start();
}

void MediaDevices::getDisplayMedia(const DisplayMediaStreamConstraints& constraints, Promise&& promise)
{
    auto* document = this->document();
    if (!document)
        return;

    bool isUserGesturePriviledged = computeUserGesturePriviledge(GestureAllowedRequest::Display);
    if (!isUserGesturePriviledged) {
        promise.reject(Exception { InvalidAccessError, "getDisplayMedia must be called from a user gesture handler."_s });
        return;
    }

    auto request = UserMediaRequest::create(*document, { MediaStreamRequest::Type::DisplayMedia, { }, createMediaConstraints(constraints.video), isUserGesturePriviledged }, WTFMove(promise));
    request->start();
}

static inline bool checkCameraAccess(const Document& document)
{
    return isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Camera, document, LogFeaturePolicyFailure::No);
}

static inline bool checkMicrophoneAccess(const Document& document)
{
    return isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Microphone, document, LogFeaturePolicyFailure::No);
}

void MediaDevices::refreshDevices(const Vector<CaptureDevice>& newDevices)
{
    auto* document = this->document();
    if (!document)
        return;

    bool canAccessCamera = checkCameraAccess(*document);
    bool canAccessMicrophone = checkMicrophoneAccess(*document);

    Vector<Ref<MediaDeviceInfo>> devices;
    for (auto& newDevice : newDevices) {
        if (!canAccessMicrophone && newDevice.type() == CaptureDevice::DeviceType::Microphone)
            continue;
        if (!canAccessCamera && newDevice.type() == CaptureDevice::DeviceType::Camera)
            continue;

        auto deviceKind = newDevice.type() == CaptureDevice::DeviceType::Microphone ? MediaDeviceInfo::Kind::Audioinput : MediaDeviceInfo::Kind::Videoinput;
        auto index = m_devices.findMatching([deviceKind, &newDevice](auto& oldDevice) {
            return oldDevice->deviceId() == newDevice.persistentId() && oldDevice->kind() == deviceKind;
        });
        if (index != notFound) {
            devices.append(m_devices[index].copyRef());
            continue;
        }

        devices.append(MediaDeviceInfo::create(newDevice.label(), newDevice.persistentId(), newDevice.groupId(), deviceKind));
    }
    m_devices = WTFMove(devices);
}

void MediaDevices::enumerateDevices(EnumerateDevicesPromise&& promise)
{
    auto* document = this->document();
    if (!document)
        return;

    auto* controller = UserMediaController::from(document->page());
    if (!controller) {
        promise.resolve({ });
        return;
    }

    if (!checkCameraAccess(*document) && !checkMicrophoneAccess(*document)) {
        controller->logEnumerateDevicesDenial(*document);
        promise.resolve({ });
        return;
    }

    controller->enumerateMediaDevices(*document, [this, weakThis = makeWeakPtr(this), promise = WTFMove(promise)](const auto& newDevices, const auto& deviceIDHashSalt) mutable {
        if (!weakThis || isContextStopped())
            return;

        this->document()->setDeviceIDHashSalt(deviceIDHashSalt);
        refreshDevices(newDevices);
        promise.resolve(m_devices);
    });
}

MediaTrackSupportedConstraints MediaDevices::getSupportedConstraints()
{
    auto& supported = RealtimeMediaSourceCenter::singleton().supportedConstraints();
    MediaTrackSupportedConstraints result;
    result.width = supported.supportsWidth();
    result.height = supported.supportsHeight();
    result.aspectRatio = supported.supportsAspectRatio();
    result.frameRate = supported.supportsFrameRate();
    result.facingMode = supported.supportsFacingMode();
    result.volume = supported.supportsVolume();
    result.sampleRate = supported.supportsSampleRate();
    result.sampleSize = supported.supportsSampleSize();
    result.echoCancellation = supported.supportsEchoCancellation();
    result.deviceId = supported.supportsDeviceId();
    result.groupId = supported.supportsGroupId();

    return result;
}

void MediaDevices::scheduledEventTimerFired()
{
    ASSERT(!isContextStopped());
    dispatchEvent(Event::create(eventNames().devicechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

bool MediaDevices::virtualHasPendingActivity() const
{
    return hasEventListeners(m_eventNames.devicechangeEvent);
}

const char* MediaDevices::activeDOMObjectName() const
{
    return "MediaDevices";
}

void MediaDevices::listenForDeviceChanges()
{
    auto* document = this->document();
    auto* controller = document ? UserMediaController::from(document->page()) : nullptr;
    if (!controller)
        return;

    bool canAccessCamera = isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Camera, *document, LogFeaturePolicyFailure::No);
    bool canAccessMicrophone = isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Microphone, *document, LogFeaturePolicyFailure::No);

    if (m_listeningForDeviceChanges || (!canAccessCamera && !canAccessMicrophone))
        return;

    m_listeningForDeviceChanges = true;

    m_deviceChangeToken = controller->addDeviceChangeObserver([weakThis = makeWeakPtr(*this), this]() {
        if (!weakThis || isContextStopped() || m_scheduledEventTimer.isActive())
            return;

        m_scheduledEventTimer.startOneShot(Seconds(randomNumber() / 2));
    });
}

bool MediaDevices::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (eventType == eventNames().devicechangeEvent)
        listenForDeviceChanges();

    return EventTargetWithInlineData::addEventListener(eventType, WTFMove(listener), options);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
