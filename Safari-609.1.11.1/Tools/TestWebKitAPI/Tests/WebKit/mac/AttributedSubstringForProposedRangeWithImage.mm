/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKRetainPtr.h>

@interface WKView ()
- (void)attributedSubstringForProposedRange:(NSRange)nsRange completionHandler:(void(^)(NSAttributedString *attrString, NSRange actualRange))completionHandlerPtr;
@end

namespace TestWebKitAPI {

static bool didFinishLoad = false;
static bool didFinishTest = false;
static bool didObserveWebProcessToCrash = false;
static bool didReceiveInvalidMessage = false;

static void didFinishNavigation(WKPageRef, WKNavigationRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void processDidCrash(WKPageRef, const void*)
{
    didObserveWebProcessToCrash = true;
}

static void invalidMessageFunction(WKStringRef messageName)
{
    didReceiveInvalidMessage = true;
}

TEST(WebKit, AttributedSubstringForProposedRangeWithImage)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextWithInjectedBundle());
    WKRetainPtr<WKPageGroupRef> pageGroup = adoptWK(WKPageGroupCreateWithIdentifier(Util::toWK("AttributedSubstringForProposedRangeWithImagePageGroup").get()));
    PlatformWebView webView(context.get(), pageGroup.get());

    WKPageNavigationClientV0 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));
    loaderClient.base.version = 0;
    loaderClient.didFinishNavigation = didFinishNavigation;
    loaderClient.webProcessDidCrash = processDidCrash;
    WKPageSetPageNavigationClient(webView.page(), &loaderClient.base);

    WKContextSetInvalidMessageFunction(invalidMessageFunction);

    WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("chinese-character-with-image", "html"));
    WKPageLoadURL(webView.page(), url.get());
    Util::run(&didFinishLoad);

    [webView.platformView() attributedSubstringForProposedRange:NSMakeRange(0, 3) completionHandler: ^(NSAttributedString *attrString, NSRange actualRange) {
        didFinishTest = true;
    }];

    Util::run(&didFinishTest);
    EXPECT_EQ(didObserveWebProcessToCrash, false);
    EXPECT_EQ(didReceiveInvalidMessage, false);
}

} // namespace TestWebKitAPI
