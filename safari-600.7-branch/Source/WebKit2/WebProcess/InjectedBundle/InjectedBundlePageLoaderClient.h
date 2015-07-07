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

#ifndef InjectedBundlePageLoaderClient_h
#define InjectedBundlePageLoaderClient_h

#include "APIClient.h"
#include "APIString.h"
#include "APIURL.h"
#include "SameDocumentNavigationType.h"
#include "WKBundlePage.h"
#include <JavaScriptCore/JSBase.h>
#include <WebCore/LayoutMilestones.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

namespace API {
class Object;

template<> struct ClientTraits<WKBundlePageLoaderClientBase> {
    typedef std::tuple<WKBundlePageLoaderClientV0, WKBundlePageLoaderClientV1, WKBundlePageLoaderClientV2, WKBundlePageLoaderClientV3, WKBundlePageLoaderClientV4, WKBundlePageLoaderClientV5, WKBundlePageLoaderClientV6, WKBundlePageLoaderClientV7, WKBundlePageLoaderClientV8> Versions;
};
}

namespace WebCore {
class DOMWindowExtension;
class DOMWrapperWorld;
class URL;
class ResourceError;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;
}

namespace WebKit {

class InjectedBundleBackForwardListItem;
class WebPage;
class WebFrame;

class InjectedBundlePageLoaderClient : public API::Client<WKBundlePageLoaderClientBase> {
public:
    void willLoadURLRequest(WebPage*, const WebCore::ResourceRequest&, API::Object*);
    void willLoadDataRequest(WebPage*, const WebCore::ResourceRequest&, WebCore::SharedBuffer*, const String&, const String&, const WebCore::URL&, API::Object*);

    bool shouldGoToBackForwardListItem(WebPage*, InjectedBundleBackForwardListItem*, RefPtr<API::Object>& userData);
    void didStartProvisionalLoadForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didFailProvisionalLoadWithErrorForFrame(WebPage*, WebFrame*, const WebCore::ResourceError&, RefPtr<API::Object>& userData);
    void didCommitLoadForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didFinishDocumentLoadForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didFinishLoadForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didFinishProgress(WebPage*);
    void didFailLoadWithErrorForFrame(WebPage*, WebFrame*, const WebCore::ResourceError&, RefPtr<API::Object>& userData);
    void didSameDocumentNavigationForFrame(WebPage*, WebFrame*, SameDocumentNavigationType, RefPtr<API::Object>& userData);
    void didReceiveTitleForFrame(WebPage*, const String&, WebFrame*, RefPtr<API::Object>& userData);
    void didRemoveFrameFromHierarchy(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didDisplayInsecureContentForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didRunInsecureContentForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didDetectXSSForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);

    void didFirstLayoutForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didFirstVisuallyNonEmptyLayoutForFrame(WebPage*, WebFrame*, RefPtr<API::Object>& userData);
    void didLayoutForFrame(WebPage*, WebFrame*);
    void didLayout(WebPage*, WebCore::LayoutMilestones, RefPtr<API::Object>& userData);

    void didClearWindowObjectForFrame(WebPage*, WebFrame*, WebCore::DOMWrapperWorld&);
    void didCancelClientRedirectForFrame(WebPage*, WebFrame*);
    void willPerformClientRedirectForFrame(WebPage*, WebFrame*, const String& url, double delay, double date);
    void didHandleOnloadEventsForFrame(WebPage*, WebFrame*);

    void globalObjectIsAvailableForFrame(WebPage*, WebFrame*, WebCore::DOMWrapperWorld&);
    void willDisconnectDOMWindowExtensionFromGlobalObject(WebPage*, WebCore::DOMWindowExtension*);
    void didReconnectDOMWindowExtensionToGlobalObject(WebPage*, WebCore::DOMWindowExtension*);
    void willDestroyGlobalObjectForDOMWindowExtension(WebPage*, WebCore::DOMWindowExtension*);

    bool shouldForceUniversalAccessFromLocalURL(WebPage*, const String& url);

    void featuresUsedInPage(WebPage*, const Vector<String>&);

    void willDestroyFrame(WebPage*, WebFrame*);
    API::String* userAgentForURL(WebFrame*, API::URL*) const;
};

} // namespace WebKit

#endif // InjectedBundlePageLoaderClient_h
