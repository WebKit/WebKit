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
#include "UserMediaRequest.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "DocumentLoader.h"
#include "ExceptionCode.h"
#include "JSMediaStream.h"
#include "JSOverconstrainedError.h"
#include "MainFrame.h"
#include "MediaConstraintsImpl.h"
#include "RealtimeMediaSourceCenter.h"
#include "Settings.h"
#include "UserMediaController.h"

namespace WebCore {

ExceptionOr<void> UserMediaRequest::start(Document& document, Ref<MediaConstraintsImpl>&& audioConstraints, Ref<MediaConstraintsImpl>&& videoConstraints, DOMPromise<IDLInterface<MediaStream>>&& promise)
{
    auto* userMedia = UserMediaController::from(document.page());
    if (!userMedia)
        return Exception { NOT_SUPPORTED_ERR }; // FIXME: Why is it better to return an exception here instead of rejecting the promise as we do just below?

    if (!audioConstraints->isValid() && !videoConstraints->isValid()) {
        promise.reject(TypeError);
        return { };
    }

    adoptRef(*new UserMediaRequest(document, *userMedia, WTFMove(audioConstraints), WTFMove(videoConstraints), WTFMove(promise)))->start();
    return { };
}

UserMediaRequest::UserMediaRequest(Document& document, UserMediaController& controller, Ref<MediaConstraintsImpl>&& audioConstraints, Ref<MediaConstraintsImpl>&& videoConstraints, DOMPromise<IDLInterface<MediaStream>>&& promise)
    : ContextDestructionObserver(&document)
    , m_audioConstraints(WTFMove(audioConstraints))
    , m_videoConstraints(WTFMove(videoConstraints))
    , m_controller(&controller)
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
    return &m_scriptExecutionContext->topOrigin();
}

static bool isSecure(DocumentLoader& documentLoader)
{
    auto& response = documentLoader.response();
    return response.url().protocolIs("https")
        && response.certificateInfo()
        && !response.certificateInfo()->containsNonRootSHA1SignedCertificate();
}

static bool canCallGetUserMedia(Document& document, String& errorMessage)
{
    bool requiresSecureConnection = document.settings().mediaCaptureRequiresSecureConnection();
    if (requiresSecureConnection && !isSecure(*document.loader())) {
        errorMessage = "Trying to call getUserMedia from an insecure document.";
        return false;
    }

    auto& topDocument = document.topDocument();
    if (&document != &topDocument) {
        auto& topOrigin = topDocument.topOrigin();

        if (!document.securityOrigin().isSameSchemeHostPort(topOrigin)) {
            errorMessage = "Trying to call getUserMedia from a document with a different security origin than its top-level frame.";
            return false;
        }

        for (auto* ancestorDocument = document.parentDocument(); ancestorDocument != &topDocument; ancestorDocument = ancestorDocument->parentDocument()) {
            if (requiresSecureConnection && !isSecure(*ancestorDocument->loader())) {
                errorMessage = "Trying to call getUserMedia from a document with an insecure parent frame.";
                return false;
            }

            if (!ancestorDocument->securityOrigin().isSameSchemeHostPort(topOrigin)) {
                errorMessage = "Trying to call getUserMedia from a document with a different security origin than its top-level frame.";
                return false;
            }
        }
    }
    
    return true;
}

void UserMediaRequest::start()
{
    if (!m_scriptExecutionContext || !m_controller) {
        deny(MediaAccessDenialReason::OtherFailure, emptyString());
        return;
    }

    Document& document = downcast<Document>(*m_scriptExecutionContext);

    // 10.2 - 6.3 Optionally, e.g., based on a previously-established user preference, for security reasons,
    // or due to platform limitations, jump to the step labeled Permission Failure below.
    String errorMessage;
    if (!canCallGetUserMedia(document, errorMessage)) {
        deny(MediaAccessDenialReason::PermissionDenied, emptyString());
        document.domWindow()->printErrorMessage(errorMessage);
        return;
    }

    m_controller->requestUserMediaAccess(*this);
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

        for (auto& track : stream->getAudioTracks())
            track->source().startProducingData();

        for (auto& track : stream->getVideoTracks())
            track->source().startProducingData();
        
        m_promise.resolve(stream);
    };

    RealtimeMediaSourceCenter::singleton().createMediaStream(WTFMove(callback), m_allowedAudioDeviceUID, m_allowedVideoDeviceUID, &m_audioConstraints.get(), &m_videoConstraints.get());
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
        m_promise.rejectType<IDLInterface<OverconstrainedError>>(OverconstrainedError::create(invalidConstraint, ASCIILiteral("Invalid constraint")).get());
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
    ContextDestructionObserver::contextDestroyed();
    Ref<UserMediaRequest> protectedThis(*this);
    if (m_controller) {
        m_controller->cancelUserMediaAccessRequest(*this);
        m_controller = nullptr;
    }
}

Document* UserMediaRequest::document() const
{
    return downcast<Document>(m_scriptExecutionContext);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
