/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Igalia S.L.
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

#include <WebKit/WKContext.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {
    
static bool didFinishLoad = false;
    
static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    didFinishLoad = true;
}

static void loadAlternateHTMLString(WKURLRef baseURL, WKURLRef unreachableURL)
{
    WKRetainPtr<WKContextRef> context(AdoptWK, WKContextCreateWithConfiguration(nullptr));
    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    
    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKRetainPtr<WKStringRef> alternateHTMLString(AdoptWK, WKStringCreateWithUTF8CString("<html><body><img src='icon.png'></body></html>"));
    WKPageLoadAlternateHTMLString(webView.page(), alternateHTMLString.get(), baseURL, unreachableURL);

    // If we can finish loading the html without resulting in an invalid message being sent from the WebProcess, this test passes.
    Util::run(&didFinishLoad);
}

TEST(WebKit, LoadAlternateHTMLStringWithNonDirectoryURL)
{
    // Call WKPageLoadAlternateHTMLString() with fileURL which does not point to a directory.
    WKRetainPtr<WKURLRef> fileURL(AdoptWK, Util::createURLForResource("simple", "html"));
    loadAlternateHTMLString(fileURL.get(), fileURL.get());
}

TEST(WebKit, LoadAlternateHTMLStringWithEmptyBaseURL)
{
    // Call WKPageLoadAlternateHTMLString() with empty baseURL to make sure this test works
    // when baseURL does not grant read access to the unreachableURL. We use a separate test
    // to ensure the previous test does not pollute the result.
    WKRetainPtr<WKURLRef> unreachableURL(AdoptWK, Util::URLForNonExistentResource());
    loadAlternateHTMLString(nullptr, unreachableURL.get());
}

} // namespace TestWebKitAPI

#endif
