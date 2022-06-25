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

// FIXME: This should also test the that the load state after didFailLoadWithErrorForFrame is kWKFrameLoadStateFinished

static bool testDone;

static void didFailProvisionalNavigation(WKPageRef page, WKNavigationRef, WKErrorRef error, WKTypeRef userData, const void* clientInfo)
{
    auto frame = WKPageGetMainFrame(page);
    EXPECT_EQ(static_cast<uint32_t>(kWKFrameLoadStateFinished), WKFrameGetFrameLoadState(frame));

    WKURLRef url = WKFrameCopyProvisionalURL(frame);
    EXPECT_NULL(url);

    testDone = true;
}

TEST(WebKit, FailedLoad)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didFailProvisionalNavigation = didFailProvisionalNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::URLForNonExistentResource());
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&testDone);
}

} // namespace TestWebKitAPI

#endif
