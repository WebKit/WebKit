/*
 * Copyright (C) 2016 Igalia S.L.
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
#include <WebKit/WKContext.h>
#include <WebKit/WKFrame.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool committedLoad;

static void didCommitNavigationCallback(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void*)
{
    WKFrameRef frame = WKPageGetMainFrame(page);

    // The provisional URL should be null.
    EXPECT_NULL(WKFrameCopyProvisionalURL(frame));

    // The committed URL is the last known provisional URL.
    WKRetainPtr<WKURLRef> committedURL = adoptWK(WKFrameCopyURL(frame));
    ASSERT_NOT_NULL(committedURL.get());
    WKRetainPtr<WKURLRef> activeURL = adoptWK(WKPageCopyActiveURL(page));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(committedURL.get(), activeURL.get()));
    assert(WKGetTypeID(userData) == WKURLGetTypeID());
    EXPECT_TRUE(WKURLIsEqual(committedURL.get(), static_cast<WKURLRef>(userData)));

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("simple2", "html"));
    EXPECT_TRUE(WKURLIsEqual(committedURL.get(), url.get()));

    committedLoad = true;
}

TEST(WebKit2, ProvisionalURLAfterWillSendRequestCallback)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, Util::createContextForInjectedBundleTest("ProvisionalURLAfterWillSendRequestCallbackTest"));

    WKContextInjectedBundleClientV0 injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));
    injectedBundleClient.base.version = 0;
    WKContextSetInjectedBundleClient(context.get(), &injectedBundleClient.base);

    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 navigationClient;
    memset(&navigationClient, 0, sizeof(navigationClient));

    navigationClient.base.version = 0;
    navigationClient.didCommitNavigation = didCommitNavigationCallback;
    WKPageSetPageNavigationClient(webView.page(), &navigationClient.base);

    WKRetainPtr<WKURLRef> url(AdoptWK, Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&committedLoad);
}

} // namespace TestWebKitAPI

#endif
