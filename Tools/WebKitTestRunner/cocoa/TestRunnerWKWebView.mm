/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "TestRunnerWKWebView.h"

#import "WebKitTestRunnerDraggingInfo.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>
@interface WKWebView ()

// FIXME: move these to WKWebView_Private.h
- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view;
- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale;
- (void)_didFinishScrolling;
- (void)_scheduleVisibleContentRectUpdate;

@end
#endif

#if WK_API_ENABLED

@interface TestRunnerWKWebView () <WKUIDelegatePrivate> {
    RetainPtr<NSNumber *> m_stableStateOverride;
    BOOL m_isInteractingWithFormControl;
}

@property (nonatomic, copy) void (^zoomToScaleCompletionHandler)(void);
@property (nonatomic, copy) void (^retrieveSpeakSelectionContentCompletionHandler)(void);
@property (nonatomic, getter=isShowingKeyboard, setter=setIsShowingKeyboard:) BOOL showingKeyboard;

@end

@implementation TestRunnerWKWebView

#if PLATFORM(MAC)
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag
IGNORE_WARNINGS_END
{
    RetainPtr<WebKitTestRunnerDraggingInfo> draggingInfo = adoptNS([[WebKitTestRunnerDraggingInfo alloc] initWithImage:anImage offset:initialOffset pasteboard:pboard source:sourceObj]);
    [self draggingUpdated:draggingInfo.get()];
}
#endif

#if PLATFORM(IOS_FAMILY)
- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (self = [super initWithFrame:frame configuration:configuration]) {
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
        [center addObserver:self selector:@selector(_invokeShowKeyboardCallbackIfNecessary) name:UIKeyboardDidShowNotification object:nil];
        [center addObserver:self selector:@selector(_invokeHideKeyboardCallbackIfNecessary) name:UIKeyboardDidHideNotification object:nil];

        self.UIDelegate = self;
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    self.didStartFormControlInteractionCallback = nil;
    self.didEndFormControlInteractionCallback = nil;
    self.didShowForcePressPreviewCallback = nil;
    self.didDismissForcePressPreviewCallback = nil;
    self.willBeginZoomingCallback = nil;
    self.didEndZoomingCallback = nil;
    self.didShowKeyboardCallback = nil;
    self.didHideKeyboardCallback = nil;
    self.didEndScrollingCallback = nil;
    self.rotationDidEndCallback = nil;

    self.zoomToScaleCompletionHandler = nil;
    self.retrieveSpeakSelectionContentCompletionHandler = nil;

    [super dealloc];
}

- (void)didStartFormControlInteraction
{
    m_isInteractingWithFormControl = YES;

    if (self.didStartFormControlInteractionCallback)
        self.didStartFormControlInteractionCallback();
}

- (void)didEndFormControlInteraction
{
    m_isInteractingWithFormControl = NO;

    if (self.didEndFormControlInteractionCallback)
        self.didEndFormControlInteractionCallback();
}

- (BOOL)isInteractingWithFormControl
{
    return m_isInteractingWithFormControl;
}

- (void)_didShowForcePressPreview
{
    if (self.didShowForcePressPreviewCallback)
        self.didShowForcePressPreviewCallback();
}

- (void)_didDismissForcePressPreview
{
    if (self.didDismissForcePressPreviewCallback)
        self.didDismissForcePressPreviewCallback();
}

- (void)zoomToScale:(double)scale animated:(BOOL)animated completionHandler:(void (^)(void))completionHandler
{
    ASSERT(!self.zoomToScaleCompletionHandler);

    if (self.scrollView.zoomScale == scale) {
        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler();
        });
        return;
    }

    self.zoomToScaleCompletionHandler = completionHandler;
    [self.scrollView setZoomScale:scale animated:animated];
}

- (void)_invokeShowKeyboardCallbackIfNecessary
{
    if (self.showingKeyboard)
        return;

    self.showingKeyboard = YES;
    if (self.didShowKeyboardCallback)
        self.didShowKeyboardCallback();
}

- (void)_invokeHideKeyboardCallbackIfNecessary
{
    if (!self.showingKeyboard)
        return;

    self.showingKeyboard = NO;
    if (self.didHideKeyboardCallback)
        self.didHideKeyboardCallback();
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view
{
    [super scrollViewWillBeginZooming:scrollView withView:view];

    if (self.willBeginZoomingCallback)
        self.willBeginZoomingCallback();
}

- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale
{
    [super scrollViewDidEndZooming:scrollView withView:view atScale:scale];
    
    if (self.didEndZoomingCallback)
        self.didEndZoomingCallback();

    if (self.zoomToScaleCompletionHandler) {
        self.zoomToScaleCompletionHandler();
        self.zoomToScaleCompletionHandler = nullptr;
    }
}

- (void)_didFinishScrolling
{
    [super _didFinishScrolling];

    if (self.didEndScrollingCallback)
        self.didEndScrollingCallback();
}

- (NSNumber *)_stableStateOverride
{
    return m_stableStateOverride.get();
}

- (void)_setStableStateOverride:(NSNumber *)overrideBoolean
{
    m_stableStateOverride = overrideBoolean;
    [self _scheduleVisibleContentRectUpdate];
}

- (void)_didEndRotation
{
    if (self.rotationDidEndCallback)
        self.rotationDidEndCallback();
}

- (void)_accessibilityDidGetSpeakSelectionContent:(NSString *)content
{
    self.accessibilitySpeakSelectionContent = content;
    if (self.retrieveSpeakSelectionContentCompletionHandler)
        self.retrieveSpeakSelectionContentCompletionHandler();
}

- (void)accessibilityRetrieveSpeakSelectionContentWithCompletionHandler:(void (^)(void))completionHandler
{
    self.retrieveSpeakSelectionContentCompletionHandler = completionHandler;
    [self _accessibilityRetrieveSpeakSelectionContent];
}

- (void)setOverrideSafeAreaInsets:(UIEdgeInsets)insets
{
    _overrideSafeAreaInsets = insets;
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    [self _updateSafeAreaInsets];
#endif
}

- (UIEdgeInsets)_safeAreaInsetsForFrame:(CGRect)frame inSuperview:(UIView *)view
{
    return _overrideSafeAreaInsets;
}

#pragma mark - WKUIDelegatePrivate

// In extra zoom mode, fullscreen form control UI takes on the same role as keyboards and input view controllers
// in UIKit. As such, we allow keyboard presentation and dismissal callbacks to work in extra zoom mode as well.
- (void)_webView:(WKWebView *)webView didPresentFocusedElementViewController:(UIViewController *)controller
{
    [self _invokeShowKeyboardCallbackIfNecessary];
}

- (void)_webView:(WKWebView *)webView didDismissFocusedElementViewController:(UIViewController *)controller
{
    [self _invokeHideKeyboardCallbackIfNecessary];
}

#endif // PLATFORM(IOS_FAMILY)

@end

#endif // WK_API_ENABLED
