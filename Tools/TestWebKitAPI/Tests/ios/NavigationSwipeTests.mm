/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PoseAsClass.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

@interface TestNavigationInteractiveTransition : UIPercentDrivenInteractiveTransition
@end

@implementation TestNavigationInteractiveTransition

- (void)startInteractiveTransition:(id <UIViewControllerContextTransitioning>)transitionContext
{
    [super startInteractiveTransition:transitionContext];
    EXPECT_TRUE([transitionContext.containerView.window.firstResponder resignFirstResponder]);
}

@end

TEST(NavigationSwipeTests, RestoreFirstResponderAfterNavigationSwipe)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    [webView _beginBackSwipeForTesting];
    [webView _completeBackSwipeForTesting];
    EXPECT_TRUE([webView _contentViewIsFirstResponder]);
}

TEST(NavigationSwipeTests, DoNotBecomeFirstResponderAfterNavigationSwipeIfWebViewIsUnparented)
{
    poseAsClass("TestNavigationInteractiveTransition", "_UINavigationInteractiveTransitionBase");

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView setAllowsBackForwardNavigationGestures:YES];
    [webView becomeFirstResponder];

    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView synchronouslyLoadTestPageNamed:@"simple2"];

    [webView _beginBackSwipeForTesting];
    [webView removeFromSuperview];
    [webView _completeBackSwipeForTesting];
    EXPECT_FALSE([webView _contentViewIsFirstResponder]);
}

#endif // PLATFORM(IOS_FAMILY)
