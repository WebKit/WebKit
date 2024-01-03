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
#import "WKBaseScrollView.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

@interface UIScrollView (GestureRecognizerDelegate) <UIGestureRecognizerDelegate>
@end

@implementation WKBaseScrollView {
    RetainPtr<UIPanGestureRecognizer> _axisLockingPanGestureRecognizer;
    UIAxis _axesToPreventMomentumScrolling;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    [WKBaseScrollView _overrideAddGestureRecognizerIfNeeded];

    if (!(self = [super initWithFrame:frame]))
        return nil;

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING) && !HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_SUBCLASS_HOOKS)
    self._allowsAsyncScrollEvent = YES;
#endif

    _axesToPreventMomentumScrolling = UIAxisNeither;
    [self.panGestureRecognizer addTarget:self action:@selector(_updatePanGestureToPreventScrolling)];
    return self;
}

+ (void)_overrideAddGestureRecognizerIfNeeded
{
    static bool hasOverridenAddGestureRecognizer = false;
    if (std::exchange(hasOverridenAddGestureRecognizer, true))
        return;

    if (WebCore::IOSApplication::isHimalaya() && !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ScrollViewSubclassImplementsAddGestureRecognizer)) {
        // This check can be removed and -_wk_addGestureRecognizer: can be renamed to -addGestureRecognizer: once the 喜马拉雅 app updates to a version of
        // the iOS 17 SDK with this WKBaseScrollView refactoring. Otherwise, the call to `-[super addGestureRecognizer:]` below will fail, due to how this
        // app uses `class_getInstanceMethod` and `method_setImplementation` to intercept and override all calls to `-[UIView addGestureRecognizer:]`.
        return;
    }

    auto method = class_getInstanceMethod(self.class, @selector(_wk_addGestureRecognizer:));
    class_addMethod(self.class, @selector(addGestureRecognizer:), method_getImplementation(method), method_getTypeEncoding(method));
}

- (void)_wk_addGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer) {
        if (!_axisLockingPanGestureRecognizer) {
            _axisLockingPanGestureRecognizer = adoptNS([[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(_updatePanGestureToPreventScrolling)]);
            [_axisLockingPanGestureRecognizer setName:@"Scroll axis locking"];
            [_axisLockingPanGestureRecognizer setDelegate:self];
        }
        [self addGestureRecognizer:_axisLockingPanGestureRecognizer.get()];
    }

    [super addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer) {
        if (auto gesture = std::exchange(_axisLockingPanGestureRecognizer, nil))
            [self removeGestureRecognizer:gesture.get()];
    }

    [super removeGestureRecognizer:gestureRecognizer];
}

- (void)_updatePanGestureToPreventScrolling
{
    auto panGesture = self.panGestureRecognizer;
    switch (self.panGestureRecognizer.state) {
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateEnded:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
        return;
    case UIGestureRecognizerStateBegan:
    case UIGestureRecognizerStateChanged:
        break;
    }

    auto axesToPrevent = self._axesToPreventScrollingFromDelegate;
    if (axesToPrevent == UIAxisNeither)
        return;

    auto adjustedTranslation = [panGesture translationInView:nil];
    bool translationChanged = false;
    if ((axesToPrevent & UIAxisHorizontal) && std::abs(adjustedTranslation.x) > CGFLOAT_EPSILON) {
        adjustedTranslation.x = 0;
        _axesToPreventMomentumScrolling |= UIAxisHorizontal;
        translationChanged = true;
    }

    if ((axesToPrevent & UIAxisVertical) && std::abs(adjustedTranslation.y) > CGFLOAT_EPSILON) {
        adjustedTranslation.y = 0;
        _axesToPreventMomentumScrolling |= UIAxisVertical;
        translationChanged = true;
    }

    if (translationChanged)
        [panGesture setTranslation:adjustedTranslation inView:nil];
}

- (UIAxis)_axesToPreventScrollingFromDelegate
{
    auto delegate = self.baseScrollViewDelegate;
    return delegate ? [delegate axesToPreventScrollingForPanGestureInScrollView:self] : UIAxisNeither;
}

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_SUBCLASS_HOOKS)

- (BOOL)_subclassHandlesAsyncScrollEvent
{
    return YES;
}

- (void)_asynchronouslyHandleScrollEvent:(UIScrollEvent *)event completion:(void (^)(BOOL handled))completion
{
    auto delegate = retainPtr(self.baseScrollViewDelegate);
    if (!delegate)
        return completion(NO);

    [delegate scrollView:self handleScrollEvent:event completion:completion];
}

#endif // HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_SUBCLASS_HOOKS)

#if HAVE(UI_SCROLL_VIEW_ACTING_PARENT_FOR_SCROLL_VIEW)

- (UIScrollView *)_actingParentScrollView
{
    if (![self.baseScrollViewDelegate respondsToSelector:@selector(actingParentScrollViewForScrollView:)])
        return nil;

    return [self.baseScrollViewDelegate actingParentScrollViewForScrollView:self];
}

#endif // HAVE(UI_SCROLL_VIEW_ACTING_PARENT_FOR_SCROLL_VIEW)

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if (gestureRecognizer == _axisLockingPanGestureRecognizer || otherGestureRecognizer == _axisLockingPanGestureRecognizer)
        return YES;

    static BOOL callIntoSuperclass = [UIScrollView instancesRespondToSelector:@selector(gestureRecognizer:shouldRecognizeSimultaneouslyWithGestureRecognizer:)];
    if (!callIntoSuperclass)
        return NO;

    return [super gestureRecognizer:gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:otherGestureRecognizer];
}

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer *)gestureRecognizer
{
    if (self.panGestureRecognizer == gestureRecognizer)
        _axesToPreventMomentumScrolling = UIAxisNeither;

    static BOOL callIntoSuperclass = [UIScrollView instancesRespondToSelector:@selector(gestureRecognizerShouldBegin:)];
    if (!callIntoSuperclass)
        return YES;

    return [super gestureRecognizerShouldBegin:gestureRecognizer];
}

@end

#endif // PLATFORM(IOS_FAMILY)
