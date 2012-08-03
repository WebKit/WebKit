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

#ifndef MockWebMediaStreamCenter_h
#define MockWebMediaStreamCenter_h

#if ENABLE(MEDIA_STREAM)
#include <public/WebMediaStreamCenter.h>

namespace WebKit {
class WebMediaStreamCenterClient;
};

class MockWebMediaStreamCenter : public WebKit::WebMediaStreamCenter {
public:
    explicit MockWebMediaStreamCenter(WebKit::WebMediaStreamCenterClient*);

    virtual void queryMediaStreamSources(const WebKit::WebMediaStreamSourcesRequest&) OVERRIDE;
    virtual void didEnableMediaStreamTrack(const WebKit::WebMediaStreamDescriptor&, const WebKit::WebMediaStreamComponent&) OVERRIDE;
    virtual void didDisableMediaStreamTrack(const WebKit::WebMediaStreamDescriptor&, const WebKit::WebMediaStreamComponent&) OVERRIDE;
    virtual void didStopLocalMediaStream(const WebKit::WebMediaStreamDescriptor&) OVERRIDE;
    virtual void didCreateMediaStream(WebKit::WebMediaStreamDescriptor&) OVERRIDE;
    virtual WebKit::WebString constructSDP(const WebKit::WebICECandidateDescriptor&) OVERRIDE;
    virtual WebKit::WebString constructSDP(const WebKit::WebSessionDescriptionDescriptor&) OVERRIDE;

private:
    MockWebMediaStreamCenter() { }
};

#endif // ENABLE(MEDIA_STREAM)
#endif // MockWebMediaStreamCenter_h

