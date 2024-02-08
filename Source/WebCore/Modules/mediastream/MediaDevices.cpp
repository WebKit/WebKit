/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#include "AudioSession.h"
#include "CaptureDeviceWithCapabilities.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "FrameDestructionObserverInlines.h"
#include "JSDOMPromiseDeferred.h"
#include "JSInputDeviceInfo.h"
#include "JSMediaDeviceInfo.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MediaTrackSupportedConstraints.h"
#include "RealtimeMediaSourceSettings.h"
#include "Settings.h"
#include "UserGestureIndicator.h"
#include "UserMediaController.h"
#include "UserMediaRequest.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaDevices);

inline MediaDevices::MediaDevices(Document& document)
    : ActiveDOMObject(document)
    , m_scheduledEventTimer(RunLoop::main(), this, &MediaDevices::scheduledEventTimerFired)
    , m_eventNames(eventNames())
    , m_groupIdHashSalt(createVersion4UUIDString())
{
    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Monitor) == static_cast<size_t>(DisplaySurfaceType::Monitor), "MediaDevices::DisplayCaptureSurfaceType::Monitor is not equal to DisplaySurfaceType::Monitor as expected");
    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Window) == static_cast<size_t>(DisplaySurfaceType::Window), "MediaDevices::DisplayCaptureSurfaceType::Window is not DisplaySurfaceType::Window as expected");
    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Application) == static_cast<size_t>(DisplaySurfaceType::Application), "MediaDevices::DisplayCaptureSurfaceType::Application is not DisplaySurfaceType::Application as expected");
    static_assert(static_cast<size_t>(MediaDevices::DisplayCaptureSurfaceType::Browser) == static_cast<size_t>(DisplaySurfaceType::Browser), "MediaDevices::DisplayCaptureSurfaceType::Browser is not DisplaySurfaceType::Browser as expected");
}

MediaDevices::~MediaDevices() = default;

void MediaDevices::stop()
{
    if (m_deviceChangeToken) {
        auto* controller = UserMediaController::from(document()->page());
        if (controller)
            controller->removeDeviceChangeObserver(m_deviceChangeToken);
    }
    m_scheduledEventTimer.stop();
}

Ref<MediaDevices> MediaDevices::create(Document& document)
{
    auto result = adoptRef(*new MediaDevices(document));
    result->suspendIfNeeded();
    return result;
}

Document* MediaDevices::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

static MediaConstraints createMediaConstraints(const std::variant<bool, MediaTrackConstraints>& constraints)
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
        m_currentGestureToken = currentGestureToken;
        m_requestTypesForCurrentGesture = { };
    }

    bool isUserGesturePriviledged = m_currentGestureToken && !m_requestTypesForCurrentGesture.contains(requestType);
    m_requestTypesForCurrentGesture.add(requestType);
    return isUserGesturePriviledged;
}

void MediaDevices::getUserMedia(StreamConstraints&& constraints, Promise&& promise)
{
    auto audioConstraints = createMediaConstraints(constraints.audio);
    auto videoConstraints = createMediaConstraints(constraints.video);

    if (!audioConstraints.isValid && !videoConstraints.isValid) {
        promise.reject(ExceptionCode::TypeError, "No constraints provided"_s);
        return;
    }

    RefPtr document = this->document();
    if (!document || !document->isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Document is not fully active"_s });
        return;
    }

#if USE(AUDIO_SESSION)
    if (audioConstraints.isValid) {
        auto categoryOverride = AudioSession::sharedSession().categoryOverride();
        if (categoryOverride != AudioSessionCategory::None && categoryOverride != AudioSessionCategory::PlayAndRecord)  {
            promise.reject(Exception { ExceptionCode::InvalidStateError, "AudioSession category is not compatible with audio capture."_s });
            return;
        }
    }
#endif

    bool isUserGesturePriviledged = false;

    if (audioConstraints.isValid) {
        isUserGesturePriviledged |= computeUserGesturePriviledge(GestureAllowedRequest::Microphone);
        audioConstraints.setDefaultAudioConstraints();
    }
    if (videoConstraints.isValid) {
        isUserGesturePriviledged |= computeUserGesturePriviledge(GestureAllowedRequest::Camera);
        videoConstraints.setDefaultVideoConstraints();
    }

    auto request = UserMediaRequest::create(*document, { MediaStreamRequest::Type::UserMedia, WTFMove(audioConstraints), WTFMove(videoConstraints), isUserGesturePriviledged, *document->pageID() }, WTFMove(constraints.audio), WTFMove(constraints.video), WTFMove(promise));

    if (!document->settings().getUserMediaRequiresFocus()) {
        request->start();
        return;
    }

    // FIXME: We use hidden while the spec is using focus, let's revisit when when spec is made clearer.
    document->whenVisible([request = WTFMove(request)] {
        if (request->isContextStopped())
            return;
        request->start();
    });
}

static bool hasInvalidGetDisplayMediaConstraint(const MediaConstraints& constraints)
{
    // https://w3c.github.io/mediacapture-screen-share/#navigator-additions
    // 1. Let constraints be the method's first argument.
    // 2. For each member present in constraints whose value, value, is a dictionary, run the following steps:
    //     1. If value contains a member named advanced, return a promise rejected with a newly created TypeError.
    //     2. If value contains a member which in turn is a dictionary containing a member named either min or
    //        exact, return a promise rejected with a newly created TypeError.
    if (!constraints.isValid)
        return true;

    if (!constraints.advancedConstraints.isEmpty())
        return true;

    bool invalid = false;
    constraints.mandatoryConstraints.filter([&invalid] (auto constraintType, const MediaConstraint& constraint) mutable {
        switch (constraintType) {
        case MediaConstraintType::Width:
        case MediaConstraintType::Height: {
            auto& intConstraint = downcast<IntConstraint>(constraint);
            int value;
            invalid = intConstraint.getExact(value) || intConstraint.getMin(value);
            break;
        }

        case MediaConstraintType::AspectRatio:
        case MediaConstraintType::FrameRate: {
            auto& doubleConstraint = downcast<DoubleConstraint>(constraint);
            double value;
            invalid = doubleConstraint.getExact(value) || doubleConstraint.getMin(value);
            break;
        }

        case MediaConstraintType::DisplaySurface:
        case MediaConstraintType::LogicalSurface: {
            auto& boolConstraint = downcast<BooleanConstraint>(constraint);
            bool value;
            invalid = boolConstraint.getExact(value);
            break;
        }

        case MediaConstraintType::FacingMode:
        case MediaConstraintType::DeviceId:
        case MediaConstraintType::GroupId: {
            auto& stringConstraint = downcast<StringConstraint>(constraint);
            Vector<String> values;
            invalid = stringConstraint.getExact(values);
            break;
        }

        case MediaConstraintType::SampleRate:
        case MediaConstraintType::SampleSize:
        case MediaConstraintType::Volume:
        case MediaConstraintType::EchoCancellation:
        case MediaConstraintType::FocusDistance:
        case MediaConstraintType::WhiteBalanceMode:
        case MediaConstraintType::Zoom:
        case MediaConstraintType::Torch:
            // Ignored.
            break;

        case MediaConstraintType::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }

        return invalid;
    });

    return invalid;
}

void MediaDevices::getDisplayMedia(DisplayMediaStreamConstraints&& constraints, Promise&& promise)
{
    RefPtr document = this->document();
    if (!document)
        return;

    bool isUserGesturePriviledged = computeUserGesturePriviledge(GestureAllowedRequest::Display);
    if (!isUserGesturePriviledged) {
        promise.reject(Exception { ExceptionCode::InvalidAccessError, "getDisplayMedia must be called from a user gesture handler."_s });
        return;
    }

    auto videoConstraints = createMediaConstraints(constraints.video);
    if (hasInvalidGetDisplayMediaConstraint(videoConstraints)) {
        promise.reject(Exception { ExceptionCode::TypeError, "getDisplayMedia must be called with valid constraints."_s });
        return;
    }

    // FIXME: We use hidden while the spec is using focus, let's revisit when when spec is made clearer.
    if (!document->isFullyActive() || document->topDocument().hidden()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Document is not fully active or does not have focus"_s });
        return;
    }

    auto request = UserMediaRequest::create(*document, { MediaStreamRequest::Type::DisplayMedia, { }, WTFMove(videoConstraints), isUserGesturePriviledged, *document->pageID() }, WTFMove(constraints.audio), WTFMove(constraints.video), WTFMove(promise));
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

static inline bool checkSpeakerAccess(const Document& document)
{
    return document.frame()
        && document.frame()->settings().exposeSpeakersEnabled()
        && isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::SpeakerSelection, document, LogFeaturePolicyFailure::No);
}

void MediaDevices::exposeDevices(Vector<CaptureDeviceWithCapabilities>&& newDevices, MediaDeviceHashSalts&& deviceIDHashSalts, EnumerateDevicesPromise&& promise)
{
    if (isContextStopped())
        return;

    auto& document = *this->document();

    bool canAccessCamera = checkCameraAccess(document);
    bool canAccessMicrophone = checkMicrophoneAccess(document);
    bool canAccessSpeaker = checkSpeakerAccess(document);

    m_audioOutputDeviceIdToPersistentId.clear();

    Vector<std::variant<RefPtr<MediaDeviceInfo>, RefPtr<InputDeviceInfo>>> devices;
    for (auto& newDeviceWithCapabilities : newDevices) {
        auto& newDevice = newDeviceWithCapabilities.device;
        if (!canAccessMicrophone && newDevice.type() == CaptureDevice::DeviceType::Microphone)
            continue;
        if (!canAccessCamera && newDevice.type() == CaptureDevice::DeviceType::Camera)
            continue;
        if (!canAccessSpeaker && newDevice.type() == CaptureDevice::DeviceType::Speaker)
            continue;

        auto& center = RealtimeMediaSourceCenter::singleton();
        String deviceId;
        if (newDevice.isEphemeral())
            deviceId = center.hashStringWithSalt(newDevice.persistentId(), deviceIDHashSalts.ephemeralDeviceSalt);
        else
            deviceId = center.hashStringWithSalt(newDevice.persistentId(), deviceIDHashSalts.persistentDeviceSalt);
        auto groupId = hashedGroupId(newDevice.groupId());

        if (newDevice.type() == CaptureDevice::DeviceType::Speaker) {
            m_audioOutputDeviceIdToPersistentId.add(deviceId, newDevice.persistentId());
            devices.append(RefPtr<MediaDeviceInfo> { MediaDeviceInfo::create(newDevice.label(), WTFMove(deviceId), WTFMove(groupId), toMediaDeviceInfoKind(newDevice.type())) });
        } else
            devices.append(RefPtr<InputDeviceInfo> { InputDeviceInfo::create(WTFMove(newDeviceWithCapabilities), WTFMove(deviceId), WTFMove(groupId)) });
    }
    promise.resolve(WTFMove(devices));
}

String MediaDevices::hashedGroupId(const String& groupId)
{
    return RealtimeMediaSourceCenter::singleton().hashStringWithSalt(groupId, m_groupIdHashSalt);
}

void MediaDevices::enumerateDevices(EnumerateDevicesPromise&& promise)
{
    RefPtr document = this->document();
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

    controller->enumerateMediaDevices(*document, [this, weakThis = WeakPtr { *this }, promise = WTFMove(promise)](Vector<CaptureDeviceWithCapabilities>&& newDevices, MediaDeviceHashSalts&& deviceIDHashSalts) mutable {
        if (!weakThis)
            return;
        exposeDevices(WTFMove(newDevices), WTFMove(deviceIDHashSalts), WTFMove(promise));
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
    result.whiteBalanceMode = supported.supportsWhiteBalanceMode();
    result.volume = supported.supportsVolume();
    result.sampleRate = supported.supportsSampleRate();
    result.sampleSize = supported.supportsSampleSize();
    result.echoCancellation = supported.supportsEchoCancellation();
    result.deviceId = supported.supportsDeviceId();
    result.groupId = supported.supportsGroupId();
    result.displaySurface = supported.supportsDisplaySurface();
    result.zoom = supported.supportsZoom();

    return result;
}

void MediaDevices::scheduledEventTimerFired()
{
    RefPtr document = this->document();
    if (!document)
        return;

    document->whenVisible([protectedThis = makePendingActivity(*this), this] {
        queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().devicechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
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
    RefPtr document = this->document();
    auto* controller = document ? UserMediaController::from(document->page()) : nullptr;
    if (!controller)
        return;

    bool canAccessCamera = isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Camera, *document, LogFeaturePolicyFailure::No);
    bool canAccessMicrophone = isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type::Microphone, *document, LogFeaturePolicyFailure::No);

    if (m_listeningForDeviceChanges || (!canAccessCamera && !canAccessMicrophone))
        return;

    m_listeningForDeviceChanges = true;

    m_deviceChangeToken = controller->addDeviceChangeObserver([weakThis = WeakPtr { *this }, this]() {
        if (!weakThis || isContextStopped() || m_scheduledEventTimer.isActive())
            return;

        m_scheduledEventTimer.startOneShot(Seconds(cryptographicallyRandomUnitInterval() / 2));
    });
}

bool MediaDevices::addEventListener(const AtomString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (eventType == eventNames().devicechangeEvent)
        listenForDeviceChanges();

    return EventTarget::addEventListener(eventType, WTFMove(listener), options);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
