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

#import "config.h"

#import "JavaScriptTest.h"
#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "Test.h"
#import <WebKit/WKRetainPtr.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static bool loadBeforeCrash = false;
static bool loadAfterCrash = false;
static bool isWaitingForPageSignalToContinue = false;
static bool didGetPageSignalToContinue = false;

static void runJavaScriptAlert(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{
    EXPECT_TRUE(isWaitingForPageSignalToContinue);
    isWaitingForPageSignalToContinue = false;
    didGetPageSignalToContinue = true;
}

static void didFinishLoad(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    // First load before WebProcess was terminated.
    if (!loadBeforeCrash) {
        loadBeforeCrash = true;
        return;
    }

    // Next load after WebProcess was terminated (hopefully
    // it will be correctly re-spawned).
    EXPECT_EQ(static_cast<uint32_t>(kWKFrameLoadStateFinished), WKFrameGetFrameLoadState(WKPageGetMainFrame(page)));
    EXPECT_FALSE(loadAfterCrash);

    // Set it, otherwise the loop will not end.
    loadAfterCrash = true;
}

static void didCrash(WKPageRef page, const void*)
{
    // Test if first load actually worked.
    EXPECT_TRUE(loadBeforeCrash);
}

TEST(WebKit, RestoreStateAfterTermination)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    // Add view to a Window and make it visible.
    RetainPtr<NSWindow> window = adoptNS([[NSWindow alloc] initWithContentRect:webView.platformView().frame styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);
    [window.get().contentView addSubview:webView.platformView()];

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishLoad;
    loaderClient.webProcessDidCrash = didCrash;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageUIClientV0 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));
    uiClient.base.version = 0;
    uiClient.runJavaScriptAlert = runJavaScriptAlert;
    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    // Load a test page containing a form.
    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("simple-form", "html")).get());
    Util::run(&loadBeforeCrash);

    // Make the page visible.
    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    [window.get() makeKeyAndOrderFront:nil];
    Util::run(&didGetPageSignalToContinue);
    EXPECT_JS_EQ(webView.page(), "document.visibilityState", "visible");

    EXPECT_JS_EQ(webView.page(), "getFormData()", "Some unimportant data");

    // Simulate user entering form data.
    EXPECT_JS_EQ(webView.page(), "fillForm('Important data')", "undefined");

    EXPECT_JS_EQ(webView.page(), "getFormData()", "Important data");

    // Unparent the view so that it is no longer visible. This should cause us to save document state.
    isWaitingForPageSignalToContinue = true;
    didGetPageSignalToContinue = false;
    [webView.platformView() removeFromSuperview];
    Util::run(&didGetPageSignalToContinue);

    EXPECT_JS_EQ(webView.page(), "document.visibilityState", "hidden");

    // Terminate page while in the background.
    WKPageTerminate(webView.page());

    // Reload should re-spawn webprocess and restore document state.
    WKPageReload(webView.page());
    Util::run(&loadAfterCrash);

    // Make sure that the form data is restored after the reload.
    EXPECT_JS_EQ(webView.page(), "getFormData()", "Important data");
}

} // namespace TestWebKitAPI
