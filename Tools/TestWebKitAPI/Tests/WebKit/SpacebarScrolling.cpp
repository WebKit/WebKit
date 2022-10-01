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

#include "config.h"

#if WK_HAVE_C_SPI

#include "JavaScriptTest.h"
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include <WebKit/WKPreferencesRefPrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKSerializedScriptValue.h>

namespace TestWebKitAPI {

static bool didFinishLoad;
static bool didNotHandleKeyDownEvent;
static bool javascriptRun;
static bool isScrolled;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void didNotHandleKeyEventCallback(WKPageRef, WKNativeEventPtr event, const void*)
{
    if (Util::isKeyDown(event))
        didNotHandleKeyDownEvent = true;
}


static void didRunJavascript(WKSerializedScriptValueRef serializedScriptValue, WKErrorRef error, void* context)
{
    JSGlobalContextRef scriptContext = JSGlobalContextCreate(0);
    JSValueRef jsValue = WKSerializedScriptValueDeserialize(serializedScriptValue, scriptContext, 0);
    isScrolled = JSValueToBoolean(scriptContext, jsValue);

    javascriptRun = true;

    JSGlobalContextRelease(scriptContext);
}

TEST(WebKit, SpacebarScrolling)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextWithInjectedBundle());

    // Turn off threaded scrolling; synchronously waiting for the main thread scroll position to
    // update using WKPageForceRepaint would be better, but for some reason the test still fails occasionally.
    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("NoThreadedScrollingPageGroup").get()));
    WKPreferencesRef preferences = WKPageGroupGetPreferences(pageGroup.get());
    WKPreferencesSetThreadedScrollingEnabled(preferences, false);

    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKPageUIClientV0 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 0;
    uiClient.didNotHandleKeyEvent = didNotHandleKeyEventCallback;

    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("spacebar-scrolling", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    EXPECT_JS_FALSE(webView.page(), "isDocumentScrolled()");
    EXPECT_JS_FALSE(webView.page(), "textFieldContainsSpace()");

    webView.simulateSpacebarKeyPress();

    EXPECT_JS_FALSE(webView.page(), "isDocumentScrolled()");
    EXPECT_JS_TRUE(webView.page(), "textFieldContainsSpace()");

    // On Mac, a key down event represents both a raw key down and a key press.
    // We expect the key press to be handled (because it inserts text into the text field),
    // but the raw key down should not be handled.
#if PLATFORM(COCOA)
    EXPECT_FALSE(didNotHandleKeyDownEvent);
#endif

    EXPECT_JS_EQ(webView.page(), "blurTextField()", "undefined");

    didNotHandleKeyDownEvent = false;
    webView.simulateSpacebarKeyPress();

    while (!isScrolled) {
        javascriptRun = false;
        WKPageRunJavaScriptInMainFrame(webView.page(), Util::toWK("isDocumentScrolled()").get(), 0, didRunJavascript);
        Util::run(&javascriptRun);
    }

    EXPECT_JS_TRUE(webView.page(), "isDocumentScrolled()");
    EXPECT_JS_TRUE(webView.page(), "textFieldContainsSpace()");

#if PLATFORM(COCOA)
    EXPECT_FALSE(didNotHandleKeyDownEvent);
#endif
}

} // namespace TestWebKitAPI

#endif
