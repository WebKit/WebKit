/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "config.h"

#if WK_HAVE_C_SPI && PLATFORM(MAC)

#import "PlatformUtilities.h"
#import "PlatformWebView.h"
#import "TestBrowsingContextLoadDelegate.h"
#import <wtf/RetainPtr.h>

static bool testFinished = false;

@interface LoadInvalidSchemeDelegate : NSObject <WKBrowsingContextLoadDelegate>
@end

@implementation LoadInvalidSchemeDelegate

- (void)browsingContextController:(WKBrowsingContextController *)sender didFailProvisionalLoadWithError:(NSError *)error
{
    testFinished = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKit2CustomProtocolsTest, RegisterNilScheme)
{
    [WKBrowsingContextController registerSchemeForCustomProtocol:nil];
    [WKBrowsingContextController unregisterSchemeForCustomProtocol:nil];
}

TEST(WebKit2CustomProtocolsTest, LoadInvalidScheme)
{
    [WKBrowsingContextController registerSchemeForCustomProtocol:@"custom"];
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("CustomProtocolInvalidSchemeTest"));
    PlatformWebView webView(context.get());

    auto loadDelegate = adoptNS([[LoadInvalidSchemeDelegate alloc] init]);
    webView.platformView().browsingContextController.loadDelegate = loadDelegate.get();
    [webView.platformView().browsingContextController loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"ht'tp://www.webkit.org"]]];
    
    Util::run(&testFinished);
}

} // namespace TestWebKitAPI

#endif
