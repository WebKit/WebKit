/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#include "MediaDevicesRequest.h"
#include "MediaTrackSupportedConstraints.h"
#include "RealtimeMediaSourceSettings.h"
#include "RuntimeEnabledFeatures.h"
#include "UserMediaController.h"
#include "UserMediaRequest.h"
#include <wtf/RandomNumber.h>

namespace WebCore {

inline MediaDevices::MediaDevices(Document& document)
    : ActiveDOMObject(&document)
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
        auto* document = this->document();
        auto* controller = document ? UserMediaController::from(document->page()) : nullptr;
        if (document && controller)
            controller->removeDeviceChangeObserver(m_deviceChangeToken);
    }
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

void MediaDevices::getUserMedia(const StreamConstraints& constraints, Promise&& promise) const
{
    auto* document = this->document();
    if (!document) {
        promise.reject(Exception { InvalidStateError });
        return;
    }

    auto audioConstraints = createMediaConstraints(constraints.audio);
    auto videoConstraints = createMediaConstraints(constraints.video);
    if (videoConstraints.isValid)
        videoConstraints.setDefaultVideoConstraints();

    auto request = UserMediaRequest::create(*document, { MediaStreamRequest::Type::UserMedia, WTFMove(audioConstraints), WTFMove(videoConstraints) }, WTFMove(promise));
    if (request)
        request->start();

    return;
}

ExceptionOr<void> MediaDevices::getDisplayMedia(const StreamConstraints& constraints, Promise&& promise) const
{
    auto* document = this->document();
    if (!document)
        return Exception { InvalidStateError };

    auto request = UserMediaRequest::create(*document, { MediaStreamRequest::Type::DisplayMedia, { }, createMediaConstraints(constraints.video) }, WTFMove(promise));
    if (request)
        request->start();

    return { };
}

void MediaDevices::enumerateDevices(EnumerateDevicesPromise&& promise) const
{
    auto* document = this->document();
    if (!document)
        return;
    MediaDevicesRequest::create(*document, WTFMove(promise))->start();
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
    if (scriptExecutionContext())
        dispatchEvent(Event::create(eventNames().devicechangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

bool MediaDevices::hasPendingActivity() const
{
    return !isContextStopped() && hasEventListeners(m_eventNames.devicechangeEvent);
}

const char* MediaDevices::activeDOMObjectName() const
{
    return "MediaDevices";
}

bool MediaDevices::canSuspendForDocumentSuspension() const
{
    return true;
}

bool MediaDevices::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (!m_listeningForDeviceChanges && eventType == eventNames().devicechangeEvent) {
        auto* document = this->document();
        auto* controller = document ? UserMediaController::from(document->page()) : nullptr;
        if (document && controller) {
            m_listeningForDeviceChanges = true;

            m_deviceChangeToken = controller->addDeviceChangeObserver([weakThis = makeWeakPtr(*this), this]() {

                if (!weakThis || m_scheduledEventTimer.isActive())
                    return;

                m_scheduledEventTimer.startOneShot(Seconds(randomNumber() / 2));
            });
        }
    }

    return EventTargetWithInlineData::addEventListener(eventType, WTFMove(listener), options);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
