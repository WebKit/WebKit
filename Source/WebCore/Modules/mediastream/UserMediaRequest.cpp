/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#include "UserMediaRequest.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "JSMediaStream.h"
#include "JSOverconstrainedError.h"
#include "MainFrame.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "OverconstrainedError.h"
#include "RealtimeMediaSourceCenter.h"
#include "SecurityOrigin.h"
#include "UserMediaController.h"
#include <wtf/MainThread.h>

namespace WebCore {

void UserMediaRequest::start(Document* document, Ref<MediaConstraintsImpl>&& audioConstraints, Ref<MediaConstraintsImpl>&& videoConstraints, MediaDevices::Promise&& promise, ExceptionCode& ec)
{
    UserMediaController* userMedia = UserMediaController::from(document ? document->page() : nullptr);
    if (!userMedia) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    if (!audioConstraints->isValid() && !videoConstraints->isValid()) {
        promise.reject(TypeError);
        return;
    }

    auto request = adoptRef(*new UserMediaRequest(document, userMedia, WTFMove(audioConstraints), WTFMove(videoConstraints), WTFMove(promise)));
    request->start();
}

UserMediaRequest::UserMediaRequest(ScriptExecutionContext* context, UserMediaController* controller, Ref<MediaConstraintsImpl>&& audioConstraints, Ref<MediaConstraintsImpl>&& videoConstraints, MediaDevices::Promise&& promise)
    : ContextDestructionObserver(context)
    , m_audioConstraints(WTFMove(audioConstraints))
    , m_videoConstraints(WTFMove(videoConstraints))
    , m_controller(controller)
    , m_promise(WTFMove(promise))
{
}

UserMediaRequest::~UserMediaRequest()
{
}

SecurityOrigin* UserMediaRequest::userMediaDocumentOrigin() const
{
    if (!m_scriptExecutionContext)
        return nullptr;

    return m_scriptExecutionContext->securityOrigin();
}

SecurityOrigin* UserMediaRequest::topLevelDocumentOrigin() const
{
    if (!m_scriptExecutionContext)
        return nullptr;

    return m_scriptExecutionContext->topOrigin();
}

void UserMediaRequest::start()
{
    if (m_controller)
        m_controller->requestUserMediaAccess(*this);
    else
        deny(MediaAccessDenialReason::OtherFailure, emptyString());
}

void UserMediaRequest::allow(const String& audioDeviceUID, const String& videoDeviceUID)
{
    m_allowedAudioDeviceUID = audioDeviceUID;
    m_allowedVideoDeviceUID = videoDeviceUID;

    RefPtr<UserMediaRequest> protectedThis = this;
    RealtimeMediaSourceCenter::NewMediaStreamHandler callback = [this, protectedThis = WTFMove(protectedThis)](RefPtr<MediaStreamPrivate>&& privateStream) mutable {
        if (!m_scriptExecutionContext)
            return;

        if (!privateStream) {
            deny(MediaAccessDenialReason::HardwareError, emptyString());
            return;
        }

        auto stream = MediaStream::create(*m_scriptExecutionContext, WTFMove(privateStream));
        if (stream->getTracks().isEmpty()) {
            deny(MediaAccessDenialReason::HardwareError, emptyString());
            return;
        }

        for (auto& track : stream->getAudioTracks()) {
            track->applyConstraints(m_audioConstraints);
            track->source().startProducingData();
        }

        for (auto& track : stream->getVideoTracks()) {
            track->applyConstraints(m_videoConstraints);
            track->source().startProducingData();
        }
        
        m_promise.resolve(stream);
    };

    RealtimeMediaSourceCenter::singleton().createMediaStream(WTFMove(callback), m_allowedAudioDeviceUID, m_allowedVideoDeviceUID);
}

void UserMediaRequest::deny(MediaAccessDenialReason reason, const String& invalidConstraint)
{
    if (!m_scriptExecutionContext)
        return;

    switch (reason) {
    case MediaAccessDenialReason::NoConstraints:
        m_promise.reject(TypeError);
        break;
    case MediaAccessDenialReason::UserMediaDisabled:
        m_promise.reject(SECURITY_ERR);
        break;
    case MediaAccessDenialReason::NoCaptureDevices:
        m_promise.reject(NOT_FOUND_ERR);
        break;
    case MediaAccessDenialReason::InvalidConstraint:
        m_promise.reject(OverconstrainedError::create(invalidConstraint, ASCIILiteral("Invalid constraint")).get());
        break;
    case MediaAccessDenialReason::HardwareError:
        m_promise.reject(NotReadableError);
        break;
    case MediaAccessDenialReason::OtherFailure:
        m_promise.reject(ABORT_ERR);
        break;
    case MediaAccessDenialReason::PermissionDenied:
        m_promise.reject(NotAllowedError);
        break;
    }
}

void UserMediaRequest::contextDestroyed()
{
    Ref<UserMediaRequest> protectedThis(*this);

    if (m_controller) {
        m_controller->cancelUserMediaAccessRequest(*this);
        m_controller = nullptr;
    }

    ContextDestructionObserver::contextDestroyed();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
