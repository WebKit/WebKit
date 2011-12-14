/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebResourceLoadClient_h
#define WebResourceLoadClient_h

#include "APIClient.h"
#include "WKPage.h"

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class APIObject;
class WebFrameProxy;
class WebPageProxy;

class WebResourceLoadClient : public APIClient<WKPageResourceLoadClient> {
public:
    void didInitiateLoadForResource(WebPageProxy*, WebFrameProxy*, uint64_t resourceIdentifier, const WebCore::ResourceRequest&, bool pageIsProvisionallyLoading);
    void didSendRequestForResource(WebPageProxy*, WebFrameProxy*, uint64_t resourceIdentifier, const WebCore::ResourceRequest&, const WebCore::ResourceResponse&);
    void didReceiveResponseForResource(WebPageProxy*, WebFrameProxy*, uint64_t resourceIdentifier, const WebCore::ResourceResponse&);
    void didReceiveContentLengthForResource(WebPageProxy*, WebFrameProxy*, uint64_t resourceIdentifier, uint64_t contentLength);
    void didFinishLoadForResource(WebPageProxy*, WebFrameProxy*, uint64_t resourceIdentifier);
    void didFailLoadForResource(WebPageProxy*, WebFrameProxy*, uint64_t resourceIdentifier, const WebCore::ResourceError&);
};

} // namespace WebKit

#endif // WebResourceLoadClient_h
