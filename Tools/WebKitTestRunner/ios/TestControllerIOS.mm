/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "TestController.h"

#import "GeneratedTouchesDebugWindow.h"
#import "HIDEventGenerator.h"
#import "IOSLayoutTestCommunication.h"
#import "PlatformWebView.h"
#import "TestInvocation.h"
#import "TestRunnerWKWebView.h"
#import "UIKitSPI.h"
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKStringCF.h>
#import <WebKit/WKUserContentControllerPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <objc/runtime.h>
#import <pal/spi/ios/GraphicsServicesSPI.h>
#import <wtf/MainThread.h>

static BOOL overrideIsInHardwareKeyboardMode()
{
    return NO;
}

static void overridePresentMenuOrPopoverOrViewController()
{
}

#if !HAVE(NONDESTRUCTIVE_IMAGE_PASTE_SUPPORT_QUERY)

static BOOL overrideKeyboardDelegateSupportsImagePaste(id, SEL)
{
    return NO;
}

#endif

namespace WTR {

static bool isDoneWaitingForKeyboardToDismiss = true;
static bool isDoneWaitingForMenuToDismiss = true;

static void handleKeyboardWillHideNotification(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    isDoneWaitingForKeyboardToDismiss = false;
}

static void handleKeyboardDidHideNotification(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    isDoneWaitingForKeyboardToDismiss = true;
}

static void handleMenuWillHideNotification(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    isDoneWaitingForMenuToDismiss = false;
}

static void handleMenuDidHideNotification(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    isDoneWaitingForMenuToDismiss = true;
}

void TestController::notifyDone()
{
    UIView *contentView = [mainWebView()->platformView() valueForKeyPath:@"_currentContentView"];
    UIView *selectionView = [contentView valueForKeyPath:@"interactionAssistant.selectionView"];
    [selectionView _removeAllAnimations:YES];
}

void TestController::platformInitialize()
{
    setUpIOSLayoutTestCommunication();
    cocoaPlatformInitialize();

    [UIApplication sharedApplication].idleTimerDisabled = YES;
    [[UIScreen mainScreen] _setScale:2.0];

    auto center = CFNotificationCenterGetLocalCenter();
    CFNotificationCenterAddObserver(center, this, handleKeyboardWillHideNotification, (CFStringRef)UIKeyboardWillHideNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, this, handleKeyboardDidHideNotification, (CFStringRef)UIKeyboardDidHideNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, this, handleMenuWillHideNotification, (CFStringRef)UIMenuControllerWillHideMenuNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, this, handleMenuDidHideNotification, (CFStringRef)UIMenuControllerDidHideMenuNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);

    // Override the implementation of +[UIKeyboard isInHardwareKeyboardMode] to ensure that test runs are deterministic
    // regardless of whether a hardware keyboard is attached. We intentionally never restore the original implementation.
    method_setImplementation(class_getClassMethod([UIKeyboard class], @selector(isInHardwareKeyboardMode)), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));
}

void TestController::platformDestroy()
{
    tearDownIOSLayoutTestCommunication();

    auto center = CFNotificationCenterGetLocalCenter();
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIKeyboardWillHideNotification, nullptr);
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIKeyboardDidHideNotification, nullptr);
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIMenuControllerWillHideMenuNotification, nullptr);
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIMenuControllerDidHideMenuNotification, nullptr);
}

void TestController::initializeInjectedBundlePath()
{
    NSString *nsBundlePath = [[NSBundle mainBundle].builtInPlugInsPath stringByAppendingPathComponent:@"WebKitTestRunnerInjectedBundle.bundle"];
    m_injectedBundlePath.adopt(WKStringCreateWithCFString((CFStringRef)nsBundlePath));
}

void TestController::initializeTestPluginDirectory()
{
    m_testPluginDirectory.adopt(WKStringCreateWithCFString((CFStringRef)[[NSBundle mainBundle] bundlePath]));
}

void TestController::configureContentExtensionForTest(const TestInvocation&)
{
}

void TestController::platformResetPreferencesToConsistentValues()
{
    WKPreferencesRef preferences = platformPreferences();
    WKPreferencesSetTextAutosizingEnabled(preferences, false);
    WKPreferencesSetTextAutosizingUsesIdempotentMode(preferences, false);
    WKPreferencesSetContentChangeObserverEnabled(preferences, false);
    [(__bridge WKPreferences *)preferences _setShouldIgnoreMetaViewport:NO];
}

bool TestController::platformResetStateToConsistentValues(const TestOptions& options)
{
    cocoaResetStateToConsistentValues(options);

    [[UIApplication sharedApplication] _cancelAllTouches];
    [[UIDevice currentDevice] setOrientation:UIDeviceOrientationPortrait animated:NO];
    UIKeyboardPreferencesController *keyboardPreferences = UIKeyboardPreferencesController.sharedPreferencesController;
    NSString *automaticMinimizationEnabledPreferenceKey = @"AutomaticMinimizationEnabled";
    if (![keyboardPreferences boolForPreferenceKey:automaticMinimizationEnabledPreferenceKey]) {
        [keyboardPreferences setValue:@YES forPreferenceKey:automaticMinimizationEnabledPreferenceKey];
        CFPreferencesSetAppValue((__bridge CFStringRef)automaticMinimizationEnabledPreferenceKey, kCFBooleanTrue, CFSTR("com.apple.Preferences"));
    }

    GSEventSetHardwareKeyboardAttached(true, 0);

#if !HAVE(NONDESTRUCTIVE_IMAGE_PASTE_SUPPORT_QUERY)
    // FIXME: Remove this workaround once -[UIKeyboardImpl delegateSupportsImagePaste] no longer increments the general pasteboard's changeCount.
    if (!m_keyboardDelegateSupportsImagePasteSwizzler)
        m_keyboardDelegateSupportsImagePasteSwizzler = makeUnique<InstanceMethodSwizzler>(UIKeyboardImpl.class, @selector(delegateSupportsImagePaste), reinterpret_cast<IMP>(overrideKeyboardDelegateSupportsImagePaste));
#endif

    m_inputModeSwizzlers.clear();
    m_overriddenKeyboardInputMode = nil;

    m_presentPopoverSwizzlers.clear();
    if (!options.shouldPresentPopovers) {
#if USE(UICONTEXTMENU)
        m_presentPopoverSwizzlers.append(makeUnique<InstanceMethodSwizzler>([UIContextMenuInteraction class], @selector(_presentMenuAtLocation:), reinterpret_cast<IMP>(overridePresentMenuOrPopoverOrViewController)));
#endif
        m_presentPopoverSwizzlers.append(makeUnique<InstanceMethodSwizzler>([UIPopoverController class], @selector(presentPopoverFromRect:inView:permittedArrowDirections:animated:), reinterpret_cast<IMP>(overridePresentMenuOrPopoverOrViewController)));
        m_presentPopoverSwizzlers.append(makeUnique<InstanceMethodSwizzler>([UIViewController class], @selector(presentViewController:animated:completion:), reinterpret_cast<IMP>(overridePresentMenuOrPopoverOrViewController)));
    }

    BOOL shouldRestoreFirstResponder = NO;
    if (PlatformWebView* platformWebView = mainWebView()) {
        TestRunnerWKWebView *webView = platformWebView->platformView();
        webView._stableStateOverride = nil;
        webView._scrollingUpdatesDisabledForTesting = NO;
        webView.usesSafariLikeRotation = NO;
        webView.overrideSafeAreaInsets = UIEdgeInsetsZero;
        [webView _clearOverrideLayoutParameters];
        [webView _clearInterfaceOrientationOverride];
        [webView resetCustomMenuAction];
        [webView setAllowedMenuActions:nil];

        UIScrollView *scrollView = webView.scrollView;
        [scrollView _removeAllAnimations:YES];
        [scrollView setZoomScale:1 animated:NO];
        scrollView.contentInset = UIEdgeInsetsMake(options.contentInsetTop, 0, 0, 0);
        scrollView.contentOffset = CGPointMake(0, -options.contentInsetTop);

        if (webView.interactingWithFormControl)
            shouldRestoreFirstResponder = [webView resignFirstResponder];
    }

    UIMenuController.sharedMenuController.menuVisible = NO;

    runUntil(isDoneWaitingForKeyboardToDismiss, m_currentInvocation->shortTimeout());
    runUntil(isDoneWaitingForMenuToDismiss, m_currentInvocation->shortTimeout());

    if (PlatformWebView* platformWebView = mainWebView()) {
        TestRunnerWKWebView *webView = platformWebView->platformView();
        UIViewController *webViewController = [[webView window] rootViewController];

        MonotonicTime waitEndTime = MonotonicTime::now() + m_currentInvocation->shortTimeout();
        
        bool hasPresentedViewController = !![webViewController presentedViewController];
        while (hasPresentedViewController && MonotonicTime::now() < waitEndTime) {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]];
            hasPresentedViewController = !![webViewController presentedViewController];
        }
        
        if (hasPresentedViewController) {
            TestInvocation::dumpWebProcessUnresponsiveness("TestController::platformResetPreferencesToConsistentValues - Failed to remove presented view controller\n");
            return false;
        }
    }

    if (shouldRestoreFirstResponder)
        [mainWebView()->platformView() becomeFirstResponder];

    [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"WebKitDebugIsInAppBrowserPrivacyEnabled"];

    return true;
}

void TestController::platformConfigureViewForTest(const TestInvocation& test)
{
    TestRunnerWKWebView *webView = mainWebView()->platformView();

    if (test.options().shouldIgnoreMetaViewport)
        webView.configuration.preferences._shouldIgnoreMetaViewport = YES;

    if (!test.options().useFlexibleViewport)
        return;

    CGRect screenBounds = [UIScreen mainScreen].bounds;
    CGSize oldSize = webView.bounds.size;
    mainWebView()->resizeTo(screenBounds.size.width, screenBounds.size.height, PlatformWebView::WebViewSizingMode::HeightRespectsStatusBar);
    CGSize newSize = webView.bounds.size;
    
    if (!CGSizeEqualToSize(oldSize, newSize)) {
        __block bool doneResizing = false;
        [webView _doAfterNextVisibleContentRectUpdate: ^{
            doneResizing = true;
        }];

        platformRunUntil(doneResizing, noTimeout);
    }
    
    // We also pass data to InjectedBundle::beginTesting() to have it call
    // WKBundlePageSetUseTestingViewportConfiguration(false).
}

void TestController::updatePlatformSpecificTestOptionsForTest(TestOptions& options, const std::string&) const
{
    options.shouldShowTouches = shouldShowTouches();
    [[GeneratedTouchesDebugWindow sharedGeneratedTouchesDebugWindow] setShouldShowTouches:options.shouldShowTouches];
}

void TestController::platformInitializeContext()
{
}

void TestController::runModal(PlatformWebView* view)
{
    UIWindow *window = [view->platformView() window];
    if (!window)
        return;
    // FIXME: how to perform on iOS?
//    [[UIApplication sharedApplication] runModalForWindow:window];
}

void TestController::abortModal()
{
}

const char* TestController::platformLibraryPathForTesting()
{
    static NSString *platformLibraryPath = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        platformLibraryPath = [[@"~/Library/Application Support/WebKitTestRunner" stringByExpandingTildeInPath] retain];
    });
    return platformLibraryPath.UTF8String;
}

void TestController::setHidden(bool)
{
    // FIXME: implement for iOS
}

static UIKeyboardInputMode *swizzleCurrentInputMode()
{
    return TestController::singleton().overriddenKeyboardInputMode();
}

static NSArray<UIKeyboardInputMode *> *swizzleActiveInputModes()
{
    return @[ TestController::singleton().overriddenKeyboardInputMode() ];
}

void TestController::setKeyboardInputModeIdentifier(const String& identifier)
{
    m_inputModeSwizzlers.clear();
    m_overriddenKeyboardInputMode = [UIKeyboardInputMode keyboardInputModeWithIdentifier:identifier];
    if (!m_overriddenKeyboardInputMode) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto controllerClass = UIKeyboardInputModeController.class;
    m_inputModeSwizzlers.reserveCapacity(3);
    m_inputModeSwizzlers.uncheckedAppend(makeUnique<InstanceMethodSwizzler>(controllerClass, @selector(currentInputMode), reinterpret_cast<IMP>(swizzleCurrentInputMode)));
    m_inputModeSwizzlers.uncheckedAppend(makeUnique<InstanceMethodSwizzler>(controllerClass, @selector(currentInputModeInPreference), reinterpret_cast<IMP>(swizzleCurrentInputMode)));
    m_inputModeSwizzlers.uncheckedAppend(makeUnique<InstanceMethodSwizzler>(controllerClass, @selector(activeInputModes), reinterpret_cast<IMP>(swizzleActiveInputModes)));
    [UIKeyboardImpl.sharedInstance prepareKeyboardInputModeFromPreferences:nil];
}

} // namespace WTR
