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

#ifndef WebUserMediaClientMock_h
#define WebUserMediaClientMock_h

#include "WebUserMediaClient.h"
#include "platform/WebCommon.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "webkit/support/test_media_stream_client.h"
#include <wtf/PassOwnPtr.h>

namespace WebKit {

class WebUserMediaClientMock : public WebUserMediaClient,
                               public webkit_support::MediaStreamUtil {
public:
    static PassOwnPtr<WebUserMediaClientMock> create();
    ~WebUserMediaClientMock() { }

    bool IsMockStream(const WebURL&);

    virtual void requestUserMedia(const WebUserMediaRequest&, const WebVector<WebMediaStreamSource>&);
    virtual void cancelUserMediaRequest(const WebUserMediaRequest&);

private:
    WebUserMediaClientMock() { }
};

} // namespace WebKit

#endif // WebUserMediaClientMock_h
