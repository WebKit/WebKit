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
#include "APIInjectedBundlePageLoaderClient.h"
#include "WKBundlePageLoaderClient.h"
#include <wtf/Forward.h>

namespace API {
class Object;
class String;
class URL;

template<> struct ClientTraits<WKBundlePageLoaderClientBase> {
    typedef std::tuple<WKBundlePageLoaderClientV0, WKBundlePageLoaderClientV1, WKBundlePageLoaderClientV2, WKBundlePageLoaderClientV3, WKBundlePageLoaderClientV4, WKBundlePageLoaderClientV5, WKBundlePageLoaderClientV6, WKBundlePageLoaderClientV7, WKBundlePageLoaderClientV8, WKBundlePageLoaderClientV9> Versions;
};
}

namespace WebKit {

class InjectedBundlePageLoaderClient : public API::Client<WKBundlePageLoaderClientBase>, public API::InjectedBundle::PageLoaderClient {
public:
    explicit InjectedBundlePageLoaderClient(const WKBundlePageLoaderClientBase*);

    void willLoadURLRequest(WebPage&, const WebCore::ResourceRequest&, API::Object*) override;
    void willLoadDataRequest(WebPage&, const WebCore::ResourceRequest&, WebCore::SharedBuffer*, const WTF::String&, const WTF::String&, const WebCore::URL&, API::Object*) override;

    void didStartProvisionalLoadForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didFailProvisionalLoadWithErrorForFrame(WebPage&, WebFrame&, const WebCore::ResourceError&, RefPtr<API::Object>&) override;
    void didCommitLoadForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didFinishDocumentLoadForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didFinishLoadForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didFinishProgress(WebPage&) override;
    void didFailLoadWithErrorForFrame(WebPage&, WebFrame&, const WebCore::ResourceError&, RefPtr<API::Object>&) override;
    void didSameDocumentNavigationForFrame(WebPage&, WebFrame&, SameDocumentNavigationType, RefPtr<API::Object>&) override;
    void didReceiveTitleForFrame(WebPage&, const WTF::String&, WebFrame&, RefPtr<API::Object>&) override;
    void didRemoveFrameFromHierarchy(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didDisplayInsecureContentForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didRunInsecureContentForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didDetectXSSForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;

    void didFirstLayoutForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didFirstVisuallyNonEmptyLayoutForFrame(WebPage&, WebFrame&, RefPtr<API::Object>&) override;
    void didLayoutForFrame(WebPage&, WebFrame&) override;
    void didReachLayoutMilestone(WebPage&, WebCore::LayoutMilestones, RefPtr<API::Object>&) override;

    void didClearWindowObjectForFrame(WebPage&, WebFrame&, WebCore::DOMWrapperWorld&) override;
    void didCancelClientRedirectForFrame(WebPage&, WebFrame&) override;
    void willPerformClientRedirectForFrame(WebPage&, WebFrame&, const WTF::String&, double /*delay*/, WallTime /*date*/) override;
    void didHandleOnloadEventsForFrame(WebPage&, WebFrame&) override;

    void globalObjectIsAvailableForFrame(WebPage&, WebFrame&, WebCore::DOMWrapperWorld&) override;
    void willDisconnectDOMWindowExtensionFromGlobalObject(WebPage&, WebCore::DOMWindowExtension*) override;
    void didReconnectDOMWindowExtensionToGlobalObject(WebPage&, WebCore::DOMWindowExtension*) override;
    void willDestroyGlobalObjectForDOMWindowExtension(WebPage&, WebCore::DOMWindowExtension*) override;

    void willInjectUserScriptForFrame(WebKit::WebPage&, WebKit::WebFrame&, WebCore::DOMWrapperWorld&) final;

    bool shouldForceUniversalAccessFromLocalURL(WebPage&, const WTF::String&) override;

    void featuresUsedInPage(WebPage&, const Vector<WTF::String>&) override;

    WTF::String userAgentForURL(WebFrame&, const WebCore::URL&) const override;

    WebCore::LayoutMilestones layoutMilestones() const override;
};

} // namespace WebKit

#endif // InjectedBundlePageLoaderClient_h
