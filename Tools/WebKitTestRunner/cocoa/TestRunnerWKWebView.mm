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

#import "TestController.h"
#import "WebKitTestRunnerDraggingInfo.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/Assertions.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <WebKit/WKWebViewPrivate.h>

@interface WKWebView ()

// FIXME: move these to WKWebView_Private.h
- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view;
- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(CGFloat)scale;
- (void)_didFinishScrolling:(UIScrollView *)scrollView;
- (void)_scheduleVisibleContentRectUpdate;

@end
#endif

#if HAVE(PEPPER_UI_CORE)
#import "PepperUICoreSPI.h"
#endif

struct CustomMenuActionInfo {
    RetainPtr<NSString> name;
    BOOL dismissesAutomatically { NO };
    BlockPtr<void()> callback;
};

@interface TestRunnerWKWebView () <WKUIDelegatePrivate, _WKInputDelegate
#if PLATFORM(IOS_FAMILY)
    , UIGestureRecognizerDelegate
#endif
> {
    RetainPtr<NSNumber> m_stableStateOverride;
    BOOL _isInteractingWithFormControl;
    BOOL _scrollingUpdatesDisabled;
    RetainPtr<NSArray<NSString *>> _allowedMenuActions;
#if PLATFORM(IOS_FAMILY)
    RetainPtr<UITapGestureRecognizer> _windowTapGestureRecognizer;
    BlockPtr<void()> _windowTapRecognizedCallback;
    UIInterfaceOrientationMask _supportedInterfaceOrientations;
#endif
}

@property (nonatomic, copy) void (^zoomToScaleCompletionHandler)(void);
@property (nonatomic, copy) void (^retrieveSpeakSelectionContentCompletionHandler)(void);
@property (nonatomic, getter=isShowingKeyboard, setter=setIsShowingKeyboard:) BOOL showingKeyboard;
@property (nonatomic, getter=isShowingMenu, setter=setIsShowingMenu:) BOOL showingMenu;
@property (nonatomic, getter=isDismissingMenu, setter=setIsDismissingMenu:) BOOL dismissingMenu;
@property (nonatomic, getter=isShowingPopover, setter=setIsShowingPopover:) BOOL showingPopover;
@property (nonatomic, getter=isShowingContextMenu, setter=setIsShowingContextMenu:) BOOL showingContextMenu;
@property (nonatomic, getter=isShowingContactPicker, setter=setIsShowingContactPicker:) BOOL showingContactPicker;

@end

@implementation TestRunnerWKWebView

@dynamic _stableStateOverride;

#if PLATFORM(MAC)
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
- (void)dragImage:(NSImage *)anImage at:(NSPoint)viewLocation offset:(NSSize)initialOffset event:(NSEvent *)event pasteboard:(NSPasteboard *)pboard source:(id)sourceObj slideBack:(BOOL)slideFlag
IGNORE_WARNINGS_END
{
    auto draggingInfo = adoptNS([[WebKitTestRunnerDraggingInfo alloc] initWithImage:anImage offset:initialOffset pasteboard:pboard source:sourceObj]);
    [self draggingUpdated:draggingInfo.get()];
}
#endif

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (self = [super initWithFrame:frame configuration:configuration]) {
        NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
#if PLATFORM(MAC)
        [center addObserver:self selector:@selector(_didShowMenu) name:NSMenuDidBeginTrackingNotification object:nil];
        [center addObserver:self selector:@selector(_didHideMenu) name:NSMenuDidEndTrackingNotification object:nil];
#else
        [center addObserver:self selector:@selector(_invokeShowKeyboardCallbackIfNecessary) name:UIKeyboardDidShowNotification object:nil];
        [center addObserver:self selector:@selector(_invokeHideKeyboardCallbackIfNecessary) name:UIKeyboardDidHideNotification object:nil];
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        [center addObserver:self selector:@selector(_didShowMenu) name:UIMenuControllerDidShowMenuNotification object:nil];
        [center addObserver:self selector:@selector(_willHideMenu) name:UIMenuControllerWillHideMenuNotification object:nil];
        [center addObserver:self selector:@selector(_didHideMenu) name:UIMenuControllerDidHideMenuNotification object:nil];
        ALLOW_DEPRECATED_DECLARATIONS_END
        [center addObserver:self selector:@selector(_willPresentPopover) name:@"UIPopoverControllerWillPresentPopoverNotification" object:nil];
        [center addObserver:self selector:@selector(_didDismissPopover) name:@"UIPopoverControllerDidDismissPopoverNotification" object:nil];
        self.inspectable = YES;
        self.UIDelegate = self;
        self._inputDelegate = self;
        self.focusStartsInputSessionPolicy = _WKFocusStartsInputSessionPolicyAuto;
        self.supportedInterfaceOrientations = UIInterfaceOrientationMaskAll;
#endif
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    [self resetInteractionCallbacks];
#if PLATFORM(IOS_FAMILY)
    self.accessibilitySpeakSelectionContent = nil;
#endif

    self.zoomToScaleCompletionHandler = nil;
    self.retrieveSpeakSelectionContentCompletionHandler = nil;

    [super dealloc];
}

- (void)_didShowContextMenu
{
    if (self.showingContextMenu)
        return;

    self.showingContextMenu = YES;
    if (self.didShowContextMenuCallback)
        self.didShowContextMenuCallback();
}

- (void)_didDismissContextMenu
{
    if (!self.showingContextMenu)
        return;

    self.showingContextMenu = NO;
    if (self.didDismissContextMenuCallback)
        self.didDismissContextMenuCallback();
}

- (void)_didShowMenu
{
    if (self.showingMenu)
        return;

    self.showingMenu = YES;
    if (self.didShowMenuCallback)
        self.didShowMenuCallback();
}

- (void)_didHideMenu
{
#if PLATFORM(IOS_FAMILY)
    self.dismissingMenu = NO;
#endif

    if (!self.showingMenu)
        return;

    self.showingMenu = NO;
    if (self.didHideMenuCallback)
        self.didHideMenuCallback();
}

- (void)dismissActiveMenu
{
#if PLATFORM(IOS_FAMILY)
    [self _dismissAllContextMenuInteractions];
    [self resignFirstResponder];
#else
    auto menu = retainPtr(self._activeMenu);
    [menu removeAllItems];
    [menu update];
    [menu cancelTracking];
#endif
}

- (void)resetInteractionCallbacks
{
    self.didShowContextMenuCallback = nil;
    self.didDismissContextMenuCallback = nil;
    self.didShowMenuCallback = nil;
    self.didHideMenuCallback = nil;
    self.didShowContactPickerCallback = nil;
    self.didHideContactPickerCallback = nil;
#if PLATFORM(IOS_FAMILY)
    self.didStartFormControlInteractionCallback = nil;
    self.didEndFormControlInteractionCallback = nil;
    self.willBeginZoomingCallback = nil;
    self.didEndZoomingCallback = nil;
    self.didShowKeyboardCallback = nil;
    self.didHideKeyboardCallback = nil;
    self.willStartInputSessionCallback = nil;
    self.willPresentPopoverCallback = nil;
    self.didDismissPopoverCallback = nil;
    self.didEndScrollingCallback = nil;
    self.rotationDidEndCallback = nil;
    self.windowTapRecognizedCallback = nil;
#endif // PLATFORM(IOS_FAMILY)
}

#if PLATFORM(IOS_FAMILY)

- (UITextEffectsWindow *)textEffectsWindow
{
    return [UITextEffectsWindow sharedTextEffectsWindowForWindowScene:self.window.windowScene];
}

- (void)_willHideMenu
{
    self.dismissingMenu = YES;
}

- (void)didStartFormControlInteraction
{
    _isInteractingWithFormControl = YES;

    if (self.didStartFormControlInteractionCallback)
        self.didStartFormControlInteractionCallback();
}

- (void)didEndFormControlInteraction
{
    _isInteractingWithFormControl = NO;

    if (self.didEndFormControlInteractionCallback)
        self.didEndFormControlInteractionCallback();
}

- (BOOL)isInteractingWithFormControl
{
    return _isInteractingWithFormControl;
}

- (void)immediatelyDismissContextMenuIfNeeded
{
    if (!self.showingContextMenu)
        return;

    self.showingContextMenu = NO;

    [self _dismissAllContextMenuInteractions];
}

- (void)_dismissAllContextMenuInteractions
{
#if USE(UICONTEXTMENU)
    for (id <UIInteraction> interaction in self.contentView.interactions) {
        if (auto contextMenuInteraction = dynamic_objc_cast<UIContextMenuInteraction>(interaction)) {
            [UIView performWithoutAnimation:^{
                [contextMenuInteraction dismissMenu];
            }];
        }
    }
#endif
}

- (void)setAllowedMenuActions:(NSArray<NSString *> *)actions
{
    _allowedMenuActions = actions;
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

- (void)_willPresentPopover
{
    if (self.showingPopover)
        return;

    self.showingPopover = YES;
    if (self.willPresentPopoverCallback)
        self.willPresentPopoverCallback();
}

- (void)_didDismissPopover
{
    if (!self.showingPopover)
        return;

    self.showingPopover = NO;
    if (self.didDismissPopoverCallback)
        self.didDismissPopoverCallback();
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

- (void)_didFinishScrolling:(UIScrollView *)scrollView
{
    [super _didFinishScrolling:scrollView];

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

- (BOOL)_scrollingUpdatesDisabledForTesting
{
    return _scrollingUpdatesDisabled;
}

- (void)_setScrollingUpdatesDisabledForTesting:(BOOL)disabled
{
    _scrollingUpdatesDisabled = disabled;
}

- (void)_didEndRotation
{
    if (self.rotationDidEndCallback)
        self.rotationDidEndCallback();
}

- (void)didRecognizeTapOnWindow
{
    ASSERT(self.windowTapRecognizedCallback);
    if (self.windowTapRecognizedCallback)
        self.windowTapRecognizedCallback();
}

- (void(^)())windowTapRecognizedCallback
{
    return _windowTapRecognizedCallback.get();
}

- (void)setWindowTapRecognizedCallback:(void(^)())windowTapRecognizedCallback
{
    _windowTapRecognizedCallback = windowTapRecognizedCallback;

    if (windowTapRecognizedCallback && !_windowTapGestureRecognizer) {
        ASSERT(self.window);
        _windowTapGestureRecognizer = adoptNS([[UITapGestureRecognizer alloc] init]);
        [_windowTapGestureRecognizer setDelegate:self];
        [_windowTapGestureRecognizer addTarget:self action:@selector(didRecognizeTapOnWindow)];
        [self.window addGestureRecognizer:_windowTapGestureRecognizer.get()];
    } else if (!windowTapRecognizedCallback && _windowTapGestureRecognizer) {
        [self.window removeGestureRecognizer:_windowTapGestureRecognizer.get()];
        _windowTapGestureRecognizer = nil;
    }
}

- (void)willMoveToWindow:(UIWindow *)window
{
    [super willMoveToWindow:window];

    if (_windowTapGestureRecognizer)
        [self.window removeGestureRecognizer:_windowTapGestureRecognizer.get()];
}

- (void)didMoveToWindow
{
    [super didMoveToWindow];

    if (_windowTapGestureRecognizer)
        [self.window addGestureRecognizer:_windowTapGestureRecognizer.get()];
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

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    [self _updateSafeAreaInsets];
#endif
}

- (UIEdgeInsets)_safeAreaInsetsForFrame:(CGRect)frame inSuperview:(UIView *)view
{
    return _overrideSafeAreaInsets;
}

- (void)setSupportedInterfaceOrientations:(UIInterfaceOrientationMask)orientations
{
    _supportedInterfaceOrientations = orientations;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return _supportedInterfaceOrientations;
}

- (UIView *)contentView
{
    return [self valueForKeyPath:@"_currentContentView"];
}

static bool isQuickboardViewController(UIViewController *viewController)
{
#if HAVE(PEPPER_UI_CORE)
    if ([viewController isKindOfClass:PUICQuickboardViewController.class])
        return true;
#if HAVE(QUICKBOARD_CONTROLLER)
    if ([viewController isKindOfClass:PUICQuickboardRemoteViewController.class])
        return true;
#endif // HAVE(QUICKBOARD_CONTROLLER)
#endif // HAVE(PEPPER_UI_CORE)
    return false;
}

- (void)_didPresentViewController:(UIViewController *)viewController
{
    if (isQuickboardViewController(viewController))
        [self _invokeShowKeyboardCallbackIfNecessary];
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

#if HAVE(UI_EDIT_MENU_INTERACTION)

- (void)webView:(WKWebView *)webView willPresentEditMenuWithAnimator:(id<UIEditMenuInteractionAnimating>)animator
{
    [animator addCompletion:[strongSelf = RetainPtr { self }] {
        [strongSelf _didShowMenu];
    }];
}

- (void)webView:(WKWebView *)webView willDismissEditMenuWithAnimator:(id<UIEditMenuInteractionAnimating>)animator
{
    [animator addCompletion:[strongSelf = RetainPtr { self }] {
        [strongSelf _didHideMenu];
    }];
}

#endif // HAVE(UI_EDIT_MENU_INTERACTION)

#pragma mark - _WKInputDelegate

- (void)_webView:(WKWebView *)webView willStartInputSession:(id <_WKFormInputSession>)inputSession
{
    if (self.willStartInputSessionCallback)
        self.willStartInputSessionCallback();
}

- (_WKFocusStartsInputSessionPolicy)_webView:(WKWebView *)webView decidePolicyForFocusedElement:(id<_WKFocusedElementInfo>)info
{
    return self.focusStartsInputSessionPolicy;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return gestureRecognizer == _windowTapGestureRecognizer;
}

#endif // PLATFORM(IOS_FAMILY)

- (void)_didPresentContactPicker
{
    if (self.showingContactPicker)
        return;

    self.showingContactPicker = YES;
    if (self.didShowContactPickerCallback)
        self.didShowContactPickerCallback();
}

- (void)_didDismissContactPicker
{
    if (!self.showingContactPicker)
        return;

    self.showingContactPicker = NO;
    if (self.didHideContactPickerCallback)
        self.didHideContactPickerCallback();
}

- (void)_didLoadAppInitiatedRequest:(void (^)(BOOL result))completionHandler
{
    [super _didLoadAppInitiatedRequest:completionHandler];
}

- (void)_didLoadNonAppInitiatedRequest:(void (^)(BOOL result))completionHandler
{
    [super _didLoadNonAppInitiatedRequest:completionHandler];
}

#if HAVE(UI_EDIT_MENU_INTERACTION)

- (void)immediatelyDismissEditMenuInteractionIfNeeded
{
    if (!self.isShowingMenu)
        return;

    self.showingMenu = NO;

    [UIView performWithoutAnimation:^{
        for (id<UIInteraction> interaction in self.contentView.interactions) {
            if (auto *editMenuInteraction = dynamic_objc_cast<UIEditMenuInteraction>(interaction))
                [editMenuInteraction dismissMenu];
        }
    }];
}

#endif // HAVE(UI_EDIT_MENU_INTERACTION)

@end
