/*
 * Copyright (C) 2017 Igalia S.L.
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

#pragma once

#include "SameDocumentNavigationType.h"
#include <WebCore/LayoutMilestone.h>
#include <wtf/Forward.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class DOMWindowExtension;
class DOMWrapperWorld;
class ResourceError;
class ResourceRequest;
class SharedBuffer;
}

namespace WebKit {
class InjectedBundleBackForwardListItem;
class WebFrame;
class WebPage;
}

namespace API {
class Object;

namespace InjectedBundle {

class PageLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~PageLoaderClient() = default;

    virtual void willLoadURLRequest(WebKit::WebPage&, const WebCore::ResourceRequest&, API::Object*) { }
    virtual void willLoadDataRequest(WebKit::WebPage&, const WebCore::ResourceRequest&, WebCore::SharedBuffer*, const WTF::String&, const WTF::String&, const WTF::URL&, API::Object*) { }

    virtual void didStartProvisionalLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didFailProvisionalLoadWithErrorForFrame(WebKit::WebPage&, WebKit::WebFrame&, const WebCore::ResourceError&, RefPtr<API::Object>&) { }
    virtual void didCommitLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didFinishDocumentLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didFinishLoadForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didFinishProgress(WebKit::WebPage&) { }
    virtual void didFailLoadWithErrorForFrame(WebKit::WebPage&, WebKit::WebFrame&, const WebCore::ResourceError&, RefPtr<API::Object>&) { }
    virtual void didSameDocumentNavigationForFrame(WebKit::WebPage&, WebKit::WebFrame&, WebKit::SameDocumentNavigationType, RefPtr<API::Object>&) { }
    virtual void didReceiveTitleForFrame(WebKit::WebPage&, const WTF::String&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didRemoveFrameFromHierarchy(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didDisplayInsecureContentForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didRunInsecureContentForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didDetectXSSForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }

    virtual void didFirstLayoutForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didFirstVisuallyNonEmptyLayoutForFrame(WebKit::WebPage&, WebKit::WebFrame&, RefPtr<API::Object>&) { }
    virtual void didLayoutForFrame(WebKit::WebPage&, WebKit::WebFrame&) { }
    virtual void didReachLayoutMilestone(WebKit::WebPage&, OptionSet<WebCore::LayoutMilestone>, RefPtr<API::Object>&) { }

    virtual void didClearWindowObjectForFrame(WebKit::WebPage&, WebKit::WebFrame&, WebCore::DOMWrapperWorld&) { }
    virtual void didCancelClientRedirectForFrame(WebKit::WebPage&, WebKit::WebFrame&) { }
    virtual void willPerformClientRedirectForFrame(WebKit::WebPage&, WebKit::WebFrame&, const WTF::String&, double /*delay*/, WallTime /*date*/) { }
    virtual void didHandleOnloadEventsForFrame(WebKit::WebPage&, WebKit::WebFrame&) { }

    virtual void globalObjectIsAvailableForFrame(WebKit::WebPage&, WebKit::WebFrame&, WebCore::DOMWrapperWorld&) { }
    virtual void willDisconnectDOMWindowExtensionFromGlobalObject(WebKit::WebPage&, WebCore::DOMWindowExtension*) { }
    virtual void didReconnectDOMWindowExtensionToGlobalObject(WebKit::WebPage&, WebCore::DOMWindowExtension*) { }
    virtual void willDestroyGlobalObjectForDOMWindowExtension(WebKit::WebPage&, WebCore::DOMWindowExtension*) { }

    virtual void willInjectUserScriptForFrame(WebKit::WebPage&, WebKit::WebFrame&, WebCore::DOMWrapperWorld&) { }

    virtual bool shouldForceUniversalAccessFromLocalURL(WebKit::WebPage&, const WTF::String&) { return false; }

    virtual void featuresUsedInPage(WebKit::WebPage&, const Vector<WTF::String>&) { }

    virtual void willDestroyFrame(WebKit::WebPage&, WebKit::WebFrame&) { }
    virtual WTF::String userAgentForURL(WebKit::WebFrame&, const WTF::URL&) const { return WTF::String(); }

    virtual OptionSet<WebCore::LayoutMilestone> layoutMilestones() const { return { }; }
};

} // namespace InjectedBundle

} // namespace API
