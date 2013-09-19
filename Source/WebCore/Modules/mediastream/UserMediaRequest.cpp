/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "MediaConstraintsImpl.h"
#include "MediaStream.h"
#include "MediaStreamCenter.h"
#include "MediaStreamDescriptor.h"
#include "SpaceSplitString.h"
#include "UserMediaController.h"
#include <wtf/Functional.h>
#include <wtf/MainThread.h>

namespace WebCore {

static PassRefPtr<MediaConstraintsImpl> parseOptions(const Dictionary& options, const String& mediaType, ExceptionCode& ec)
{
    RefPtr<MediaConstraintsImpl> constraints;

    Dictionary constraintsDictionary;
    bool ok = options.get(mediaType, constraintsDictionary);
    if (ok && !constraintsDictionary.isUndefinedOrNull())
        constraints = MediaConstraintsImpl::create(constraintsDictionary, ec);
    else {
        bool mediaRequested = false;
        options.get(mediaType, mediaRequested);
        if (mediaRequested)
            constraints = MediaConstraintsImpl::create();
    }

    return constraints.release();
}

PassRefPtr<UserMediaRequest> UserMediaRequest::create(ScriptExecutionContext* context, UserMediaController* controller, const Dictionary& options, PassRefPtr<NavigatorUserMediaSuccessCallback> successCallback, PassRefPtr<NavigatorUserMediaErrorCallback> errorCallback, ExceptionCode& ec)
{
    ASSERT(successCallback);

    RefPtr<MediaConstraintsImpl> audioConstraints = parseOptions(options, AtomicString("audio", AtomicString::ConstructFromLiteral), ec);
    if (ec)
        return 0;

    RefPtr<MediaConstraintsImpl> videoConstraints = parseOptions(options, AtomicString("video", AtomicString::ConstructFromLiteral), ec);
    if (ec)
        return 0;

    if (!audioConstraints && !videoConstraints)
        return 0;

    return adoptRef(new UserMediaRequest(context, controller, audioConstraints.release(), videoConstraints.release(), successCallback, errorCallback));
}

UserMediaRequest::UserMediaRequest(ScriptExecutionContext* context, UserMediaController* controller, PassRefPtr<MediaConstraintsImpl> audioConstraints, PassRefPtr<MediaConstraintsImpl> videoConstraints, PassRefPtr<NavigatorUserMediaSuccessCallback> successCallback, PassRefPtr<NavigatorUserMediaErrorCallback> errorCallback)
    : ContextDestructionObserver(context)
    , m_audioConstraints(audioConstraints)
    , m_videoConstraints(videoConstraints)
    , m_controller(controller)
    , m_successCallback(successCallback)
    , m_errorCallback(errorCallback)
{
}

UserMediaRequest::~UserMediaRequest()
{
}

PassRefPtr<MediaConstraints> UserMediaRequest::audioConstraints() const
{
    return m_audioConstraints;
}

PassRefPtr<MediaConstraints> UserMediaRequest::videoConstraints() const
{
    return m_videoConstraints;
}

Document* UserMediaRequest::ownerDocument()
{
    if (m_scriptExecutionContext)
        return toDocument(m_scriptExecutionContext);

    return 0;
}

void UserMediaRequest::start()
{
    MediaStreamCenter::instance().queryMediaStreamSources(this);
}

void UserMediaRequest::didCompleteQuery(const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
{
    if (m_controller)
        m_controller->requestUserMedia(this, audioSources, videoSources);
}

void UserMediaRequest::succeed(PassRefPtr<MediaStreamDescriptor> streamDescriptor)
{
    if (!m_scriptExecutionContext)
        return;

    RefPtr<MediaStream> stream = MediaStream::create(m_scriptExecutionContext, streamDescriptor);

    MediaStreamTrackVector tracks = stream->getAudioTracks();
    for (MediaStreamTrackVector::iterator iter = tracks.begin(); iter != tracks.end(); ++iter)
        (*iter)->component()->source()->setConstraints(m_audioConstraints);

    tracks = stream->getVideoTracks();
    for (MediaStreamTrackVector::iterator iter = tracks.begin(); iter != tracks.end(); ++iter)
        (*iter)->component()->source()->setConstraints(m_videoConstraints);

    m_successCallback->handleEvent(stream.get());
}

void UserMediaRequest::permissionFailure()
{
    if (!m_scriptExecutionContext)
        return;

    if (!m_errorCallback)
        return;
    
    RefPtr<NavigatorUserMediaError> error = NavigatorUserMediaError::create(NavigatorUserMediaError::permissionDeniedErrorName(), emptyString());
    callOnMainThread(bind(&UserMediaRequest::callErrorHandler, this, error.release()));
}

void UserMediaRequest::constraintFailure(const String& constraintName)
{
    ASSERT(!constraintName.isEmpty());
    if (!m_scriptExecutionContext)
        return;
    
    if (!m_errorCallback)
        return;

    RefPtr<NavigatorUserMediaError> error = NavigatorUserMediaError::create(NavigatorUserMediaError::constraintNotSatisfiedErrorName(), constraintName);
    callOnMainThread(bind(&UserMediaRequest::callErrorHandler, this, error.release()));
}

void UserMediaRequest::callSuccessHandler(PassRefPtr<MediaStream> stream)
{
    ASSERT(m_successCallback);
    
    m_successCallback->handleEvent(stream.get());
}

void UserMediaRequest::callErrorHandler(PassRefPtr<NavigatorUserMediaError> error)
{
    ASSERT(error);
    
    m_errorCallback->handleEvent(error.get());
}

void UserMediaRequest::contextDestroyed()
{
    Ref<UserMediaRequest> protect(*this);

    if (m_controller) {
        m_controller->cancelUserMediaRequest(this);
        m_controller = 0;
    }

    ContextDestructionObserver::contextDestroyed();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
