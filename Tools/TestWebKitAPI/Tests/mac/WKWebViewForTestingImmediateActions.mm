/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WKWebViewForTestingImmediateActions.h"

#if PLATFORM(MAC)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKHitTestResult.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

using namespace TestWebKitAPI;

static NSPoint gSwizzledImmediateActionLocation = NSZeroPoint;
static NSPoint swizzledImmediateActionLocationInView(id, SEL, NSView *)
{
    return gSwizzledImmediateActionLocation;
}

@implementation WKWebViewForTestingImmediateActions {
    bool _hasReturnedImmediateActionController;
    RetainPtr<_WKHitTestResult> _hitTestResult;
    _WKImmediateActionType _actionType;
}

- (id)_immediateActionAnimationControllerForHitTestResult:(_WKHitTestResult *)hitTestResult withType:(_WKImmediateActionType)type userData:(id<NSSecureCoding>)userData
{
    _hasReturnedImmediateActionController = true;
    _hitTestResult = hitTestResult;
    _actionType = type;
    return [super _immediateActionAnimationControllerForHitTestResult:hitTestResult withType:type userData:userData];
}

- (NSImmediateActionGestureRecognizer *)immediateActionGesture
{
    for (NSGestureRecognizer *gesture in [self gestureRecognizers]) {
        if (RetainPtr immediateActionGesture = dynamic_objc_cast<NSImmediateActionGestureRecognizer>(gesture))
            return immediateActionGesture.get();
    }
    return nil;
}

- (ImmediateActionHitTestResult)simulateImmediateAction:(NSPoint)location
{
    auto immediateActionGesture = self.immediateActionGesture;
    if (!immediateActionGesture.delegate)
        return { nil, _WKImmediateActionNone };

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

#endif
