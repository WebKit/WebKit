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

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool testDone;

struct State {
    State()
        : didStartProvisionalNavigation(false)
        , didCommitNavigation(false)
    {
    }

    bool didStartProvisionalNavigation;
    bool didCommitNavigation;
};

static void didStartProvisionalNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    EXPECT_FALSE(state->didCommitNavigation);

    EXPECT_FALSE(state->didStartProvisionalNavigation);

    state->didStartProvisionalNavigation = true;
}

static void didCommitNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    EXPECT_TRUE(state->didStartProvisionalNavigation);

    state->didCommitNavigation = true;
}

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    State* state = reinterpret_cast<State*>(const_cast<void*>(clientInfo));
    EXPECT_TRUE(state->didStartProvisionalNavigation);
    EXPECT_TRUE(state->didCommitNavigation);

    WKRetainPtr<WKURLRef> url = adoptWK(WKFrameCopyURL(WKPageGetMainFrame(page)));
    EXPECT_TRUE(WKURLIsEqual(url.get(), adoptWK(WKURLCreateWithUTF8CString("about:blank")).get()));

    testDone = true;
}

TEST(WebKit, PageLoadEmptyURL)
{
    State state;

    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    zeroBytes(loaderClient);

    loaderClient.base.version = 0;
    loaderClient.base.clientInfo = &state;
    loaderClient.didStartProvisionalNavigation = didStartProvisionalNavigation;
    loaderClient.didCommitNavigation = didCommitNavigation;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(WKURLCreateWithUTF8CString(""));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&testDone);
}

} // namespace TestWebKitAPI

#endif
