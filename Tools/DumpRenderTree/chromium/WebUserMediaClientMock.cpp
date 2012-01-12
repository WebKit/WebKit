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
#include "WebUserMediaClientMock.h"

#include "WebMediaStreamRegistry.h"
#include "WebUserMediaRequest.h"
#include "platform/WebMediaStreamDescriptor.h"
#include "platform/WebMediaStreamSource.h"
#include "platform/WebVector.h"
#include <wtf/Assertions.h>

namespace WebKit {

PassOwnPtr<WebUserMediaClientMock> WebUserMediaClientMock::create()
{
    return adoptPtr(new WebUserMediaClientMock());
}

bool WebUserMediaClientMock::IsMockStream(const WebURL& url)
{
    WebMediaStreamDescriptor descriptor(WebMediaStreamRegistry::lookupMediaStreamDescriptor(url));
    if (descriptor.isNull())
        return false;

    WebVector<WebMediaStreamSource> sourceVector;
    descriptor.sources(sourceVector);
    WebString trackId;
    for (size_t i = 0; i < sourceVector.size(); ++i) {
        if (sourceVector[i].type() == WebMediaStreamSource::TypeVideo) {
            trackId = sourceVector[i].id();
            break;
        }
    }
    return trackId.equals("mediastreamtest");
}

void WebUserMediaClientMock::requestUserMedia(const WebUserMediaRequest& streamRequest, const WebVector<WebMediaStreamSource>& streamSourceVector)
{
    ASSERT(!streamRequest.isNull());

    WebUserMediaRequest request = streamRequest;
    const size_t size = 1;
    WebVector<WebMediaStreamSource> sourceVector(size);
    WebString trackId("mediastreamtest");
    WebString trackName("VideoCapture");
    sourceVector[0].initialize(trackId, WebMediaStreamSource::TypeVideo, trackName);
    request.requestSucceeded(sourceVector);
}

void WebUserMediaClientMock::cancelUserMediaRequest(const WebUserMediaRequest&)
{
}

} // namespace WebKit
