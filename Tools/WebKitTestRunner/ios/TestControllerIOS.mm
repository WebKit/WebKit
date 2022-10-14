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
#import "TextInputSPI.h"
#import "UIKitSPI.h"
#import "UIPasteboardConsistencyEnforcer.h"
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
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(TextInput)
SOFT_LINK_CLASS(TextInput, TIPreferencesController);

static void overrideSyncInputManagerToAcceptedAutocorrection(id, SEL, TIKeyboardCandidate *candidate, TIKeyboardInput *input)
{
    // Intentionally unimplemented. See usage below for more information.
}

static BOOL overrideIsInHardwareKeyboardMode()
{
    return NO;
}

static void overridePresentMenuOrPopoverOrViewController()
{
}

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
    UIView *contentView = mainWebView()->platformView().contentView;
    UIView *selectionView = [contentView valueForKeyPath:@"interactionAssistant.selectionView"];
    [selectionView _removeAllAnimations:YES];
}

void TestController::platformInitialize(const Options& options)
{
    setUpIOSLayoutTestCommunication();
    cocoaPlatformInitialize(options);

    [UIApplication sharedApplication].idleTimerDisabled = YES;
    [[UIScreen mainScreen] _setScale:2.0];

    auto center = CFNotificationCenterGetLocalCenter();
    CFNotificationCenterAddObserver(center, this, handleKeyboardWillHideNotification, (CFStringRef)UIKeyboardWillHideNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, this, handleKeyboardDidHideNotification, (CFStringRef)UIKeyboardDidHideNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CFNotificationCenterAddObserver(center, this, handleMenuWillHideNotification, (CFStringRef)UIMenuControllerWillHideMenuNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, this, handleMenuDidHideNotification, (CFStringRef)UIMenuControllerDidHideMenuNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

void TestController::platformDestroy()
{
    tearDownIOSLayoutTestCommunication();

    auto center = CFNotificationCenterGetLocalCenter();
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIKeyboardWillHideNotification, nullptr);
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIKeyboardDidHideNotification, nullptr);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIMenuControllerWillHideMenuNotification, nullptr);
    CFNotificationCenterRemoveObserver(center, this, (CFStringRef)UIMenuControllerDidHideMenuNotification, nullptr);
    ALLOW_DEPRECATED_DECLARATIONS_END
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

static _WKDragInteractionPolicy dragInteractionPolicy(const TestOptions& options)
{
    auto policy = options.dragInteractionPolicy();
    if (policy == "always-enable")
        return _WKDragInteractionPolicyAlwaysEnable;
    if (policy == "always-disable")
        return _WKDragInteractionPolicyAlwaysDisable;
    return _WKDragInteractionPolicyDefault;
}

static _WKFocusStartsInputSessionPolicy focusStartsInputSessionPolicy(const TestOptions& options)
{
    auto policy = options.focusStartsInputSessionPolicy();
    if (policy == "allow")
        return _WKFocusStartsInputSessionPolicyAllow;
    if (policy == "disallow")
        return _WKFocusStartsInputSessionPolicyDisallow;
    return _WKFocusStartsInputSessionPolicyAuto;
}

void TestController::restorePortraitOrientationIfNeeded()
{
#if PLATFORM(IOS)
    auto *scene = mainWebView()->platformView().window.windowScene;
    if (scene.interfaceOrientation == UIInterfaceOrientationPortrait)
        return;

    lockScreenOrientation(kWKScreenOrientationTypePortraitPrimary);

    auto startTime = MonotonicTime::now();
    while ([NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:NSDate.distantPast]) {
        if (scene.interfaceOrientation == UIInterfaceOrientationPortrait)
            break;

        if (MonotonicTime::now() - startTime >= m_currentInvocation->shortTimeout())
            break;
    }
    unlockScreenOrientation();
#else
    [[UIDevice currentDevice] setOrientation:UIDeviceOrientationPortrait animated:NO];
#endif
}

bool TestController::platformResetStateToConsistentValues(const TestOptions& options)
{
    cocoaResetStateToConsistentValues(options);

    [UIKeyboardImpl.activeInstance setCorrectionLearningAllowed:NO];
    [pasteboardConsistencyEnforcer() clearPasteboard];
    [[UIApplication sharedApplication] _cancelAllTouches];
    [[UIScreen mainScreen] _setScale:2.0];
    [[HIDEventGenerator sharedHIDEventGenerator] resetActiveModifiers];

    restorePortraitOrientationIfNeeded();

    // Ensures that only the UCB is on-screen when showing the keyboard, if the hardware keyboard is attached.
    TIPreferencesController *textInputPreferences = [getTIPreferencesControllerClass() sharedPreferencesController];
    if (!textInputPreferences.automaticMinimizationEnabled)
        textInputPreferences.automaticMinimizationEnabled = YES;

    UIKeyboardPreferencesController *keyboardPreferences = UIKeyboardPreferencesController.sharedPreferencesController;
    // Ensures that changing selection does not cause the software keyboard to appear,
    // even when the hardware keyboard is attached.
    auto hardwareKeyboardLastSeenPreferenceKey = @"HardwareKeyboardLastSeen";
    auto preferencesActions = keyboardPreferences.preferencesActions;
    if (![preferencesActions oneTimeActionCompleted:hardwareKeyboardLastSeenPreferenceKey])
        [preferencesActions didTriggerOneTimeAction:hardwareKeyboardLastSeenPreferenceKey];

    auto didShowContinuousPathIntroductionKey = @"DidShowContinuousPathIntroduction";
    if (![preferencesActions oneTimeActionCompleted:didShowContinuousPathIntroductionKey])
        [preferencesActions didTriggerOneTimeAction:didShowContinuousPathIntroductionKey];

    // Disables the dictation keyboard shortcut for testing.
    auto dictationKeyboardShortcutPreferenceKey = @"HWKeyboardDictationShortcut";
    auto dictationKeyboardShortcutValueForTesting = @(-1);
    if (![dictationKeyboardShortcutValueForTesting isEqual:[keyboardPreferences valueForPreferenceKey:dictationKeyboardShortcutPreferenceKey]]) {
        [keyboardPreferences setValue:dictationKeyboardShortcutValueForTesting forPreferenceKey:dictationKeyboardShortcutPreferenceKey];
        CFPreferencesSetAppValue((__bridge CFStringRef)dictationKeyboardShortcutPreferenceKey, (__bridge CFNumberRef)dictationKeyboardShortcutValueForTesting, CFSTR("com.apple.Preferences"));
    }

    GSEventSetHardwareKeyboardAttached(true, 0);

    // Ignore calls to inform the keyboard daemon that we accepted autocorrection candidates.
    // This prevents the device from learning misspelled words in between layout tests.
    method_setImplementation(class_getInstanceMethod(UIKeyboardImpl.class, @selector(syncInputManagerToAcceptedAutocorrection:forInput:)), reinterpret_cast<IMP>(overrideSyncInputManagerToAcceptedAutocorrection));

    // Override the implementation of +[UIKeyboard isInHardwareKeyboardMode] to ensure that test runs are deterministic
    // regardless of whether a hardware keyboard is attached. We intentionally never restore the original implementation.
    //
    // FIXME: Investigate whether this can be removed. The swizzled return value is inconsistent with GSEventSetHardwareKeyboardAttached.
    method_setImplementation(class_getClassMethod([UIKeyboard class], @selector(isInHardwareKeyboardMode)), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));

    if (m_overriddenKeyboardInputMode) {
        m_overriddenKeyboardInputMode = nil;
        m_inputModeSwizzlers.clear();
        [UIKeyboardImpl.sharedInstance prepareKeyboardInputModeFromPreferences:nil];
    }

    m_presentPopoverSwizzlers.clear();
    if (!options.shouldPresentPopovers()) {
#if USE(UICONTEXTMENU)
        m_presentPopoverSwizzlers.append(makeUnique<InstanceMethodSwizzler>([UIContextMenuInteraction class], @selector(_presentMenuAtLocation:), reinterpret_cast<IMP>(overridePresentMenuOrPopoverOrViewController)));
#endif
        m_presentPopoverSwizzlers.append(makeUnique<InstanceMethodSwizzler>([UIPopoverController class], @selector(presentPopoverFromRect:inView:permittedArrowDirections:animated:), reinterpret_cast<IMP>(overridePresentMenuOrPopoverOrViewController)));
        m_presentPopoverSwizzlers.append(makeUnique<InstanceMethodSwizzler>([UIViewController class], @selector(presentViewController:animated:completion:), reinterpret_cast<IMP>(overridePresentMenuOrPopoverOrViewController)));
    }

    BOOL shouldRestoreFirstResponder = NO;
    if (PlatformWebView* platformWebView = mainWebView()) {
        TestRunnerWKWebView *webView = platformWebView->platformView();
        webView._suppressSoftwareKeyboard = NO;
        webView._stableStateOverride = nil;
        webView._scrollingUpdatesDisabledForTesting = NO;
        webView.usesSafariLikeRotation = NO;
        webView.overrideSafeAreaInsets = UIEdgeInsetsZero;
        [webView _clearOverrideLayoutParameters];
        [webView _clearInterfaceOrientationOverride];
        [webView setAllowedMenuActions:nil];
        webView._dragInteractionPolicy = dragInteractionPolicy(options);
        webView.focusStartsInputSessionPolicy = focusStartsInputSessionPolicy(options);

#if HAVE(UIFINDINTERACTION)
        webView.findInteractionEnabled = options.findInteractionEnabled();
#endif

        UIScrollView *scrollView = webView.scrollView;
        [scrollView _removeAllAnimations:YES];
        [scrollView setZoomScale:1 animated:NO];
        scrollView.firstResponderKeyboardAvoidanceEnabled = YES;

        auto currentContentInset = scrollView.contentInset;
        auto contentInsetTop = options.contentInsetTop();
        if (currentContentInset.top != contentInsetTop) {
            currentContentInset.top = contentInsetTop;
            scrollView.contentInset = currentContentInset;
            scrollView.contentOffset = CGPointMake(-currentContentInset.left, -currentContentInset.top);
        }

        if (webView.interactingWithFormControl)
            shouldRestoreFirstResponder = [webView resignFirstResponder];

        [webView immediatelyDismissContextMenuIfNeeded];

#if HAVE(UI_EDIT_MENU_INTERACTION)
        [webView immediatelyDismissEditMenuInteractionIfNeeded];
#endif
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
            // As a last resort, just dismiss the remaining presented view controller ourselves.
            __block BOOL isDoneDismissingViewController = NO;
            [webViewController dismissViewControllerAnimated:NO completion:^{
                isDoneDismissingViewController = YES;
            }];
            runUntil(isDoneDismissingViewController, m_currentInvocation->shortTimeout());
            hasPresentedViewController = !![webViewController presentedViewController];
        }
        
        if (hasPresentedViewController) {
            TestInvocation::dumpWebProcessUnresponsiveness("TestController::platformResetStateToConsistentValues - Failed to remove presented view controller\n");
            return false;
        }
    }

    if (shouldRestoreFirstResponder)
        [mainWebView()->platformView() becomeFirstResponder];

    return true;
}

void TestController::platformConfigureViewForTest(const TestInvocation& test)
{
    [[GeneratedTouchesDebugWindow sharedGeneratedTouchesDebugWindow] setShouldShowTouches:test.options().shouldShowTouches()];

    TestRunnerWKWebView *webView = mainWebView()->platformView();

    if (!test.options().useFlexibleViewport())
        return;

    UIWindowScene *scene = webView.window.windowScene;
    CGRect sceneBounds = [UIScreen mainScreen].bounds;
    if (scene.sizeRestrictions) {
        // For platforms that support resizeable scenes, resize to match iPad 5th Generation,
        // the default iPad device used for layout testing.
        // We add the status bar in here because it is subtracted back out in viewRectForWindowRect.
        static constexpr CGSize defaultTestingiPadViewSize = { 768, 1004 };
        sceneBounds = CGRectMake(0, 0, defaultTestingiPadViewSize.width, defaultTestingiPadViewSize.height + CGRectGetHeight(UIApplication.sharedApplication.statusBarFrame));
        scene.sizeRestrictions.minimumSize = sceneBounds.size;
        scene.sizeRestrictions.maximumSize = sceneBounds.size;
    }

    CGSize oldSize = webView.bounds.size;
    mainWebView()->resizeTo(sceneBounds.size.width, sceneBounds.size.height, PlatformWebView::WebViewSizingMode::HeightRespectsStatusBar);
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

TestFeatures TestController::platformSpecificFeatureDefaultsForTest(const TestCommand&) const
{
    return { };
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
    static NeverDestroyed<RetainPtr<NSString>> platformLibraryPath;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        platformLibraryPath.get() = [@"~/Library/Application Support/WebKitTestRunner" stringByExpandingTildeInPath];
    });
    return [platformLibraryPath.get() UTF8String];
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

UIPasteboardConsistencyEnforcer *TestController::pasteboardConsistencyEnforcer()
{
    if (!m_pasteboardConsistencyEnforcer)
        m_pasteboardConsistencyEnforcer = adoptNS([[UIPasteboardConsistencyEnforcer alloc] initWithPasteboardName:UIPasteboardNameGeneral]);
    return m_pasteboardConsistencyEnforcer.get();
}

#if PLATFORM(IOS)
void TestController::lockScreenOrientation(WKScreenOrientationType orientation)
{
    TestRunnerWKWebView *webView = mainWebView()->platformView();

    // Make sure this is the top-most window or the call to setNeedsUpdateOfSupportedInterfaceOrientations
    // below won't do anything. UIKit prioritizes the top-most scene-sized window when determining interface
    // orientation.
    [webView.window makeKeyWindow];

    switch (orientation) {
    case kWKScreenOrientationTypePortraitPrimary:
        webView.supportedInterfaceOrientations = UIInterfaceOrientationMaskPortrait;
        break;
    case kWKScreenOrientationTypePortraitSecondary:
        webView.supportedInterfaceOrientations = UIInterfaceOrientationMaskPortraitUpsideDown;
        break;
    case kWKScreenOrientationTypeLandscapePrimary:
        webView.supportedInterfaceOrientations = UIInterfaceOrientationMaskLandscapeLeft;
        break;
    case kWKScreenOrientationTypeLandscapeSecondary:
        webView.supportedInterfaceOrientations = UIInterfaceOrientationMaskLandscapeRight;
        break;
    }
    [UIView performWithoutAnimation:^{
        [webView.window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
    }];
}

void TestController::unlockScreenOrientation()
{
    TestRunnerWKWebView *webView = mainWebView()->platformView();
    webView.supportedInterfaceOrientations = UIInterfaceOrientationMaskAll;
    [UIView performWithoutAnimation:^{
        [webView.window.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
    }];
}
#endif

} // namespace WTR
