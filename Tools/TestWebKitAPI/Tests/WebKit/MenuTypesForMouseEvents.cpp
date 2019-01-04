/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

namespace TestWebKitAPI {

static bool didFinishLoad;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

static void buildAndPerformTest(WKEventMouseButton button, WKEventModifiers modifiers, const char* expectedButton, const char* expectedMenuType)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("mouse-button-listener", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    didFinishLoad = false;

    webView.simulateButtonClick(button, 10, 10, modifiers);

    EXPECT_JS_EQ(webView.page(), "pressedMouseButton()", expectedButton);
    EXPECT_JS_EQ(webView.page(), "displayedMenu()", expectedMenuType);
}

TEST(WebKit, MenuAndButtonForNormalLeftClick)
{
    buildAndPerformTest(kWKEventMouseButtonLeftButton, 0, "0", "none");
}

TEST(WebKit, MenuAndButtonForNormalRightClick)
{
    buildAndPerformTest(kWKEventMouseButtonRightButton, 0, "2", "context");
}

TEST(WebKit, MenuAndButtonForNormalMiddleClick)
{
    buildAndPerformTest(kWKEventMouseButtonMiddleButton, 0, "1", "none");
}

TEST(WebKit, MenuAndButtonForControlLeftClick)
{
    buildAndPerformTest(kWKEventMouseButtonLeftButton, kWKEventModifiersControlKey, "0", "context");
}

TEST(WebKit, MenuAndButtonForControlRightClick)
{
    buildAndPerformTest(kWKEventMouseButtonRightButton, kWKEventModifiersControlKey, "2", "context");
}

TEST(WebKit, MenuAndButtonForControlMiddleClick)
{
    buildAndPerformTest(kWKEventMouseButtonMiddleButton, kWKEventModifiersControlKey, "1", "none");
}

TEST(WebKit, MenuAndButtonForShiftLeftClick)
{
    buildAndPerformTest(kWKEventMouseButtonLeftButton, kWKEventModifiersShiftKey, "0", "none");
}

TEST(WebKit, MenuAndButtonForShiftRightClick)
{
    buildAndPerformTest(kWKEventMouseButtonRightButton, kWKEventModifiersShiftKey, "2", "context");
}

TEST(WebKit, MenuAndButtonForShiftMiddleClick)
{
    buildAndPerformTest(kWKEventMouseButtonMiddleButton, kWKEventModifiersShiftKey, "1", "none");
}

TEST(WebKit, MenuAndButtonForCommandLeftClick)
{
    buildAndPerformTest(kWKEventMouseButtonLeftButton, kWKEventModifiersMetaKey, "0", "none");
}

TEST(WebKit, MenuAndButtonForCommandRightClick)
{
    buildAndPerformTest(kWKEventMouseButtonRightButton, kWKEventModifiersMetaKey, "2", "context");
}

TEST(WebKit, MenuAndButtonForCommandMiddleClick)
{
    buildAndPerformTest(kWKEventMouseButtonMiddleButton, kWKEventModifiersMetaKey, "1", "none");
}

TEST(WebKit, MenuAndButtonForAltLeftClick)
{
    buildAndPerformTest(kWKEventMouseButtonLeftButton, kWKEventModifiersAltKey, "0", "none");
}

TEST(WebKit, MenuAndButtonForAltRightClick)
{
    buildAndPerformTest(kWKEventMouseButtonRightButton, kWKEventModifiersAltKey, "2", "context");
}

TEST(WebKit, MenuAndButtonForAltMiddleClick)
{
    buildAndPerformTest(kWKEventMouseButtonMiddleButton, kWKEventModifiersAltKey, "1", "none");
}
    
} // namespace TestWebKitAPI

#endif
