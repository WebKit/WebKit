/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#if ENABLE_WEBRTC
#include "WebUserMediaClientMock.h"

#include "MockConstraints.h"
#include "WebDocument.h"
#include "WebMediaStreamRegistry.h"
#include "WebTestDelegate.h"
#include "WebUserMediaRequest.h"
#include <public/WebMediaConstraints.h>
#include <public/WebMediaStream.h>
#include <public/WebMediaStreamSource.h>
#include <public/WebMediaStreamTrack.h>
#include <public/WebVector.h>

using namespace WebKit;

namespace WebTestRunner {

class UserMediaRequestTask : public WebMethodTask<WebUserMediaClientMock> {
public:
    UserMediaRequestTask(WebUserMediaClientMock* object, const WebUserMediaRequest& request, const WebMediaStream result)
        : WebMethodTask<WebUserMediaClientMock>(object)
        , m_request(request)
        , m_result(result)
    {
    }

    virtual void runIfValid() OVERRIDE
    {
        if (m_result.isNull())
            m_request.requestFailed();
        else
            m_request.requestSucceeded(m_result);
    }

private:
    WebUserMediaRequest m_request;
    WebMediaStream m_result;
};

////////////////////////////////

class MockExtraData : public WebMediaStream::ExtraData {
public:
    int foo;
};

WebUserMediaClientMock::WebUserMediaClientMock(WebTestDelegate* delegate)
    : m_delegate(delegate)
{
}

void WebUserMediaClientMock::requestUserMedia(const WebUserMediaRequest& streamRequest, const WebVector<WebMediaStreamSource>& audioSourcesVector, const WebVector<WebMediaStreamSource>& videoSourcesVector)
{
    WEBKIT_ASSERT(!streamRequest.isNull());
    WebUserMediaRequest request = streamRequest;

    if (request.ownerDocument().isNull() || !request.ownerDocument().frame()) {
        m_delegate->postTask(new UserMediaRequestTask(this, request, WebMediaStream()));
        return;
    }

    WebMediaConstraints constraints = request.audioConstraints();
    if (!constraints.isNull() && !MockConstraints::verifyConstraints(constraints)) {
        m_delegate->postTask(new UserMediaRequestTask(this, request, WebMediaStream()));
        return;
    }
    constraints = request.videoConstraints();
    if (!constraints.isNull() && !MockConstraints::verifyConstraints(constraints)) {
        m_delegate->postTask(new UserMediaRequestTask(this, request, WebMediaStream()));
        return;
    }

    const size_t zero = 0;
    const size_t one = 1;
    WebVector<WebMediaStreamTrack> audioTracks(request.audio() ? one : zero);
    WebVector<WebMediaStreamTrack> videoTracks(request.video() ? one : zero);

    if (request.audio()) {
        WebMediaStreamSource source;
        source.initialize("MockAudioDevice#1", WebMediaStreamSource::TypeAudio, "Mock audio device");
        audioTracks[0].initialize(source);
    }

    if (request.video()) {
        WebMediaStreamSource source;
        source.initialize("MockVideoDevice#1", WebMediaStreamSource::TypeVideo, "Mock video device");
        videoTracks[0].initialize(source);
    }

    WebMediaStream stream;
    stream.initialize(audioTracks, videoTracks);

    stream.setExtraData(new MockExtraData());

    m_delegate->postTask(new UserMediaRequestTask(this, request, stream));
}

void WebUserMediaClientMock::cancelUserMediaRequest(const WebUserMediaRequest&)
{
}

}

#endif // ENABLE_WEBRTC
