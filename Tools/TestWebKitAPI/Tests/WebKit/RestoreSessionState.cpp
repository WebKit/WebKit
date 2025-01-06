/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "config.h"

#if WK_HAVE_C_SPI

#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKSessionStateRef.h>
#include <array>
#include <wtf/RunLoop.h>

namespace TestWebKitAPI {

static bool didFinishLoad;
static bool didDecideNavigationPolicy;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void decidePolicyForNavigationAction(WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKFrameRef originatingFrame, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
{
    WKRetainPtr<WKFramePolicyListenerRef> retainedListener(listener);
    RunLoop::main().dispatch([retainedListener = WTFMove(retainedListener)] {
        WKFramePolicyListenerUse(retainedListener.get());
    });
}

static void decidePolicyForNavigationActionIgnore(WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKFrameRef originatingFrame, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
{
    WKRetainPtr<WKFramePolicyListenerRef> retainedListener(listener);
    RunLoop::main().dispatch([retainedListener = WTFMove(retainedListener), page = WTFMove(page)] {
        EXPECT_NOT_NULL(adoptWK(WKPageCopyPendingAPIRequestURL(page)));
        WKFramePolicyListenerIgnore(retainedListener.get());
        didDecideNavigationPolicy = true;
    });
}

static void decidePolicyForResponse(WKPageRef page, WKFrameRef frame, WKURLResponseRef response, WKURLRequestRef request, bool canShowMIMEType, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
{
    WKRetainPtr<WKFramePolicyListenerRef> retainedListener(listener);
    RunLoop::main().dispatch([retainedListener = WTFMove(retainedListener)] {
        WKFramePolicyListenerUse(retainedListener.get());
    });
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV0 loaderClient;
    zeroBytes(loaderClient);

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

static WKRetainPtr<WKDataRef> createSessionStateData(WKContextRef context)
{
    PlatformWebView webView(context);
    setPageLoaderClient(webView.page());

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple-form", "html")).get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    auto sessionState = adoptWK(static_cast<WKSessionStateRef>(WKPageCopySessionState(webView.page(), reinterpret_cast<void*>(1), nullptr)));
    return adoptWK(WKSessionStateCopyData(sessionState.get()));
}

TEST(WebKit, RestoreSessionStateContainingScrollRestorationDefault)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKRetainPtr<WKDataRef> data = createSessionStateData(context.get());
    EXPECT_NOT_NULL(data);

    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionState(webView.page(), sessionState.get());

    Util::run(&didFinishLoad);

    EXPECT_JS_EQ(webView.page(), "history.scrollRestoration", "auto");
}

TEST(WebKit, RestoreSessionStateContainingScrollRestorationDefaultWithAsyncPolicyDelegates)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));

    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKPagePolicyClientV1 policyClient;
    zeroBytes(policyClient);
    policyClient.base.version = 1;
    policyClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    policyClient.decidePolicyForResponse = decidePolicyForResponse;
    WKPageSetPagePolicyClient(webView.page(), &policyClient.base);

    WKRetainPtr<WKDataRef> data = createSessionStateData(context.get());
    EXPECT_NOT_NULL(data);

    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionState(webView.page(), sessionState.get());

    Util::run(&didFinishLoad);

    EXPECT_JS_EQ(webView.page(), "history.scrollRestoration", "auto");
}

TEST(WebKit, RestoreSessionStateAfterClose)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());
    auto data = createSessionStateData(context.get());
    EXPECT_NOT_NULL(data);
    WKPageClose(webView.page());
    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionState(webView.page(), sessionState.get());
}

TEST(WebKit, PendingURLAfterRestoreSessionState)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());
    auto data = createSessionStateData(context.get());
    EXPECT_NOT_NULL(data);
    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionState(webView.page(), sessionState.get());
    auto pendingURL = adoptWK(WKPageCopyPendingAPIRequestURL(webView.page()));
    EXPECT_NOT_NULL(pendingURL);
    if (!pendingURL)
        return;

    auto expectedURL = adoptWK(Util::createURLForResource("simple-form", "html"));
    EXPECT_WK_STREQ(adoptWK(WKURLCopyString(expectedURL.get())).get(), adoptWK(WKURLCopyString(pendingURL.get())).get());
}

TEST(WebKit, ClearedPendingURLAfterCancelingRestoreSessionState)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKPagePolicyClientV1 policyClient;
    zeroBytes(policyClient);
    policyClient.base.version = 1;
    policyClient.decidePolicyForNavigationAction = decidePolicyForNavigationActionIgnore;
    WKPageSetPagePolicyClient(webView.page(), &policyClient.base);

    auto data = createSessionStateData(context.get());
    EXPECT_NOT_NULL(data);
    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionState(webView.page(), sessionState.get());
    Util::run(&didDecideNavigationPolicy);
    didDecideNavigationPolicy = false;
    EXPECT_NULL(adoptWK(WKPageCopyPendingAPIRequestURL(webView.page())));
}

// V2 SessionState that does not contain "SessionHistoryEntryData" keys.
static const uint8_t sessionStateWithoutHistoryEntryData[]  = {
    0x00, 0x00, 0x00, 0x02, 0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x30, 0x30, 0xd3, 0x01, 0x02, 0x03,
    0x04, 0x18, 0x19, 0x5e, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x48, 0x69, 0x73, 0x74, 0x6f,
    0x72, 0x79, 0x5e, 0x49, 0x73, 0x41, 0x70, 0x70, 0x49, 0x6e, 0x69, 0x74, 0x69, 0x61, 0x74, 0x65,
    0x64, 0x5e, 0x52, 0x65, 0x6e, 0x64, 0x65, 0x72, 0x54, 0x72, 0x65, 0x65, 0x53, 0x69, 0x7a, 0x65,
    0xd3, 0x05, 0x06, 0x07, 0x08, 0x17, 0x0f, 0x5f, 0x10, 0x15, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f,
    0x6e, 0x48, 0x69, 0x73, 0x74, 0x6f, 0x72, 0x79, 0x45, 0x6e, 0x74, 0x72, 0x69, 0x65, 0x73, 0x5f,
    0x10, 0x1a, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x48, 0x69, 0x73, 0x74, 0x6f, 0x72, 0x79,
    0x43, 0x75, 0x72, 0x72, 0x65, 0x6e, 0x74, 0x49, 0x6e, 0x64, 0x65, 0x78, 0x5f, 0x10, 0x15, 0x53,
    0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x48, 0x69, 0x73, 0x74, 0x6f, 0x72, 0x79, 0x56, 0x65, 0x72,
    0x73, 0x69, 0x6f, 0x6e, 0xa4, 0x09, 0x11, 0x13, 0x15, 0xd4, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x10, 0x5f, 0x10, 0x18, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x48, 0x69, 0x73, 0x74,
    0x6f, 0x72, 0x79, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x54, 0x69, 0x74, 0x6c, 0x65, 0x5f, 0x10, 0x32,
    0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x48, 0x69, 0x73, 0x74, 0x6f, 0x72, 0x79, 0x45, 0x6e,
    0x74, 0x72, 0x79, 0x53, 0x68, 0x6f, 0x75, 0x6c, 0x64, 0x4f, 0x70, 0x65, 0x6e, 0x45, 0x78, 0x74,
    0x65, 0x72, 0x6e, 0x61, 0x6c, 0x55, 0x52, 0x4c, 0x73, 0x50, 0x6f, 0x6c, 0x69, 0x63, 0x79, 0x4b,
    0x65, 0x79, 0x5f, 0x10, 0x16, 0x53, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x48, 0x69, 0x73, 0x74,
    0x6f, 0x72, 0x79, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x55, 0x52, 0x4c, 0x5f, 0x10, 0x1e, 0x53, 0x65,
    0x73, 0x73, 0x69, 0x6f, 0x6e, 0x48, 0x69, 0x73, 0x74, 0x6f, 0x72, 0x79, 0x45, 0x6e, 0x74, 0x72,
    0x79, 0x4f, 0x72, 0x69, 0x67, 0x69, 0x6e, 0x61, 0x6c, 0x55, 0x52, 0x4c, 0x50, 0x10, 0x01, 0x5c,
    0x66, 0x61, 0x76, 0x6f, 0x72, 0x69, 0x74, 0x65, 0x73, 0x3a, 0x2f, 0x2f, 0xd4, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f, 0x12, 0x12, 0x5f, 0x10, 0x13, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x65,
    0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0xd4, 0x0a, 0x0b, 0x0c, 0x0d,
    0x0e, 0x0f, 0x14, 0x14, 0x5f, 0x10, 0x15, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x77,
    0x77, 0x77, 0x2e, 0x69, 0x61, 0x6e, 0x61, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0xd4, 0x0a, 0x0b, 0x0c,
    0x0d, 0x0e, 0x0f, 0x16, 0x16, 0x5f, 0x10, 0x13, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f,
    0x77, 0x65, 0x62, 0x6b, 0x69, 0x74, 0x2e, 0x6f, 0x72, 0x67, 0x2f, 0x10, 0x03, 0x09, 0x11, 0x01,
    0x8b, 0x00, 0x08, 0x00, 0x0f, 0x00, 0x1e, 0x00, 0x2d, 0x00, 0x3c, 0x00, 0x43, 0x00, 0x5b, 0x00,
    0x78, 0x00, 0x90, 0x00, 0x95, 0x00, 0x9e, 0x00, 0xb9, 0x00, 0xee, 0x01, 0x07, 0x01, 0x28, 0x01,
    0x29, 0x01, 0x2b, 0x01, 0x38, 0x01, 0x41, 0x01, 0x57, 0x01, 0x60, 0x01, 0x78, 0x01, 0x81, 0x01,
    0x97, 0x01, 0x99, 0x01, 0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x9d
};

TEST(WebKit, CanRestoreSessionStateWithoutHistoryEntryData)
{
    auto context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    auto data = adoptWK(WKDataCreate(sessionStateWithoutHistoryEntryData, sizeof(sessionStateWithoutHistoryEntryData)));
    ASSERT_NOT_NULL(data);

    auto sessionState = adoptWK(WKSessionStateCreateFromData(data.get()));
    WKPageRestoreFromSessionState(webView.page(), sessionState.get());

    auto list = WKPageGetBackForwardList(webView.page());
    ASSERT_EQ(WKBackForwardListGetBackListCount(list), 3u);

    constexpr std::array<const char *, 4> expectedURLStrings = { {
        "https://webkit.org/",
        "https://www.iana.org/",
        "http://example.com/",
        "favorites://",
    } };

    int i = 0;
    for (auto expectedURLString : expectedURLStrings) {
        auto item = WKBackForwardListGetItemAtIndex(list, i--);
        auto url = adoptWK(WKBackForwardListItemCopyURL(item));
        auto expectedURL = adoptWK(WKURLCreateWithUTF8CString(expectedURLString));
        ASSERT_TRUE(WKURLIsEqual(url.get(), expectedURL.get()));
    }
}

} // namespace TestWebKitAPI

#endif
