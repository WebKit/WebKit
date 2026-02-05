/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
#include "UserMediaRequest.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSession.h"
#include "ContextDestructionObserverInlines.h"
#include "DocumentPage.h"
#include "ExceptionCode.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaStream.h"
#include "JSOverconstrainedError.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MediaDevices.h"
#include "Navigator.h"
#include "NavigatorMediaDevices.h"
#include "PermissionsPolicy.h"
#include "PlatformMediaSessionManager.h"
#include "RTCController.h"
#include "RealtimeMediaSourceCenter.h"
#include "Settings.h"
#include "UserMediaController.h"
#include "WindowEventLoop.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <algorithm>
#include <wtf/Scope.h>

namespace WebCore {

Ref<UserMediaRequest> UserMediaRequest::create(Document& document, MediaStreamRequest&& request, TrackConstraints&& audioConstraints, TrackConstraints&& videoConstraints, DOMPromiseDeferred<IDLInterface<MediaStream>>&& promise)
{
    auto result = adoptRef(*new UserMediaRequest(document, WTF::move(request), WTF::move(audioConstraints), WTF::move(videoConstraints), WTF::move(promise)));
    result->suspendIfNeeded();
    return result;
}

UserMediaRequest::UserMediaRequest(Document& document, MediaStreamRequest&& request, TrackConstraints&& audioConstraints, TrackConstraints&& videoConstraints, DOMPromiseDeferred<IDLInterface<MediaStream>>&& promise)
    : ActiveDOMObject(document)
    , m_promise(makeUniqueRef<DOMPromiseDeferred<IDLInterface<MediaStream>>>(WTF::move(promise)))
    , m_request(WTF::move(request))
    , m_audioConstraints(WTF::move(audioConstraints))
    , m_videoConstraints(WTF::move(videoConstraints))
{
}

UserMediaRequest::~UserMediaRequest()
{
    if (auto completionHandler = std::exchange(m_allowCompletionHandler, { }))
        completionHandler();
}

SecurityOrigin* UserMediaRequest::userMediaDocumentOrigin() const
{
    RefPtr context = scriptExecutionContext();
    return context ? context->securityOrigin() : nullptr;
}

SecurityOrigin* UserMediaRequest::topLevelDocumentOrigin() const
{
    RefPtr context = scriptExecutionContext();
    return context ? &context->topOrigin() : nullptr;
}

void UserMediaRequest::start()
{
    RefPtr context = scriptExecutionContext();
    ASSERT(context);
    if (!context) {
        deny(MediaAccessDenialReason::UserMediaDisabled);
        return;
    }

    // 4. If the current settings object's responsible document is NOT allowed to use the feature indicated by
    //    attribute name allowusermedia, return a promise rejected with a DOMException object whose name
    //    attribute has the value SecurityError.
    Ref document = downcast<Document>(*context);
    auto* controller = UserMediaController::from(protect(document->page()).get());
    if (!controller) {
        deny(MediaAccessDenialReason::UserMediaDisabled);
        return;
    }

    // 6.3 Optionally, e.g., based on a previously-established user preference, for security reasons,
    //     or due to platform limitations, jump to the step labeled Permission Failure below.
    // ...
    // 6.10 Permission Failure: Reject p with a new DOMException object whose name attribute has
    //      the value NotAllowedError.

    switch (m_request.type) {
    case MediaStreamRequest::Type::DisplayMedia:
    case MediaStreamRequest::Type::DisplayMediaWithAudio:
        if (!PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DisplayCapture, document.get())) {
            deny(MediaAccessDenialReason::PermissionDenied);
            controller->logGetDisplayMediaDenial(document.get());
            return;
        }
        break;
    case MediaStreamRequest::Type::UserMedia:
        if (m_request.audioConstraints.isValid && !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Microphone, document.get())) {
            deny(MediaAccessDenialReason::PermissionDenied);
            controller->logGetUserMediaDenial(document.get());
            return;
        }
        if (m_request.videoConstraints.isValid && !PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::Camera, document.get())) {
            deny(MediaAccessDenialReason::PermissionDenied);
            controller->logGetUserMediaDenial(document.get());
            return;
        }
        break;
    }

    controller->requestUserMediaAccess(*this);
}

static inline bool isMediaStreamCorrectlyStarted(const MediaStream& stream)
{
    if (stream.getTracks().isEmpty())
        return false;

    return std::ranges::all_of(stream.getTracks(), [](auto& track) {
        return !track->source().captureDidFail();
    });
}

void UserMediaRequest::allow(CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, MediaDeviceHashSalts&& deviceIdentifierHashSalt, CompletionHandler<void()>&& completionHandler)
{
    RELEASE_LOG(MediaStream, "UserMediaRequest::allow %s %s", audioDevice ? audioDevice.persistentId().utf8().data() : "", videoDevice ? videoDevice.persistentId().utf8().data() : "");

    Ref document = downcast<Document>(*scriptExecutionContext());
    RefPtr localWindow = document->window();
    RefPtr mediaDevices = localWindow ? NavigatorMediaDevices::mediaDevices(localWindow->protectedNavigator()) : nullptr;
    if (mediaDevices)
        mediaDevices->willStartMediaCapture(!!audioDevice, !!videoDevice);

    m_allowCompletionHandler = WTF::move(completionHandler);
    queueTaskKeepingObjectAlive(*this, TaskSource::UserInteraction, [audioDevice = WTF::move(audioDevice), videoDevice = WTF::move(videoDevice), deviceIdentifierHashSalt = WTF::move(deviceIdentifierHashSalt)](auto& request) mutable {
        auto callback = [protectedThis = Ref { request }, protector = request.makePendingActivity(request)](auto privateStreamOrError) mutable {
            auto scopeExit = makeScopeExit([completionHandler = WTF::move(protectedThis->m_allowCompletionHandler)]() mutable {
                completionHandler();
            });
            if (protectedThis->isContextStopped()) {
                if (!!privateStreamOrError) {
                    RELEASE_LOG(MediaStream, "UserMediaRequest::allow, context is stopped");
                    privateStreamOrError.value()->forEachTrack([](auto& track) {
                        track.endTrack();
                    });
                }
                return;
            }

            if (!privateStreamOrError) {
                RELEASE_LOG(MediaStream, "UserMediaRequest::allow failed to create media stream!");
                auto error = privateStreamOrError.error();
                protectedThis->scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, error.errorMessage);
                protectedThis->deny(error.denialReason, error.errorMessage, error.invalidConstraint);
                return;
            }
            auto privateStream = WTF::move(privateStreamOrError).value();

            auto& document = downcast<Document>(*protectedThis->scriptExecutionContext());
            privateStream->monitorOrientation(document.orientationNotifier());

            Ref stream = MediaStream::create(document, WTF::move(privateStream));
            stream->startProducingData();

            if (!isMediaStreamCorrectlyStarted(stream)) {
                protectedThis->deny(MediaAccessDenialReason::HardwareError);
                return;
            }

            if (RefPtr audioTrack = stream->getFirstAudioTrack()) {
#if USE(AUDIO_SESSION)
                AudioSession::singleton().tryToSetActive(true);
#endif
                if (std::holds_alternative<MediaTrackConstraints>(protectedThis->m_audioConstraints))
                    audioTrack->setConstraints(std::get<MediaTrackConstraints>(WTF::move(protectedThis->m_audioConstraints)));
            }
            if (RefPtr videoTrack = stream->getFirstVideoTrack()) {
                if (std::holds_alternative<MediaTrackConstraints>(protectedThis->m_videoConstraints))
                    videoTrack->setConstraints(std::get<MediaTrackConstraints>(WTF::move(protectedThis->m_videoConstraints)));
            }

            ASSERT(document.isCapturing());
            document.setHasCaptureMediaStreamTrack();
            protectedThis->m_promise->resolve(WTF::move(stream));
        };

        auto& document = downcast<Document>(*request.scriptExecutionContext());
        RealtimeMediaSourceCenter::singleton().createMediaStream(document.logger(), WTF::move(callback), WTF::move(deviceIdentifierHashSalt), WTF::move(audioDevice), WTF::move(videoDevice), request.m_request);

        if (!request.scriptExecutionContext())
            return;

#if ENABLE(WEB_RTC)
        if (RefPtr page = document.page())
            page->rtcController().disableICECandidateFilteringForDocument(document);
#endif
    });
}

void UserMediaRequest::deny(MediaAccessDenialReason reason, const String& message, MediaConstraintType invalidConstraint)
{
    if (!scriptExecutionContext())
        return;

    ExceptionCode code;
    switch (reason) {
    case MediaAccessDenialReason::NoReason:
        ASSERT_NOT_REACHED();
        code = ExceptionCode::AbortError;
        break;
    case MediaAccessDenialReason::NoConstraints:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - no constraints");
        code = ExceptionCode::TypeError;
        break;
    case MediaAccessDenialReason::UserMediaDisabled:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - user media disabled");
        code = ExceptionCode::SecurityError;
        break;
    case MediaAccessDenialReason::NoCaptureDevices:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - no capture devices");
        code = ExceptionCode::NotFoundError;
        break;
    case MediaAccessDenialReason::InvalidConstraint:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - invalid constraint - %d", (int)invalidConstraint);
        m_promise->rejectType<IDLInterface<OverconstrainedError>>(OverconstrainedError::create(invalidConstraint, "Invalid constraint"_s).get());
        return;
    case MediaAccessDenialReason::HardwareError:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - hardware error");
        code = ExceptionCode::NotReadableError;
        break;
    case MediaAccessDenialReason::OtherFailure:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - other failure");
        code = ExceptionCode::AbortError;
        break;
    case MediaAccessDenialReason::PermissionDenied:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - permission denied");
        code = ExceptionCode::NotAllowedError;
        break;
    case MediaAccessDenialReason::InvalidAccess:
        RELEASE_LOG(MediaStream, "UserMediaRequest::deny - invalid access");
        code = ExceptionCode::InvalidAccessError;
        break;
    }

    if (!message.isEmpty())
        m_promise->reject(code, message);
    else
        m_promise->reject(code);
}

void UserMediaRequest::stop()
{
    Ref document = downcast<Document>(*scriptExecutionContext());
    if (auto* controller = UserMediaController::from(protect(document->page()).get()))
        controller->cancelUserMediaAccessRequest(*this);
}

Document* UserMediaRequest::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
