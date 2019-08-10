/*
 * Copyright (C) 2013 Adenilson Cavalcanti <cavalcantii@gmail.com>
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
#include <WebKit/WKPagePrivate.h>
#include <WebKit/WKRetainPtr.h>
#include <signal.h>

namespace TestWebKitAPI {

static bool loadBeforeCrash = false;
static bool loadAfterCrash = false;
static bool calledCrashHandler = false;

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

    // Reload should re-spawn webprocess.
    WKPageReload(page);
}

TEST(WebKit, ReloadPageAfterCrash)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishLoad;
    loaderClient.webProcessDidCrash = didCrash;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    // Load a blank page and next kills WebProcess.
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadBeforeCrash);
    WKPageTerminate(webView.page());

    // Let's try load a page and see what happens.
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadAfterCrash);
}

#if !PLATFORM(WIN)

static void nullJavaScriptCallback(WKSerializedScriptValueRef, WKErrorRef, void*)
{
}

static void didCrashCheckFrames(WKPageRef page, const void*)
{
    // Test if first load actually worked.
    EXPECT_TRUE(loadBeforeCrash);

    EXPECT_TRUE(!WKPageGetMainFrame(page));
    EXPECT_TRUE(!WKPageGetFocusedFrame(page));
    EXPECT_TRUE(!WKPageGetFrameSetLargestFrame(page));

    calledCrashHandler = true;
}

TEST(WebKit, FocusedFrameAfterCrash)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishLoad;
    loaderClient.webProcessDidCrash = didCrashCheckFrames;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("many-same-origin-iframes", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadBeforeCrash);

    EXPECT_FALSE(!WKPageGetMainFrame(webView.page()));

    WKRetainPtr<WKStringRef> javaScriptString = adoptWK(WKStringCreateWithUTF8CString("frames[2].focus()"));
    WKPageRunJavaScriptInMainFrame(webView.page(), javaScriptString.get(), 0, nullJavaScriptCallback);

    while (!WKPageGetFocusedFrame(webView.page()))
        Util::spinRunLoop(10);

    kill(WKPageGetProcessIdentifier(webView.page()), 9);

    Util::run(&calledCrashHandler);
}

TEST(WebKit, FrameSetLargestFrameAfterCrash)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishLoad;
    loaderClient.webProcessDidCrash = didCrashCheckFrames;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> baseURL = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    WKRetainPtr<WKStringRef> htmlString = Util::toWK("<frameset cols='25%,*,25%'><frame src='about:blank'><frame src='about:blank'><frame src='about:blank'></frameset>");

    WKPageLoadHTMLString(webView.page(), htmlString.get(), baseURL.get());
    Util::run(&loadBeforeCrash);

    EXPECT_FALSE(!WKPageGetMainFrame(webView.page()));

    while (!WKPageGetFrameSetLargestFrame(webView.page()))
        Util::spinRunLoop(10);

    kill(WKPageGetProcessIdentifier(webView.page()), 9);

    Util::run(&calledCrashHandler);
}

#endif // !PLATFORM(WIN)

} // namespace TestWebKitAPI

#endif
