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

#import "config.h"

#import "MockContentFilterSettings.h"
#import "PlatformUtilities.h"
#import "TestProtocol.h"
#import <WebKit/WebKit.h>
#import <WebKit/WebKitErrorsPrivate.h>

using Decision = WebCore::MockContentFilterSettings::Decision;
using DecisionPoint = WebCore::MockContentFilterSettings::DecisionPoint;

static bool isDone;

@interface LoadAlternateFrameLoadDelegate : NSObject <WebFrameLoadDelegate>
@end

@implementation LoadAlternateFrameLoadDelegate

- (void)webView:(WebView *)sender didFailProvisionalLoadWithError:(NSError *)error forFrame:(WebFrame *)frame
{
    EXPECT_WK_STREQ(WebKitErrorDomain, error.domain);
    EXPECT_EQ(WebKitErrorFrameLoadBlockedByContentFilter, error.code);
    [frame loadAlternateHTMLString:@"FAIL" baseURL:nil forUnreachableURL:[error.userInfo objectForKey:NSURLErrorFailingURLErrorKey]];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    EXPECT_WK_STREQ(@"blocked", frame.DOMDocument.body.innerText);
    isDone = true;
}

@end

namespace TestWebKitAPI {

static void loadAlternateTest(Decision decision, DecisionPoint decisionPoint)
{
    @autoreleasepool {
        auto& settings = WebCore::MockContentFilterSettings::singleton();
        settings.setEnabled(true);
        settings.setDecision(decision);
        settings.setDecisionPoint(decisionPoint);
        settings.setBlockedString("blocked"_s);
        [TestProtocol registerWithScheme:@"http"];

        auto webView = adoptNS([[WebView alloc] initWithFrame:NSZeroRect]);
        auto frameLoadDelegate = adoptNS([[LoadAlternateFrameLoadDelegate alloc] init]);
        [webView setFrameLoadDelegate:frameLoadDelegate.get()];
        [[webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"http://redirect/?result"]]];

        isDone = false;
        TestWebKitAPI::Util::run(&isDone);

        settings.setEnabled(false);
        [TestProtocol unregister];
    }
}

TEST(ContentFiltering, LoadAlternateAfterWillSendRequestWK1)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterWillSendRequest);
}

TEST(ContentFiltering, LoadAlternateAfterRedirectWK1)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterRedirect);
}

TEST(ContentFiltering, LoadAlternateAfterResponseWK1)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterResponse);
}

TEST(ContentFiltering, LoadAlternateAfterAddDataWK1)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterAddData);
}

TEST(ContentFiltering, LoadAlternateAfterFinishedAddingDataWK1)
{
    loadAlternateTest(Decision::Block, DecisionPoint::AfterFinishedAddingData);
}

} // namespace TestWebKitAPI
