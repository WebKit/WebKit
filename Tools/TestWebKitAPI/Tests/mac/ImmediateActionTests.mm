/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKHitTestResult.h>
#import <pal/spi/cocoa/RevealSPI.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <wtf/RetainPtr.h>

static NSPoint gSwizzledImmediateActionLocation = NSZeroPoint;
static NSPoint swizzledImmediateActionLocationInView(id, SEL, NSView *)
{
    return gSwizzledImmediateActionLocation;
}

using ImmediateActionHitTestResult = std::pair<RetainPtr<_WKHitTestResult>, _WKImmediateActionType>;

@interface WKWebViewForTestingImmediateActions : TestWKWebView

@property (nonatomic, readonly) NSImmediateActionGestureRecognizer *immediateActionGesture;

- (ImmediateActionHitTestResult)simulateImmediateAction:(NSPoint)location;

@end

@implementation WKWebViewForTestingImmediateActions {
    bool _hasReturnedImmediateActionController;
    RetainPtr<_WKHitTestResult> _hitTestResult;
    _WKImmediateActionType _actionType;
}

- (id)_immediateActionAnimationControllerForHitTestResult:(_WKHitTestResult *)hitTestResult withType:(_WKImmediateActionType)type userData:(id <NSSecureCoding>)userData
{
    _hasReturnedImmediateActionController = true;
    _hitTestResult = hitTestResult;
    _actionType = type;
    return [super _immediateActionAnimationControllerForHitTestResult:hitTestResult withType:type userData:userData];
}

- (NSImmediateActionGestureRecognizer *)immediateActionGesture
{
    for (NSGestureRecognizer *gesture in [self gestureRecognizers]) {
        if ([gesture isKindOfClass:NSImmediateActionGestureRecognizer.class])
            return static_cast<NSImmediateActionGestureRecognizer *>(gesture);
    }
    return nil;
}

- (ImmediateActionHitTestResult)simulateImmediateAction:(NSPoint)location
{
    auto immediateActionGesture = self.immediateActionGesture;
    if (!immediateActionGesture.delegate)
        return ImmediateActionHitTestResult { nil, _WKImmediateActionNone };

    _hasReturnedImmediateActionController = false;

    InstanceMethodSwizzler swizzleLocationInView {
        NSImmediateActionGestureRecognizer.class,
        @selector(locationInView:),
        reinterpret_cast<IMP>(swizzledImmediateActionLocationInView),
    };

    gSwizzledImmediateActionLocation = location;
    [immediateActionGesture.delegate immediateActionRecognizerWillPrepare:immediateActionGesture];

    TestWebKitAPI::Util::run(&_hasReturnedImmediateActionController);

    _hasReturnedImmediateActionController = false;
    return { std::exchange(_hitTestResult, nil), std::exchange(_actionType, _WKImmediateActionNone) };
}

@end

@interface RVPresentingContext (Internal)
@property (nonatomic, readonly, weak) id<RVPresenterHighlightDelegate> highlightDelegate;
@end

static RetainPtr<RVPresentingContext> lastPresentingContext;

@implementation RVPresentingContext (ImmediateActionTests)

- (instancetype)swizzled_initWithPointerLocationInView:(NSPoint)location inView:(NSView *)view highlightDelegate:(id<RVPresenterHighlightDelegate>)delegate
{
    lastPresentingContext = [self swizzled_initWithPointerLocationInView:location inView:view highlightDelegate:delegate];
    return lastPresentingContext.get();
}

@end

namespace TestWebKitAPI {

static void swizzlePresentingContextInitialization()
{
    auto originalMethod = class_getInstanceMethod(RVPresentingContext.class, @selector(initWithPointerLocationInView:inView:highlightDelegate:));
    auto swizzledMethod = class_getInstanceMethod(RVPresentingContext.class, @selector(swizzled_initWithPointerLocationInView:inView:highlightDelegate:));
    auto originalImplementation = method_getImplementation(originalMethod);
    auto swizzledImplementation = method_getImplementation(swizzledMethod);
    class_replaceMethod(RVPresentingContext.class, @selector(swizzled_initWithPointerLocationInView:inView:highlightDelegate:), originalImplementation, method_getTypeEncoding(originalMethod));
    class_replaceMethod(RVPresentingContext.class, @selector(initWithPointerLocationInView:inView:highlightDelegate:), swizzledImplementation, method_getTypeEncoding(swizzledMethod));
}

TEST(ImmediateActionTests, ImmediateActionOverText)
{
    swizzlePresentingContextInitialization();

    auto webView = adoptNS([[WKWebViewForTestingImmediateActions alloc] initWithFrame:NSMakeRect(0, 0, 500, 500)]);
    [webView synchronouslyLoadHTMLString:@"<div style='font-size: 32px;'>Foobar</div>"];

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_NOT_NULL([webView immediateActionGesture].animationController);
    EXPECT_EQ(actionType, _WKImmediateActionLookupText);
    EXPECT_WK_STREQ([hitTestResult lookupText], "Foobar");
    EXPECT_NOT_NULL([lastPresentingContext highlightDelegate]);
}

TEST(ImmediateActionTests, ImmediateActionOverBody)
{
    auto webView = adoptNS([[WKWebViewForTestingImmediateActions alloc] initWithFrame:NSMakeRect(0, 0, 500, 500)]);
    [webView synchronouslyLoadHTMLString:@"<div style='font-size: 32px;'>Foobar</div>"];

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(490, 490)];
    EXPECT_NULL([webView immediateActionGesture].animationController);
    EXPECT_EQ(actionType, _WKImmediateActionNone);
    EXPECT_EQ([hitTestResult lookupText].length, 0U);
}

#if ENABLE(IMAGE_ANALYSIS)

TEST(ImmediateActionTests, ImmediateActionOverImageOverlay)
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    auto webView = adoptNS([[WKWebViewForTestingImmediateActions alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"simple-image-overlay"];

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(50, 50)];
    EXPECT_NOT_NULL([webView immediateActionGesture].animationController);
    EXPECT_EQ(actionType, _WKImmediateActionLookupText);
    EXPECT_WK_STREQ([hitTestResult lookupText], "foobar");
}

#endif // ENABLE(IMAGE_ANALYSIS)

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
