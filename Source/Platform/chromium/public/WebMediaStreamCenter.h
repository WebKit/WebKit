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

#ifndef WebMediaStreamCenter_h
#define WebMediaStreamCenter_h

#include "WebString.h"

namespace WebKit {
class WebICECandidateDescriptor;
class WebMediaStreamComponent;
class WebMediaStreamDescriptor;
class WebMediaStreamSourcesRequest;
class WebSessionDescriptionDescriptor;

class WebMediaStreamCenter {
public:
    virtual ~WebMediaStreamCenter() { }

    virtual void queryMediaStreamSources(const WebMediaStreamSourcesRequest&) = 0;
    virtual void didEnableMediaStreamTrack(const WebMediaStreamDescriptor&, const WebMediaStreamComponent&) = 0;
    virtual void didDisableMediaStreamTrack(const WebMediaStreamDescriptor&, const WebMediaStreamComponent&) = 0;
    virtual void didStopLocalMediaStream(const WebMediaStreamDescriptor&) = 0;
    virtual void didCreateMediaStream(WebMediaStreamDescriptor&) { }

    // DEPRECATED
    virtual void didConstructMediaStream(const WebMediaStreamDescriptor&) { }

    virtual WebString constructSDP(const WebICECandidateDescriptor&) = 0;
    virtual WebString constructSDP(const WebSessionDescriptionDescriptor&) = 0;
};

} // namespace WebKit

#endif // WebMediaStreamCenter_h

