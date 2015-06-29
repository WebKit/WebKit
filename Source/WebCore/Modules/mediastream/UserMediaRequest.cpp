/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "Dictionary.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "JSMediaStream.h"
#include "JSNavigatorUserMediaError.h"
#include "MediaConstraintsImpl.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "NavigatorUserMediaErrorCallback.h"
#include "NavigatorUserMediaSuccessCallback.h"
#include "RealtimeMediaSourceCenter.h"
#include "SecurityOrigin.h"
#include "UserMediaController.h"
#include <wtf/MainThread.h>

namespace WebCore {

static RefPtr<MediaConstraints> parseOptions(const Dictionary& options, const String& mediaType)
{
    Dictionary constraintsDictionary;
    if (options.get(mediaType, constraintsDictionary) && !constraintsDictionary.isUndefinedOrNull())
        return MediaConstraintsImpl::create(constraintsDictionary);

    bool mediaRequested = false;
    if (!options.get(mediaType, mediaRequested) || !mediaRequested)
        return nullptr;

    return MediaConstraintsImpl::create();
}

void UserMediaRequest::start(Document* document, const Dictionary& options, MediaDevices::Promise&& promise, ExceptionCode& ec)
{
    if (!options.isObject()) {
        ec = TypeError;
        return;
    }

    UserMediaController* userMedia = UserMediaController::from(document ? document->page() : nullptr);
    if (!userMedia) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    RefPtr<MediaConstraints> audioConstraints = parseOptions(options, AtomicString("audio", AtomicString::ConstructFromLiteral));
    RefPtr<MediaConstraints> videoConstraints = parseOptions(options, AtomicString("video", AtomicString::ConstructFromLiteral));

    if (!audioConstraints && !videoConstraints) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    Ref<UserMediaRequest> request = adoptRef(*new UserMediaRequest(document, userMedia, audioConstraints.release(), videoConstraints.release(), WTF::move(promise)));
    request->start();
}

UserMediaRequest::UserMediaRequest(ScriptExecutionContext* context, UserMediaController* controller, PassRefPtr<MediaConstraints> audioConstraints, PassRefPtr<MediaConstraints> videoConstraints, MediaDevices::Promise&& promise)
    : ContextDestructionObserver(context)
    , m_audioConstraints(audioConstraints)
    , m_videoConstraints(videoConstraints)
    , m_controller(controller)
    , m_promise(WTF::move(promise))
{
}

UserMediaRequest::~UserMediaRequest()
{
}

SecurityOrigin* UserMediaRequest::securityOrigin() const
{
    if (m_scriptExecutionContext)
        return m_scriptExecutionContext->securityOrigin();

    return nullptr;
}
    
void UserMediaRequest::start()
{
    // 1 - make sure the system is capable of supporting the audio and video constraints. We don't want to ask for
    // user permission if the constraints can not be suported.
    RealtimeMediaSourceCenter::singleton().validateRequestConstraints(this, m_audioConstraints, m_videoConstraints);
}

void UserMediaRequest::constraintsValidated()
{
    RefPtr<UserMediaRequest> protectedThis(this);
    callOnMainThread([protectedThis] {
        // 2 - The constraints are valid, ask the user for access to media.
        if (UserMediaController* controller = protectedThis->m_controller)
            controller->requestPermission(*protectedThis.get());
    });
}

void UserMediaRequest::userMediaAccessGranted()
{
    RefPtr<UserMediaRequest> protectedThis(this);
    callOnMainThread([protectedThis] {
        // 3 - the user granted access, ask platform to create the media stream descriptors.
        RealtimeMediaSourceCenter::singleton().createMediaStream(protectedThis.get(), protectedThis->m_audioConstraints, protectedThis->m_videoConstraints);
    });
}

void UserMediaRequest::userMediaAccessDenied()
{
    failedToCreateStreamWithPermissionError();
}

void UserMediaRequest::constraintsInvalid(const String& constraintName)
{
    failedToCreateStreamWithConstraintsError(constraintName);
}

void UserMediaRequest::didCreateStream(PassRefPtr<MediaStreamPrivate> privateStream)
{
    if (!m_scriptExecutionContext)
        return;

    // 4 - Create the MediaStream and pass it to the success callback.
    RefPtr<MediaStream> stream = MediaStream::create(*m_scriptExecutionContext, privateStream);
    for (auto& track : stream->getAudioTracks())
        track->applyConstraints(*m_audioConstraints);
    for (auto& track : stream->getVideoTracks())
        track->applyConstraints(*m_videoConstraints);

    m_promise.resolve(stream);
}

void UserMediaRequest::failedToCreateStreamWithConstraintsError(const String& constraintName)
{
    ASSERT(!constraintName.isEmpty());
    if (!m_scriptExecutionContext)
        return;

    m_promise.reject(NavigatorUserMediaError::create(NavigatorUserMediaError::constraintNotSatisfiedErrorName(), constraintName));
}

void UserMediaRequest::failedToCreateStreamWithPermissionError()
{
    if (!m_scriptExecutionContext)
        return;

    // FIXME: Replace NavigatorUserMediaError with MediaStreamError (see bug 143335)
    m_promise.reject(NavigatorUserMediaError::create(NavigatorUserMediaError::permissionDeniedErrorName(), emptyString()));
}

void UserMediaRequest::contextDestroyed()
{
    Ref<UserMediaRequest> protect(*this);

    if (m_controller) {
        m_controller->cancelRequest(*this);
        m_controller = nullptr;
    }

    ContextDestructionObserver::contextDestroyed();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
