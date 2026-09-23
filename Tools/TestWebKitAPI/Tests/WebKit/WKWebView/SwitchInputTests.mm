/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import "IOSMouseEventTestHarness.h"
#import "InstanceMethodSwizzler.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/_WKTouchEventGenerator.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(MAC)

TEST(SwitchInputTests, HapticFeedbackOnDrag)
{
    __block bool done = false;

    InstanceMethodSwizzler swizzler {
        NSHapticFeedbackManager.defaultPerformer.class,
        @selector(performFeedbackPattern:performanceTime:),
        imp_implementationWithBlock(^{
            done = true;
        })
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300)]);
    [webView synchronouslyLoadHTMLString:@"<input type='checkbox' switch>"];

    [webView mouseMoveToPoint:NSMakePoint(15, 290) withFlags:0];
    [webView mouseDownAtPoint:NSMakePoint(15, 290) simulatePressure:NO];
    [webView waitForPendingMouseEvents];

    [webView mouseDragToPoint:NSMakePoint(30, 290)];
    [webView waitForPendingMouseEvents];

    TestWebKitAPI::Util::run(&done);
}

#endif // PLATFORM(MAC)

#if HAVE(UI_IMPACT_FEEDBACK_GENERATOR)

TEST(SwitchInputTests, HapticFeedbackOnClick)
{
    __block bool done = false;

    InstanceMethodSwizzler swizzler {
        UIImpactFeedbackGenerator.class,
        @selector(impactOccurred),
        imp_implementationWithBlock(^{
            done = true;
        })
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><label><input type='checkbox' switch></label>"];

    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(15, 10);
    testHarness.mouseDown();
    testHarness.mouseUp();
    [webView waitForPendingMouseEvents];
    [webView waitForNextPresentationUpdate];

    TestWebKitAPI::Util::run(&done);
}

#endif // HAVE(UI_IMPACT_FEEDBACK_GENERATOR)

#if HAVE(UI_IMPACT_FEEDBACK_GENERATOR) || PLATFORM(MAC)

TEST(SwitchInputTests, HapticFeedbackRequiresUserGestureAndTrustedEvent)
{
    InstanceMethodSwizzler swizzler {
#if PLATFORM(MAC)
        NSHapticFeedbackManager.defaultPerformer.class,
        @selector(performFeedbackPattern:performanceTime:),
#else
        UIImpactFeedbackGenerator.class,
        @selector(impactOccurred),
#endif
        imp_implementationWithBlock(^{
            FAIL();
        })
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 300)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width,initial-scale=1'><div style='width: 100px; height: 100px; background: red;' onclick=\"document.querySelector('label').click()\"></div><label><input type='checkbox' switch></label>"];

#if PLATFORM(MAC)
    [webView sendClickAtPoint:NSMakePoint(15, 290)];
#else
    TestWebKitAPI::MouseEventTestHarness testHarness { webView.get() };
    testHarness.mouseMove(15, 10);
    testHarness.mouseDown();
    testHarness.mouseUp();
#endif

    [webView waitForPendingMouseEvents];
    [webView waitForNextPresentationUpdate];
}

#endif // HAVE(UI_IMPACT_FEEDBACK_GENERATOR) || PLATFORM(MAC)

#endif // PLATFORM(COCOA)
