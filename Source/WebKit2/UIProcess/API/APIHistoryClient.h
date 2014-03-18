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

#ifndef APIHistoryClient_h
#define APIHistoryClient_h

#include <wtf/Forward.h>

namespace WebKit {
class WebContext;
class WebFrameProxy;
class WebPageProxy;
struct WebNavigationDataStore;
}

namespace API {

class HistoryClient {
public:
    virtual ~HistoryClient() { }

    virtual void didNavigateWithNavigationData(WebKit::WebContext*, WebKit::WebPageProxy*, const WebKit::WebNavigationDataStore&, WebKit::WebFrameProxy*) { }
    virtual void didPerformClientRedirect(WebKit::WebContext*, WebKit::WebPageProxy*, const WTF::String& sourceURL, const WTF::String& destinationURL, WebKit::WebFrameProxy*)
    {
        UNUSED_PARAM(sourceURL);
        UNUSED_PARAM(destinationURL);
    }
    virtual void didPerformServerRedirect(WebKit::WebContext*, WebKit::WebPageProxy*, const WTF::String& sourceURL, const WTF::String& destinationURL, WebKit::WebFrameProxy*)
    {
        UNUSED_PARAM(sourceURL);
        UNUSED_PARAM(destinationURL);
    }
    virtual void didUpdateHistoryTitle(WebKit::WebContext*, WebKit::WebPageProxy*, const WTF::String& title, const WTF::String& url, WebKit::WebFrameProxy*)
    {
        UNUSED_PARAM(title);
        UNUSED_PARAM(url);
    }
    virtual void populateVisitedLinks(WebKit::WebContext*) { }
    virtual bool shouldTrackVisitedLinks() const { return false; }
};

} // namespace API

#endif // APIHistoryClient_h
