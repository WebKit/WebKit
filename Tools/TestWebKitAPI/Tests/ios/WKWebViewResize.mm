/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "Test.h"
#import "TestWKWebView.h"
#import <objc/runtime.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/Assertions.h>
#import <wtf/OptionSet.h>

namespace WebKit {
enum class ViewStabilityFlag : uint8_t;
}

namespace {
CGRect _mostRecentUnobscuredRect;
}

@interface NSObject ()

- (void)didUpdateVisibleRect:(CGRect)visibleRect
    unobscuredRect:(CGRect)unobscuredRect
    contentInsets:(UIEdgeInsets)contentInsets
    unobscuredRectInScrollViewCoordinates:(CGRect)unobscuredRectInScrollViewCoordinates
    obscuredInsets:(UIEdgeInsets)obscuredInsets
    unobscuredSafeAreaInsets:(UIEdgeInsets)unobscuredSafeAreaInsets
    inputViewBounds:(CGRect)inputViewBounds
    scale:(CGFloat)scale minimumScale:(CGFloat)minimumScale
    viewStability:(OptionSet<WebKit::ViewStabilityFlag>)viewStability
    enclosedInScrollableAncestorView:(BOOL)enclosedInScrollableAncestorView
    sendEvenIfUnchanged:(BOOL)sendEvenIfUnchanged;

@end

@implementation NSObject (WKContentViewSwizzleForTesting)

- (void)swizzled_didUpdateVisibleRect:(CGRect)visibleRect
    unobscuredRect:(CGRect)unobscuredRect
    contentInsets:(UIEdgeInsets)contentInsets
    unobscuredRectInScrollViewCoordinates:(CGRect)unobscuredRectInScrollViewCoordinates
    obscuredInsets:(UIEdgeInsets)obscuredInsets
    unobscuredSafeAreaInsets:(UIEdgeInsets)unobscuredSafeAreaInsets
    inputViewBounds:(CGRect)inputViewBounds
    scale:(CGFloat)scale minimumScale:(CGFloat)minimumScale
    viewStability:(OptionSet<WebKit::ViewStabilityFlag>)viewStability
    enclosedInScrollableAncestorView:(BOOL)enclosedInScrollableAncestorView
    sendEvenIfUnchanged:(BOOL)sendEvenIfUnchanged {
    _mostRecentUnobscuredRect = unobscuredRect;
    [self swizzled_didUpdateVisibleRect:visibleRect unobscuredRect:unobscuredRect contentInsets:contentInsets unobscuredRectInScrollViewCoordinates:unobscuredRectInScrollViewCoordinates obscuredInsets:obscuredInsets unobscuredSafeAreaInsets:unobscuredSafeAreaInsets inputViewBounds:inputViewBounds scale:scale minimumScale:minimumScale viewStability:viewStability enclosedInScrollableAncestorView:enclosedInScrollableAncestorView sendEvenIfUnchanged:sendEvenIfUnchanged];
}

@end

static void setupWKContentViewSwizzle()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        Class class1 = NSClassFromString(@"WKContentView");

        SEL originalSelector = @selector(didUpdateVisibleRect:unobscuredRect:contentInsets:unobscuredRectInScrollViewCoordinates:obscuredInsets:unobscuredSafeAreaInsets:inputViewBounds:scale:minimumScale:viewStability:enclosedInScrollableAncestorView:sendEvenIfUnchanged:);
        SEL swizzledSelector = @selector(swizzled_didUpdateVisibleRect:unobscuredRect:contentInsets:unobscuredRectInScrollViewCoordinates:obscuredInsets:unobscuredSafeAreaInsets:inputViewBounds:scale:minimumScale:viewStability:enclosedInScrollableAncestorView:sendEvenIfUnchanged:);

        Method originalMethod = class_getInstanceMethod(class1, originalSelector);
        Method swizzledMethod = class_getInstanceMethod(class1, swizzledSelector);

        IMP originalImp = method_getImplementation(originalMethod);
        IMP swizzledImp = method_getImplementation(swizzledMethod);

        class_replaceMethod(class1, swizzledSelector, originalImp, method_getTypeEncoding(originalMethod));
        class_replaceMethod(class1, originalSelector, swizzledImp, method_getTypeEncoding(swizzledMethod));
    });
}


static bool operator==(const CGRect a, const CGRect b)
{
    return CGRectEqualToRect(a, b);
}

static void webViewHasExpectedBounds(TestWKWebView *webView, CGRect expectedBounds)
{
    [CATransaction addCommitHandler:[webView, expectedBounds] {
        EXPECT_EQ([webView bounds], expectedBounds);
        EXPECT_EQ(_mostRecentUnobscuredRect, expectedBounds);

    } forPhase:kCATransactionPhasePreCommit];
}

TEST(WKWebView, EnsureUnobscuredContentRectMatchesWebViewBounds)
{
    setupWKContentViewSwizzle();

    CGRect initialFrame = CGRectMake(0, 0, 800, 600);
    CGRect largerFrame = CGRectMake(0, 0, 1200, 800);
    CGRect smallerFrame = CGRectMake(0, 0, 400, 300);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:initialFrame]);
    [webView setFrame:largerFrame];

    webViewHasExpectedBounds(webView.get(), largerFrame);

    [webView setFrame:smallerFrame];
    webViewHasExpectedBounds(webView.get(), smallerFrame);
}

#endif
