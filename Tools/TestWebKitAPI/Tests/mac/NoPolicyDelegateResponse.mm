/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "PlatformUtilities.h"
#import <WebKit/WebViewPrivate.h>
#import <wtf/RetainPtr.h>

static bool didFinishLoad;
static bool didNavigationActionCheck;
static bool didNavigationResponseCheck;
static bool didStartProvisionalLoad;

@interface NoDecidePolicyForMIMETypeDecisionDelegate : NSObject <WebPolicyDelegate, WebFrameLoadDelegate> {
}
@end

@implementation NoDecidePolicyForMIMETypeDecisionDelegate

- (void)webView:(WebView *)webView decidePolicyForMIMEType:(NSString *)type request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    // Implements decidePolicyForMIMEType but fails to call the decision listener.
    didNavigationResponseCheck = YES;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    didFinishLoad = true;
}

@end

@interface NoDecidePolicyForNavigationActionDecisionDelegate : NSObject <WebPolicyDelegate, WebFrameLoadDelegate> {
}
@end

@implementation NoDecidePolicyForNavigationActionDecisionDelegate

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id<WebPolicyDecisionListener>)listener
{
    // Implements decidePolicyForNavigationAction but fails to call the decision listener.
    didNavigationActionCheck = YES;
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame
{
    didStartProvisionalLoad = true;
}

@end

namespace TestWebKitAPI {

TEST(WebKitLegacy, NoDecidePolicyForMIMETypeDecision)
{
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSZeroRect frameName:nil groupName:nil]);
    auto delegate = adoptNS([NoDecidePolicyForMIMETypeDecisionDelegate new]);

    webView.get().frameLoadDelegate = delegate.get();
    webView.get().policyDelegate = delegate.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"verboseMarkup" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didFinishLoad);

    EXPECT_TRUE(didNavigationResponseCheck);
}

TEST(WebKitLegacy, NoDecidePolicyForNavigationActionDecision)
{
    auto webView = adoptNS([[WebView alloc] initWithFrame:NSZeroRect frameName:nil groupName:nil]);
    auto delegate = adoptNS([NoDecidePolicyForNavigationActionDecisionDelegate new]);

    webView.get().frameLoadDelegate = delegate.get();
    webView.get().policyDelegate = delegate.get();
    [[webView.get() mainFrame] loadRequest:[NSURLRequest requestWithURL:[[NSBundle mainBundle] URLForResource:@"verboseMarkup" withExtension:@"html" subdirectory:@"TestWebKitAPI.resources"]]];

    Util::run(&didNavigationActionCheck);

    EXPECT_FALSE(didStartProvisionalLoad);
}

} // namespace TestWebKitAPI
