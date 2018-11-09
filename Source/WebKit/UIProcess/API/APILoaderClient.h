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

#pragma once

#include "APIData.h"
#include "PluginModuleInfo.h"
#include "SameDocumentNavigationType.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/LayoutMilestone.h>
#include <wtf/Forward.h>

namespace WebCore {
class ResourceError;
}

namespace WebKit {
class AuthenticationChallengeProxy;
class QuickLookDocumentData;
class WebBackForwardListItem;
class WebFrameProxy;
class WebPageProxy;
class WebProtectionSpace;
struct WebNavigationDataStore;
}

namespace API {

class Dictionary;
class Navigation;
class Object;

class LoaderClient {
public:
    virtual ~LoaderClient() { }
    virtual void didStartProvisionalLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didReceiveServerRedirectForProvisionalLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didFailProvisionalLoadWithErrorForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) { }
    virtual void didFinishLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
    virtual void didFailLoadWithErrorForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, const WebCore::ResourceError&, API::Object*) { }
    virtual void didFirstVisuallyNonEmptyLayoutForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Object*) { }
    virtual void didReachLayoutMilestone(WebKit::WebPageProxy&, OptionSet<WebCore::LayoutMilestone>) { }
    virtual bool shouldKeepCurrentBackForwardListItemInList(WebKit::WebPageProxy&, WebKit::WebBackForwardListItem&) { return true; }
    virtual bool processDidCrash(WebKit::WebPageProxy&) { return false; }
    virtual void didChangeBackForwardList(WebKit::WebPageProxy&, WebKit::WebBackForwardListItem*, Vector<Ref<WebKit::WebBackForwardListItem>>&&) { }
    virtual void didCommitLoadForFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, API::Navigation*, API::Object*) { }
};

} // namespace API
