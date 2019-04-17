/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include <WebKit/WKFrame.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool loadedAllFrames;

static bool performedServerRedirect;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    loadedAllFrames = true;
}

static void didPerformServerRedirect(WKContextRef context, WKPageRef page, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef frame, const void *clientInfo)
{
    performedServerRedirect = true;
}

TEST(WebKit, LoadCanceledNoServerRedirectCallback)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("LoadCanceledNoServerRedirectCallbackTest"));
    
    WKContextInjectedBundleClientV0 injectedBundleClient;
    memset(&injectedBundleClient, 0, sizeof(injectedBundleClient));

    injectedBundleClient.base.version = 0;

    WKContextSetInjectedBundleClient(context.get(), &injectedBundleClient.base);

    PlatformWebView webView(context.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    
    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;

    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);
    
    WKContextHistoryClientV0 historyClient;
    memset(&historyClient, 0, sizeof(historyClient));
    
    historyClient.base.version = 0;
    historyClient.didPerformServerRedirect = didPerformServerRedirect;

    WKContextSetHistoryClient(context.get(), &historyClient.base);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple-iframe", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&loadedAllFrames);
    
    // We shouldn't have performed a server redirect when the iframe load was cancelled.
    EXPECT_FALSE(performedServerRedirect);
}

} // namespace TestWebKitAPI

#endif
