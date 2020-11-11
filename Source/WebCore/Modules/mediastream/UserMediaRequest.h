/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "CaptureDevice.h"
#include "IDLTypes.h"
#include "MediaConstraints.h"
#include "MediaStreamPrivate.h"
#include "MediaStreamRequest.h"
#include <wtf/CompletionHandler.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class MediaStream;
class SecurityOrigin;

template<typename IDLType> class DOMPromiseDeferred;

enum UserMediaRequestIdentifierType { };
using UserMediaRequestIdentifier = ObjectIdentifier<UserMediaRequestIdentifierType>;

class UserMediaRequest : public RefCounted<UserMediaRequest>, public ActiveDOMObject {
public:
    static Ref<UserMediaRequest> create(Document&, MediaStreamRequest&&, DOMPromiseDeferred<IDLInterface<MediaStream>>&&);
    virtual ~UserMediaRequest();

    UserMediaRequestIdentifier identifier() const { return m_identifier; }
    void start();

    WEBCORE_EXPORT void setAllowedMediaDeviceUIDs(const String& audioDeviceUID, const String& videoDeviceUID);
    WEBCORE_EXPORT void allow(CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, String&& deviceIdentifierHashSalt, CompletionHandler<void()>&&);

    enum MediaAccessDenialReason { NoConstraints, UserMediaDisabled, NoCaptureDevices, InvalidConstraint, HardwareError, PermissionDenied, InvalidAccess, IllegalConstraint, OtherFailure };
    WEBCORE_EXPORT void deny(MediaAccessDenialReason, const String& errorMessage = emptyString());

    const Vector<String>& audioDeviceUIDs() const { return m_audioDeviceUIDs; }
    const Vector<String>& videoDeviceUIDs() const { return m_videoDeviceUIDs; }

    const MediaConstraints& audioConstraints() const { return m_request.audioConstraints; }
    const MediaConstraints& videoConstraints() const { return m_request.videoConstraints; }

    WEBCORE_EXPORT SecurityOrigin* userMediaDocumentOrigin() const;
    WEBCORE_EXPORT SecurityOrigin* topLevelDocumentOrigin() const;
    WEBCORE_EXPORT Document* document() const;

    const MediaStreamRequest& request() const { return m_request; }

private:
    UserMediaRequest(Document&, MediaStreamRequest&&, DOMPromiseDeferred<IDLInterface<MediaStream>>&&);

    void stop() final;
    const char* activeDOMObjectName() const final;

    void mediaStreamDidFail(RealtimeMediaSource::Type);

    UserMediaRequestIdentifier m_identifier;

    Vector<String> m_videoDeviceUIDs;
    Vector<String> m_audioDeviceUIDs;

    UniqueRef<DOMPromiseDeferred<IDLInterface<MediaStream>>> m_promise;
    CompletionHandler<void()> m_allowCompletionHandler;
    MediaStreamRequest m_request;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
