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
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool testDone;

static void didStartProvisionalNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    WKRetainPtr<WKStringRef> wkMIME = adoptWK(WKFrameCopyMIMEType(WKPageGetMainFrame(page)));
    EXPECT_TRUE(WKStringIsEmpty(wkMIME.get()));
}

static void didCommitNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    WKRetainPtr<WKStringRef> wkMIME = adoptWK(WKFrameCopyMIMEType(WKPageGetMainFrame(page)));
    EXPECT_WK_STREQ("image/png", wkMIME);
}

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    WKRetainPtr<WKStringRef> wkMIME = adoptWK(WKFrameCopyMIMEType(WKPageGetMainFrame(page)));
    EXPECT_WK_STREQ("image/png", wkMIME);

    testDone = true;
}

TEST(WebKit, FrameMIMETypePNG)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 0;
    loaderClient.didStartProvisionalNavigation = didStartProvisionalNavigation;
    loaderClient.didCommitNavigation = didCommitNavigation;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("icon", "png"));
    WKPageLoadURL(webView.page(), url.get());

    Util::run(&testDone);
}

} // namespace TestWebKitAPI

#endif
