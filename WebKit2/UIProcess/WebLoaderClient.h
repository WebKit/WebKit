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

#ifndef WebLoaderClient_h
#define WebLoaderClient_h

#include "WKPage.h"
#include <wtf/Forward.h>

namespace WebKit {

class APIObject;
class WebPageProxy;
class WebFrameProxy;

class WebLoaderClient {
public:
    WebLoaderClient();
    void initialize(const WKPageLoaderClient*);

    void didStartProvisionalLoadForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didFailProvisionalLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didCommitLoadForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didFinishDocumentLoadForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didFinishLoadForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didFailLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didReceiveTitleForFrame(WebPageProxy*, const String&, WebFrameProxy*, APIObject*);
    void didFirstLayoutForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy*, WebFrameProxy*, APIObject*);
    void didRemoveFrameFromHierarchy(WebPageProxy*, WebFrameProxy*, APIObject*);

    void didStartProgress(WebPageProxy*);
    void didChangeProgress(WebPageProxy*);
    void didFinishProgress(WebPageProxy*);

    // FIXME: These three functions should not be part of this client.
    void didBecomeUnresponsive(WebPageProxy*);
    void didBecomeResponsive(WebPageProxy*);
    void processDidExit(WebPageProxy*);

    void didChangeBackForwardList(WebPageProxy*);

private:
    WKPageLoaderClient m_pageLoaderClient;
};

} // namespace WebKit

#endif // WebLoaderClient_h
