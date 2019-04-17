/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static unsigned testNumber = 0;
static bool done = false;

static void runJavaScriptAlertEnabled(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{   
    ASSERT_NOT_NULL(frame);    
    EXPECT_EQ(page, WKFrameGetPage(frame));
    
    ++testNumber;
    EXPECT_WK_STREQ("PASS: [OverrideBuiltins] was not disabled", alertText);
    if (testNumber == 2)
        done = true;
}

TEST(WebKit, InjectedBundleNoDisableOverrideBuiltinsBehaviorTest)
{
    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(WKStringCreateWithUTF8CString("InjectedBundleNoDisableOverrideBuiltinsBehaviorTestPageGroup")));

    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("InjectedBundleNoDisableOverrideBuiltinsBehaviorTest", pageGroup.get()));
    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageUIClientV0 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 0;
    uiClient.runJavaScriptAlert = runJavaScriptAlertEnabled;

    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    testNumber = 0;
    done = false;
    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("override-builtins-test", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&done);
}

static void runJavaScriptAlertDisabled(WKPageRef page, WKStringRef alertText, WKFrameRef frame, const void* clientInfo)
{   
    ASSERT_NOT_NULL(frame);    
    EXPECT_EQ(page, WKFrameGetPage(frame));

    ++testNumber;
    EXPECT_WK_STREQ("PASS: [OverrideBuiltins] was disabled", alertText);
    if (testNumber == 2)
        done = true;
}

TEST(WebKit, InjectedBundleDisableOverrideBuiltinsBehaviorTest)
{
    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(WKStringCreateWithUTF8CString("InjectedBundleDisableOverrideBuiltinsBehaviorTestPageGroup")));

    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("InjectedBundleDisableOverrideBuiltinsBehaviorTest", pageGroup.get()));
    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageUIClientV0 uiClient;
    memset(&uiClient, 0, sizeof(uiClient));

    uiClient.base.version = 0;
    uiClient.runJavaScriptAlert = runJavaScriptAlertDisabled;

    WKPageSetPageUIClient(webView.page(), &uiClient.base);

    testNumber = 0;
    done = false;
    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("override-builtins-test", "html"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&done);
}

} // namespace TestWebKitAPI

#endif
