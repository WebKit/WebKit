/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
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

#ifndef UserMediaRequest_h
#define UserMediaRequest_h

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "MediaStreamCenter.h"
#include "MediaStreamSource.h"
#include "NavigatorUserMediaErrorCallback.h"
#include "NavigatorUserMediaSuccessCallback.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class UserMediaClient;

class UserMediaRequest : public MediaStreamSourcesQueryClient, public ContextDestructionObserver {
public:
    static PassRefPtr<UserMediaRequest> create(ScriptExecutionContext*, UserMediaClient*, const String& options, PassRefPtr<NavigatorUserMediaSuccessCallback>, PassRefPtr<NavigatorUserMediaErrorCallback>);
    ~UserMediaRequest();

    NavigatorUserMediaSuccessCallback* successCallback() const { return m_successCallback.get(); }
    NavigatorUserMediaErrorCallback* errorCallback() const { return m_errorCallback.get(); }

    void start();

    void succeed(const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources);
    void fail();

    // MediaStreamSourcesQueryClient
    virtual bool audio() const { return m_audio; }
    virtual bool video() const { return m_video; }
    virtual bool cameraPreferenceUser() const { return m_cameraPreferenceUser; }
    virtual bool cameraPreferenceEnvironment() const { return m_cameraPreferenceEnvironment; }
    virtual void didCompleteQuery(const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources);

    // ContextDestructionObserver
    virtual void contextDestroyed();

private:
    UserMediaRequest(ScriptExecutionContext*, UserMediaClient*, const String& options, PassRefPtr<NavigatorUserMediaSuccessCallback>, PassRefPtr<NavigatorUserMediaErrorCallback>);

    void parseOptions(const String& options);

    bool m_audio;
    bool m_video;

    bool m_cameraPreferenceUser;
    bool m_cameraPreferenceEnvironment;

    UserMediaClient* m_client;

    RefPtr<NavigatorUserMediaSuccessCallback> m_successCallback;
    RefPtr<NavigatorUserMediaErrorCallback> m_errorCallback;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // UserMediaRequest_h
