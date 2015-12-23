/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef MediaDevicesRequest_h
#define MediaDevicesRequest_h

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "MediaDevices.h"
#include "MediaStreamCreationClient.h"
#include "MediaStreamTrackSourcesRequestClient.h"
#include "UserMediaPermissionCheck.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class Frame;
class SecurityOrigin;

typedef int ExceptionCode;

class MediaDevicesRequest : public MediaStreamTrackSourcesRequestClient, public UserMediaPermissionCheckClient, public ContextDestructionObserver {
public:
    static RefPtr<MediaDevicesRequest> create(Document*, MediaDevices::EnumerateDevicesPromise&&, ExceptionCode&);

    virtual ~MediaDevicesRequest();

    void start();

    SecurityOrigin* securityOrigin() const;

private:
    MediaDevicesRequest(ScriptExecutionContext*, MediaDevices::EnumerateDevicesPromise&&);

    void getTrackSources();

    // MediaStreamTrackSourcesRequestClient
    const String& requestOrigin() const final;
    void didCompleteRequest(const TrackSourceInfoVector&) final;

    // ContextDestructionObserver
    void contextDestroyed() override final;

    // UserMediaPermissionCheckClient
    void didCompleteCheck(bool) override final;

    MediaDevices::EnumerateDevicesPromise m_promise;
    RefPtr<MediaDevicesRequest> m_protector;
    RefPtr<UserMediaPermissionCheck> m_permissionCheck;

    bool m_canShowLabels { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaDevicesRequest_h
