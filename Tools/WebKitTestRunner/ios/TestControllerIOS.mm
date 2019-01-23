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
#import <objc/runtime.h>
#import <wtf/MainThread.h>

static BOOL overrideIsInHardwareKeyboardMode()
{
    return NO;
}

namespace WTR {

static bool isDoneWaitingForKeyboardToDismiss = true;

static void handleKeyboardWillHideNotification(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    isDoneWaitingForKeyboardToDismiss = false;
}

static void handleKeyboardDidHideNotification(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    isDoneWaitingForKeyboardToDismiss = true;
}

void TestController::notifyDone()
{
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

void TestController::platformResetPreferencesToConsistentValues()
{
    WKPreferencesRef preferences = platformPreferences();
    WKPreferencesSetTextAutosizingEnabled(preferences, false);
}

void TestController::platformResetStateToConsistentValues(const TestOptions& options)
{
    cocoaResetStateToConsistentValues(options);

    [[UIApplication sharedApplication] _cancelAllTouches];
    [[UIDevice currentDevice] setOrientation:UIDeviceOrientationPortrait animated:NO];

    m_inputModeSwizzlers.clear();
    m_overriddenKeyboardInputMode = nil;
    
    BOOL shouldRestoreFirstResponder = NO;
    if (PlatformWebView* platformWebView = mainWebView()) {
        TestRunnerWKWebView *webView = platformWebView->platformView();
        webView._stableStateOverride = nil;
        webView.usesSafariLikeRotation = NO;
        webView.overrideSafeAreaInsets = UIEdgeInsetsZero;
        [webView _clearOverrideLayoutParameters];
        [webView _clearInterfaceOrientationOverride];

        UIScrollView *scrollView = webView.scrollView;
        [scrollView _removeAllAnimations:YES];
        [scrollView setZoomScale:1 animated:NO];
        scrollView.contentInset = UIEdgeInsetsMake(options.contentInsetTop, 0, 0, 0);
        scrollView.contentOffset = CGPointMake(0, -options.contentInsetTop);

        if (webView.interactingWithFormControl)
            shouldRestoreFirstResponder = [webView resignFirstResponder];
    }

    runUntil(isDoneWaitingForKeyboardToDismiss, m_currentInvocation->shortTimeout());

    if (shouldRestoreFirstResponder)
        [mainWebView()->platformView() becomeFirstResponder];
}

void TestController::platformConfigureViewForTest(const TestInvocation& test)
{
    if (!test.options().useFlexibleViewport)
        return;
        
    TestRunnerWKWebView *webView = mainWebView()->platformView();

    if (test.options().shouldIgnoreMetaViewport)
        webView.configuration.preferences._shouldIgnoreMetaViewport = YES;

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

const char* TestController::platformLibraryPathForTesting()
{
    return [[@"~/Library/Application Support/WebKitTestRunner" stringByExpandingTildeInPath] UTF8String];
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
    m_inputModeSwizzlers.uncheckedAppend(std::make_unique<InstanceMethodSwizzler>(controllerClass, @selector(currentInputMode), reinterpret_cast<IMP>(swizzleCurrentInputMode)));
    m_inputModeSwizzlers.uncheckedAppend(std::make_unique<InstanceMethodSwizzler>(controllerClass, @selector(currentInputModeInPreference), reinterpret_cast<IMP>(swizzleCurrentInputMode)));
    m_inputModeSwizzlers.uncheckedAppend(std::make_unique<InstanceMethodSwizzler>(controllerClass, @selector(activeInputModes), reinterpret_cast<IMP>(swizzleActiveInputModes)));
    [UIKeyboardImpl.sharedInstance prepareKeyboardInputModeFromPreferences:nil];
}

} // namespace WTR
