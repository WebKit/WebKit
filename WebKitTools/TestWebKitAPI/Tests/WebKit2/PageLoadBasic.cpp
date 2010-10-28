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

#include "Test.h"

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include <WebKit2/WebKit2.h>
#include <WebKit2/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool test1Done;

struct State {
    State()
        : didDecidePolicyForNavigationAction(false)
        , didStartProvisionalLoadForFrame(false)
        , didCommitLoadForFrame(false)
    {
    }

    bool didDecidePolicyForNavigationAction;
    bool didStartProvisionalLoadForFrame;
    bool didCommitLoadForFrame;
};

static void didStartProvisionalLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    TEST_ASSERT(state->didDecidePolicyForNavigationAction);
    TEST_ASSERT(!state->didCommitLoadForFrame);

    // The commited URL should be null.
    TEST_ASSERT(!WKFrameCopyURL(frame));

    TEST_ASSERT(!state->didStartProvisionalLoadForFrame);


    state->didStartProvisionalLoadForFrame = true;
}

static void didCommitLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    TEST_ASSERT(state->didDecidePolicyForNavigationAction);
    TEST_ASSERT(state->didStartProvisionalLoadForFrame);

    // The provisional URL should be null.
    TEST_ASSERT(!WKFrameCopyProvisionalURL(frame));

    state->didCommitLoadForFrame = true;
}

static void didFinishLoadForFrame(WKPageRef page, WKFrameRef frame, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    TEST_ASSERT(state->didDecidePolicyForNavigationAction);
    TEST_ASSERT(state->didStartProvisionalLoadForFrame);
    TEST_ASSERT(state->didCommitLoadForFrame);

    // The provisional URL should be null.
    TEST_ASSERT(!WKFrameCopyProvisionalURL(frame));

    test1Done = true;
}

static void decidePolicyForNavigationAction(WKPageRef page, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    TEST_ASSERT(!state->didStartProvisionalLoadForFrame);
    TEST_ASSERT(!state->didCommitLoadForFrame);

    state->didDecidePolicyForNavigationAction = true;

    WKFramePolicyListenerUse(listener);
}

static void decidePolicyForNewWindowAction(WKPageRef page, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void* clientInfo)
{
    WKFramePolicyListenerUse(listener);
}

static void decidePolicyForMIMEType(WKPageRef page, WKStringRef MIMEType, WKURLRef url, WKFrameRef frame, WKFramePolicyListenerRef listener, const void* clientInfo)
{
    WKFramePolicyListenerUse(listener);
}

TEST(WebKit2, PageLoadBasic)
{
    State state;

    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());
    WKRetainPtr<WKPageNamespaceRef> pageNamespace(AdoptWK, WKPageNamespaceCreate(context.get()));
 
    PlatformWebView webView(pageNamespace.get());

    WKPageLoaderClient loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.version = 0;
    loaderClient.clientInfo = &state;
    loaderClient.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    loaderClient.didCommitLoadForFrame = didCommitLoadForFrame;
    loaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    WKPageSetPageLoaderClient(webView.page(), &loaderClient);

    WKPagePolicyClient policyClient;
    memset(&policyClient, 0, sizeof(policyClient));

    policyClient.version = 0;
    policyClient.clientInfo = &state;
    policyClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    policyClient.decidePolicyForNewWindowAction = decidePolicyForNewWindowAction;
    policyClient.decidePolicyForMIMEType = decidePolicyForMIMEType;
    WKPageSetPagePolicyClient(webView.page(), &policyClient);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&test1Done);
}

} // namespace TestWebKitAPI
