/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef AssociatedURLLoader_h
#define AssociatedURLLoader_h

#include "WebURLLoader.h"
#include "WebURLLoaderClient.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {

class WebFrameImpl;

// This class is used to implement WebFrame::createAssociatedURLLoader.
// FIXME: Implement in terms of WebCore::SubresourceLoader.
class AssociatedURLLoader : public WebURLLoader,
                            public WebURLLoaderClient {
public:
    AssociatedURLLoader(PassRefPtr<WebFrameImpl>);
    ~AssociatedURLLoader();

    // WebURLLoader methods:
    virtual void loadSynchronously(const WebURLRequest&, WebURLResponse&, WebURLError&, WebData&);
    virtual void loadAsynchronously(const WebURLRequest&, WebURLLoaderClient*);
    virtual void cancel();
    virtual void setDefersLoading(bool);

    // WebURLLoaderClient methods:
    virtual void willSendRequest(WebURLLoader*, WebURLRequest& newRequest, const WebURLResponse& redirectResponse);
    virtual void didSendData(WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent);
    virtual void didReceiveResponse(WebURLLoader*, const WebURLResponse&);
    virtual void didDownloadData(WebURLLoader*, int dataLength);
    virtual void didReceiveData(WebURLLoader*, const char* data, int dataLength);
    virtual void didReceiveCachedMetadata(WebURLLoader*, const char* data, int dataLength);
    virtual void didFinishLoading(WebURLLoader*, double finishTime);
    virtual void didFail(WebURLLoader*, const WebURLError&);

private:
    void prepareRequest(WebURLRequest&);

    RefPtr<WebFrameImpl> m_frameImpl;
    OwnPtr<WebURLLoader> m_realLoader;
    WebURLLoaderClient* m_realClient;
};

} // namespace WebKit

#endif
