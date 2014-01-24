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

#include "APIClient.h"
#include "APILoaderClient.h"
#include "WKPageLoaderClient.h"

namespace API {
class Object;

template<> struct ClientTraits<WKPageLoaderClientBase> {
    typedef std::tuple<WKPageLoaderClientV0, WKPageLoaderClientV1, WKPageLoaderClientV2, WKPageLoaderClientV3, WKPageLoaderClientV4> Versions;
};
}

namespace WebCore {
class ResourceError;
}

namespace WebKit {

class AuthenticationChallengeProxy;
class AuthenticationDecisionListener;
class ImmutableDictionary;
class WebBackForwardListItem;
class WebFrameProxy;
class WebPageProxy;
class WebProtectionSpace;

class WebLoaderClient : public API::Client<WKPageLoaderClientBase>, public API::LoaderClient {
public:
    explicit WebLoaderClient(const WKPageLoaderClientBase*);

private:
    virtual void didStartProvisionalLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;
    virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    virtual void didFailProvisionalLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceError&, API::Object*) override;
    virtual void didCommitLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;
    virtual void didFinishDocumentLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;
    virtual void didFinishLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;
    virtual void didFailLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceError&, API::Object*) override;
    virtual void didSameDocumentNavigationForFrame(WebPageProxy*, WebFrameProxy*, SameDocumentNavigationType, API::Object*) override;
    virtual void didReceiveTitleForFrame(WebPageProxy*, const WTF::String&, WebFrameProxy*, API::Object*) override;
    virtual void didFirstLayoutForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;

    // FIXME: We should consider removing didFirstVisuallyNonEmptyLayoutForFrame since it is replaced by didLayout.
    virtual void didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;

    virtual void didRemoveFrameFromHierarchy(WebPageProxy*, WebFrameProxy*, API::Object*) override;
    virtual void didDisplayInsecureContentForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;
    virtual void didRunInsecureContentForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;
    virtual void didDetectXSSForFrame(WebPageProxy*, WebFrameProxy*, API::Object*) override;

    virtual void didLayout(WebPageProxy*, WebCore::LayoutMilestones, API::Object*) override;
    
    bool canAuthenticateAgainstProtectionSpaceInFrame(WebPageProxy*, WebFrameProxy*, WebProtectionSpace*) override;
    virtual void didReceiveAuthenticationChallengeInFrame(WebPageProxy*, WebFrameProxy*, AuthenticationChallengeProxy*) override;

    virtual void didStartProgress(WebPageProxy*) override;
    virtual void didChangeProgress(WebPageProxy*) override;
    virtual void didFinishProgress(WebPageProxy*) override;

    // FIXME: These three functions should not be part of this client.
    virtual void processDidBecomeUnresponsive(WebPageProxy*) override;
    virtual void interactionOccurredWhileProcessUnresponsive(WebPageProxy*) override;
    virtual void processDidBecomeResponsive(WebPageProxy*) override;
    virtual void processDidCrash(WebPageProxy*) override;

    virtual void didChangeBackForwardList(WebPageProxy*, WebBackForwardListItem* addedItem, Vector<RefPtr<API::Object>>* removedItems) override;
    virtual void willGoToBackForwardListItem(WebPageProxy*, WebBackForwardListItem*, API::Object*) override;

#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginModuleLoadPolicy pluginLoadPolicy(WebPageProxy*, PluginModuleLoadPolicy currentPluginLoadPolicy, ImmutableDictionary*, WTF::String& unavailabilityDescription, WTF::String& useBlockedPluginTitle) override;
    virtual void didFailToInitializePlugin(WebPageProxy*, ImmutableDictionary*) override;
    virtual void didBlockInsecurePluginVersion(WebPageProxy*, ImmutableDictionary*) override;
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLLoadPolicy(WebPageProxy*, const WTF::String&) const;
#endif // ENABLE(WEBGL)
};

} // namespace WebKit

#endif // WebLoaderClient_h
