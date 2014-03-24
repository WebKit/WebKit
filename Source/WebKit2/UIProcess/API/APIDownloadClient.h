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

#include <wtf/text/WTFString.h>

namespace WebCore {
class ResourceError;
class ResourceResponse;
}

namespace WebKit {
class AuthenticationChallengeProxy;
class DownloadProxy;
class WebContext;
}

namespace API {

class DownloadClient {
public:
    virtual ~DownloadClient() { }

    virtual void didStart(WebKit::WebContext*, WebKit::DownloadProxy*) { }
    virtual void didReceiveAuthenticationChallenge(WebKit::WebContext*, WebKit::DownloadProxy*, WebKit::AuthenticationChallengeProxy*) { }
    virtual void didReceiveResponse(WebKit::WebContext*, WebKit::DownloadProxy*, const WebCore::ResourceResponse&) { }
    virtual void didReceiveData(WebKit::WebContext*, WebKit::DownloadProxy*, uint64_t length) { }
    virtual bool shouldDecodeSourceDataOfMIMEType(WebKit::WebContext*, WebKit::DownloadProxy*, const WTF::String& mimeType) { return true; }
    virtual WTF::String decideDestinationWithSuggestedFilename(WebKit::WebContext*, WebKit::DownloadProxy*, const WTF::String& filename, bool& allowOverwrite) { return {}; }
    virtual void didCreateDestination(WebKit::WebContext*, WebKit::DownloadProxy*, const WTF::String& path) { }
    virtual void didFinish(WebKit::WebContext*, WebKit::DownloadProxy*) { }
    virtual void didFail(WebKit::WebContext*, WebKit::DownloadProxy*, const WebCore::ResourceError&) { }
    virtual void didCancel(WebKit::WebContext*, WebKit::DownloadProxy*) { }
    virtual void processDidCrash(WebKit::WebContext*, WebKit::DownloadProxy*) { }
};

} // namespace API

#endif // APIDownloadClient_h
