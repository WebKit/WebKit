/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && USE(RUNNINGBOARD)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/RetainPtr.h>

static bool waitForAssertionType(TestWKWebView *webView, NSString *expectedType)
{
    return TestWebKitAPI::Util::waitFor([&] {
        return [expectedType isEqualToString:[webView _processAssertionTypeForTesting]];
    });
}

TEST(ProcessJetsamPriority, DisablingJetsamBoostMovesBackgroundAssertionToIdleBand)
{
    // WebProcessActivityState::dropVisibleActivity() only takes a background activity when there
    // are more than four cores. Below that it takes a foreground activity, so hiding the view does
    // not move the process out of the foreground.
    if (NSProcessInfo.processInfo.processorCount <= 4)
        return;

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<body>Hello world!</body>"];

    EXPECT_TRUE(waitForAssertionType(webView.get(), @"foreground"));

    [webView removeFromSuperview];
    EXPECT_TRUE(waitForAssertionType(webView.get(), @"background"));

    // A process without the jetsam boost runs its background activities at the idle jetsam band.
    [webView _setJetsamBoostEnabledForTesting:NO];
    EXPECT_WK_STREQ("background-idle-jetsam", [webView _processAssertionTypeForTesting]);

    [webView _setJetsamBoostEnabledForTesting:YES];
    EXPECT_WK_STREQ("background", [webView _processAssertionTypeForTesting]);
}

TEST(ProcessJetsamPriority, DisablingJetsamBoostDoesNotAffectForegroundAssertion)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    [webView synchronouslyLoadHTMLString:@"<body>Hello world!</body>"];

    EXPECT_TRUE(waitForAssertionType(webView.get(), @"foreground"));

    [webView _setJetsamBoostEnabledForTesting:NO];
    EXPECT_WK_STREQ("foreground", [webView _processAssertionTypeForTesting]);
}

#endif // PLATFORM(MAC) && USE(RUNNINGBOARD)
