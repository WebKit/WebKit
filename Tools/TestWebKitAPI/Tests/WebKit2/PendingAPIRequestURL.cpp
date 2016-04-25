/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKit2, PendingAPIRequestURL)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreate());
    PlatformWebView webView(context.get());

    WKRetainPtr<WKURLRef> activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    EXPECT_NULL(activeURL.get());

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple", "html"));
    WKPageLoadURL(webView.page(), url.get());
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), url.get()));
    WKPageStopLoading(webView.page());

    WKRetainPtr<WKStringRef> htmlString = Util::toWK("<body>Hello, World</body>");
    WKRetainPtr<WKURLRef> blankURL = adoptWK(WKURLCreateWithUTF8CString("about:blank"));
    WKPageLoadHTMLString(webView.page(), htmlString.get(), nullptr);
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), blankURL.get()));
    WKPageStopLoading(webView.page());

    url = adoptWK(WKURLCreateWithUTF8CString("http://www.webkit.org"));
    WKPageLoadHTMLString(webView.page(), htmlString.get(), url.get());
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), url.get()));
    WKPageStopLoading(webView.page());

    WKRetainPtr<WKDataRef> data = adoptWK(WKDataCreate(nullptr, 0));
    WKPageLoadData(webView.page(), data.get(), nullptr, nullptr, nullptr);
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), blankURL.get()));
    WKPageStopLoading(webView.page());

    WKPageLoadData(webView.page(), data.get(), nullptr, nullptr, url.get());
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), url.get()));
    WKPageStopLoading(webView.page());

    WKPageLoadAlternateHTMLString(webView.page(), htmlString.get(), nullptr, url.get());
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), url.get()));
    WKPageStopLoading(webView.page());

    WKRetainPtr<WKStringRef> plainTextString = Util::toWK("Hello, World");
    WKPageLoadPlainTextString(webView.page(), plainTextString.get());
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), blankURL.get()));
    WKPageStopLoading(webView.page());

    url = adoptWK(WKURLCreateWithUTF8CString("file:///tmp/index.html"));
    WKPageLoadFile(webView.page(), url.get(), nullptr);
    activeURL = adoptWK(WKPageCopyActiveURL(webView.page()));
    ASSERT_NOT_NULL(activeURL.get());
    EXPECT_TRUE(WKURLIsEqual(activeURL.get(), url.get()));
    WKPageStopLoading(webView.page());
}

} // namespace TestWebKitAPI
