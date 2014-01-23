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
#include "PluginModuleInfo.h"
#include "SameDocumentNavigationType.h"
#include "WKPage.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/LayoutMilestones.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

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

class WebLoaderClient : public API::Client<WKPageLoaderClientBase> {
public:
    void didStartProvisionalLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didFailProvisionalLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceError&, API::Object*);
    void didCommitLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didFinishDocumentLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didFinishLoadForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didFailLoadWithErrorForFrame(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceError&, API::Object*);
    void didSameDocumentNavigationForFrame(WebPageProxy*, WebFrameProxy*, SameDocumentNavigationType, API::Object*);
    void didReceiveTitleForFrame(WebPageProxy*, const String&, WebFrameProxy*, API::Object*);
    void didFirstLayoutForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);

    // FIXME: We should consider removing didFirstVisuallyNonEmptyLayoutForFrame since it is replaced by didLayout.
    void didFirstVisuallyNonEmptyLayoutForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);

    void didRemoveFrameFromHierarchy(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didDisplayInsecureContentForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didRunInsecureContentForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);
    void didDetectXSSForFrame(WebPageProxy*, WebFrameProxy*, API::Object*);

    void didLayout(WebPageProxy*, WebCore::LayoutMilestones, API::Object*);
    
    bool canAuthenticateAgainstProtectionSpaceInFrame(WebPageProxy*, WebFrameProxy*, WebProtectionSpace*);
    void didReceiveAuthenticationChallengeInFrame(WebPageProxy*, WebFrameProxy*, AuthenticationChallengeProxy*);

    void didStartProgress(WebPageProxy*);
    void didChangeProgress(WebPageProxy*);
    void didFinishProgress(WebPageProxy*);

    // FIXME: These three functions should not be part of this client.
    void processDidBecomeUnresponsive(WebPageProxy*);
    void interactionOccurredWhileProcessUnresponsive(WebPageProxy*);
    void processDidBecomeResponsive(WebPageProxy*);
    void processDidCrash(WebPageProxy*);

    void didChangeBackForwardList(WebPageProxy*, WebBackForwardListItem* addedItem, Vector<RefPtr<API::Object>>* removedItems);
    bool shouldGoToBackForwardListItem(WebPageProxy*, WebBackForwardListItem*);
    void willGoToBackForwardListItem(WebPageProxy*, WebBackForwardListItem*, API::Object*);

#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginModuleLoadPolicy pluginLoadPolicy(WebPageProxy*, PluginModuleLoadPolicy currentPluginLoadPolicy, ImmutableDictionary*, String& unavailabilityDescription, String& useBlockedPluginTitle);
    void didFailToInitializePlugin(WebPageProxy*, ImmutableDictionary*);
    void didBlockInsecurePluginVersion(WebPageProxy*, ImmutableDictionary*);
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLLoadPolicy(WebPageProxy*, const String&) const;
#endif // ENABLE(WEBGL)
};

} // namespace WebKit

#endif // WebLoaderClient_h
