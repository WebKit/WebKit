/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef APIDownloadClient_h
#define APIDownloadClient_h

#include <functional>
#include <wtf/text/WTFString.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {
class AuthenticationChallengeProxy;
class DownloadProxy;
class WebProcessPool;
class WebProtectionSpace;
}

namespace API {

class DownloadClient {
public:
    virtual ~DownloadClient() { }

    virtual void didStart(WebKit::WebProcessPool*, WebKit::DownloadProxy*) { }
    virtual void didReceiveAuthenticationChallenge(WebKit::WebProcessPool*, WebKit::DownloadProxy*, WebKit::AuthenticationChallengeProxy*) { }
    virtual void didReceiveResponse(WebKit::WebProcessPool*, WebKit::DownloadProxy*, const WebCore::ResourceResponse&) { }
    virtual void didReceiveData(WebKit::WebProcessPool*, WebKit::DownloadProxy*, uint64_t) { }
    virtual bool shouldDecodeSourceDataOfMIMEType(WebKit::WebProcessPool*, WebKit::DownloadProxy*, const WTF::String&) { return true; }
    virtual WTF::String decideDestinationWithSuggestedFilename(WebKit::WebProcessPool*, WebKit::DownloadProxy*, const WTF::String&, bool&) { return { }; }
    virtual void didCreateDestination(WebKit::WebProcessPool*, WebKit::DownloadProxy*, const WTF::String&) { }
    virtual void didFinish(WebKit::WebProcessPool*, WebKit::DownloadProxy*) { }
    virtual void didFail(WebKit::WebProcessPool*, WebKit::DownloadProxy*, const WebCore::ResourceError&) { }
    virtual void didCancel(WebKit::WebProcessPool*, WebKit::DownloadProxy*) { }
    virtual void processDidCrash(WebKit::WebProcessPool*, WebKit::DownloadProxy*) { }
    virtual bool canAuthenticateAgainstProtectionSpace(WebKit::WebProtectionSpace*) { return true; }
    virtual void willSendRequest(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse&, std::function<void(const WebCore::ResourceRequest&)> callback) { callback(request); }
};

} // namespace API

#endif // APIDownloadClient_h
