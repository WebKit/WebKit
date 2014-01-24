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

#ifndef APILoaderClient_h
#define APILoaderClient_h

#include "PluginModuleInfo.h"
#include "SameDocumentNavigationType.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/LayoutMilestones.h>
#include <wtf/Forward.h>

namespace WebCore {
class ResourceError;
}

namespace WebKit {
class AuthenticationChallengeProxy;
class ImmutableDictionary;
class WebBackForwardListItem;
class WebFrameProxy;
class WebPageProxy;
class WebProtectionSpace;
}

namespace API {

class Object;

class LoaderClient {
public:
    virtual ~LoaderClient() { }

    virtual void didStartProvisionalLoadForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didFailProvisionalLoadWithErrorForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, const WebCore::ResourceError&, API::Object*) = 0;
    virtual void didCommitLoadForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didFinishDocumentLoadForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didFinishLoadForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didFailLoadWithErrorForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, const WebCore::ResourceError&, API::Object*) = 0;
    virtual void didSameDocumentNavigationForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::SameDocumentNavigationType, API::Object*) = 0;
    virtual void didReceiveTitleForFrame(WebKit::WebPageProxy*, const WTF::String&, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didFirstLayoutForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;

    // FIXME: We should consider removing didFirstVisuallyNonEmptyLayoutForFrame since it is replaced by didLayout.
    virtual void didFirstVisuallyNonEmptyLayoutForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;

    virtual void didRemoveFrameFromHierarchy(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didDisplayInsecureContentForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didRunInsecureContentForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;
    virtual void didDetectXSSForFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, API::Object*) = 0;

    virtual void didLayout(WebKit::WebPageProxy*, WebCore::LayoutMilestones, API::Object*) = 0;
    
    virtual bool canAuthenticateAgainstProtectionSpaceInFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::WebProtectionSpace*) = 0;
    virtual void didReceiveAuthenticationChallengeInFrame(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebKit::AuthenticationChallengeProxy*) = 0;

    virtual void didStartProgress(WebKit::WebPageProxy*) = 0;
    virtual void didChangeProgress(WebKit::WebPageProxy*)= 0;
    virtual void didFinishProgress(WebKit::WebPageProxy*)= 0;

    // FIXME: These three functions should not be part of this client.
    virtual void processDidBecomeUnresponsive(WebKit::WebPageProxy*) = 0;
    virtual void interactionOccurredWhileProcessUnresponsive(WebKit::WebPageProxy*) = 0;
    virtual void processDidBecomeResponsive(WebKit::WebPageProxy*) = 0;
    virtual void processDidCrash(WebKit::WebPageProxy*) = 0;

    virtual void didChangeBackForwardList(WebKit::WebPageProxy*, WebKit::WebBackForwardListItem* addedItem, Vector<RefPtr<API::Object>>* removedItems) = 0;
    virtual void willGoToBackForwardListItem(WebKit::WebPageProxy*, WebKit::WebBackForwardListItem*, API::Object*) = 0;

#if ENABLE(NETSCAPE_PLUGIN_API)
    virtual WebKit::PluginModuleLoadPolicy pluginLoadPolicy(WebKit::WebPageProxy*, WebKit::PluginModuleLoadPolicy currentPluginLoadPolicy, WebKit::ImmutableDictionary*, WTF::String& unavailabilityDescription, WTF::String& useBlockedPluginTitle) = 0;
    virtual void didFailToInitializePlugin(WebKit::WebPageProxy*, WebKit::ImmutableDictionary*) = 0;
    virtual void didBlockInsecurePluginVersion(WebKit::WebPageProxy*, WebKit::ImmutableDictionary*) = 0;
#endif // ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(WEBGL)
    virtual WebCore::WebGLLoadPolicy webGLLoadPolicy(WebKit::WebPageProxy*, const WTF::String&) const = 0;
#endif // ENABLE(WEBGL)
};

} // namespace API

#endif // APILoaderClient_h
