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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAction.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WKNavigationActionPrivate.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMenuItem.h"
#import "WebExtensionMenuItemContextParameters.h"
#import "WebExtensionMenuItemParameters.h"
#import "WebExtensionTabParameters.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKWebExtensionActionInternal.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

#if PLATFORM(VISION)
constexpr CGFloat maximumPopoverWidth = 734;
constexpr CGFloat maximumPopoverHeight = 734;
#else
constexpr CGFloat maximumPopoverWidth = 800;
constexpr CGFloat maximumPopoverHeight = 600;
#endif

constexpr CGFloat minimumPopoverWidth = 50;
constexpr CGFloat minimumPopoverHeight = 50;

#ifdef NDEBUG
constexpr NSTimeInterval popoverShowTimeout = 1;
constexpr NSTimeInterval popoverStableSizeDuration = 0.1;
#else
// Debug builds are slower, so give rendering more time.
constexpr NSTimeInterval popoverShowTimeout = 2;
constexpr NSTimeInterval popoverStableSizeDuration = 0.2;
#endif

using namespace WebKit;

@interface _WKWebExtensionActionWebViewDelegate : NSObject <WKNavigationDelegatePrivate, WKUIDelegatePrivate>
@end

@implementation _WKWebExtensionActionWebViewDelegate {
    WeakPtr<WebExtensionAction> _webExtensionAction;
}

- (instancetype)initWithWebExtensionAction:(WebExtensionAction&)action
{
    if (!(self = [super init]))
        return nil;

    _webExtensionAction = action;

    return self;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (!_webExtensionAction || !_webExtensionAction->extensionContext()) {
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }

    NSURL *targetURL = navigationAction.request.URL;
    bool isURLForThisExtension = _webExtensionAction->extensionContext()->isURLForThisExtension(targetURL);

    // New window or main frame navigation to an external URL opens in a new tab.
    if (!navigationAction.targetFrame || (navigationAction.targetFrame.isMainFrame && !isURLForThisExtension)) {
        RefPtr currentWindow = _webExtensionAction->window();
        RefPtr currentTab = _webExtensionAction->tab();
        if (!currentWindow && currentTab)
            currentWindow = currentTab->window();

        WebExtensionTabParameters tabParameters;
        tabParameters.url = targetURL;
        tabParameters.windowIdentifier = currentWindow ? currentWindow->identifier() : WebExtensionWindowConstants::CurrentIdentifier;
        tabParameters.index = currentTab ? std::optional(currentTab->index() + 1) : std::nullopt;
        tabParameters.active = true;

        _webExtensionAction->extensionContext()->openNewTab(tabParameters, [](RefPtr<WebExtensionTab> newTab) {
            ASSERT(newTab);
        });

        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }

    // Allow subframes to load any URL.
    if (!navigationAction.targetFrame.isMainFrame) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    // Require an extension URL for the main frame.
    ASSERT(navigationAction.targetFrame.isMainFrame);
    ASSERT(isURLForThisExtension);

    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (!_webExtensionAction)
        return;

    _webExtensionAction->popupDidClose();
}

- (void)webViewDidClose:(WKWebView *)webView
{
    if (!_webExtensionAction)
        return;

    _webExtensionAction->popupDidClose();
}

@end

#if PLATFORM(IOS_FAMILY)
static void* kvoContext = &kvoContext;
#endif

@interface _WKWebExtensionActionWebView : WKWebView

@property (nonatomic, readonly) BOOL contentSizeHasStabilized;

@end

@implementation _WKWebExtensionActionWebView {
    WeakPtr<WebExtensionAction> _webExtensionAction;
    CGSize _previousContentSize;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration webExtensionAction:(WebExtensionAction&)action
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    _webExtensionAction = action;

#if PLATFORM(IOS_FAMILY)
    // macOS uses invalidateIntrinsicContentSize to track this.
    [self.scrollView addObserver:self forKeyPath:@"contentSize" options:NSKeyValueObservingOptionNew context:kvoContext];
#endif

    return self;
}

#if PLATFORM(IOS_FAMILY)
- (void)dealloc
{
    [self.scrollView removeObserver:self forKeyPath:@"contentSize" context:kvoContext];
}
#endif

#if PLATFORM(MAC)
- (void)invalidateIntrinsicContentSize
{
    [super invalidateIntrinsicContentSize];

    [self _contentSizeDidChange];
}
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSString *, id> *)change context:(void*)context
{
    if (context != kvoContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    ASSERT(object == self.scrollView);

    [self _contentSizeDidChange];
}
#endif // PLATFORM(IOS_FAMILY)

- (CGSize)_contentSize
{
#if PLATFORM(IOS_FAMILY)
    return self.scrollView.contentSize;
#else
    return self.intrinsicContentSize;
#endif
}

- (void)_contentSizeDidChange
{
    if (!_webExtensionAction)
        return;

    CGSize contentSize = self._contentSize;
    if (CGSizeEqualToSize(contentSize, _previousContentSize))
        return;

    SEL contentSizeStabilizedSelector = @selector(_checkIfContentSizeStabilizedAndPresentPopup);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:contentSizeStabilizedSelector object:nil];
    [self performSelector:contentSizeStabilizedSelector withObject:nil afterDelay:popoverStableSizeDuration];

    _previousContentSize = contentSize;
}

- (void)_checkIfContentSizeStabilizedAndPresentPopup
{
    if (!_webExtensionAction || self.loading)
        return;

    _webExtensionAction->popupSizeDidChange();

    CGSize contentSize = self._contentSize;
    if (contentSize.width < minimumPopoverWidth || contentSize.height < minimumPopoverHeight)
        return;

    _contentSizeHasStabilized = YES;

    _webExtensionAction->readyToPresentPopup();
}

@end

#if PLATFORM(IOS_FAMILY)
@interface _WKWebExtensionActionViewController : UIViewController <UIPopoverPresentationControllerDelegate>
@end

@implementation _WKWebExtensionActionViewController {
    WeakPtr<WebExtensionAction> _webExtensionAction;
    _WKWebExtensionActionWebView *_popupWebView;
    BOOL _presentedAsSheet;
}

- (instancetype)initWithWebExtensionAction:(WebExtensionAction&)action
{
    if (!(self = [super init]))
        return nil;

    RefPtr extensionContext = action.extensionContext();
    if (!extensionContext)
        return nil;

    _webExtensionAction = action;
    _popupWebView = dynamic_objc_cast<_WKWebExtensionActionWebView>(action.popupWebView());

    self.view = _popupWebView;
    self.modalPresentationStyle = UIModalPresentationPopover;
    self.popoverPresentationController.delegate = self;

    UINavigationItem *navigationItem = self.navigationItem;
    navigationItem.title = extensionContext->extension().displayName();
    navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(_dismissPopup)];

    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(_viewControllerDismissalTransitionDidEnd:) name:UIPresentationControllerDismissalTransitionDidEndNotification object:nil];

    return self;
}

- (void)viewIsAppearing:(BOOL)animated
{
    [self _updatePopoverContentSize];
}

- (UIModalPresentationStyle)adaptivePresentationStyleForPresentationController:(UIPresentationController *)presentationController traitCollection:(UITraitCollection *)traitCollection
{
    return traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact ? UIModalPresentationFormSheet : UIModalPresentationPopover;
}

- (void)presentationController:(UIPresentationController *)presentationController prepareAdaptivePresentationController:(UIPresentationController *)adaptivePresentationController
{
    if (auto *sheetPresentationController = dynamic_objc_cast<UISheetPresentationController>(adaptivePresentationController))
        [self _updateDetentForSheetPresentationController:sheetPresentationController];
}

- (UIViewController *)presentationController:(UIPresentationController *)presentationController viewControllerForAdaptivePresentationStyle:(UIModalPresentationStyle)style
{
    auto *presentedViewController = presentationController.presentedViewController;

    if (style != UIModalPresentationFormSheet)
        return presentedViewController;

    auto *navigationController = [[UINavigationController alloc] initWithRootViewController:presentedViewController];
    auto *navigationBar = navigationController.navigationBar;
    navigationBar.scrollEdgeAppearance = navigationBar.standardAppearance;
    return navigationController;
}

- (void)_viewControllerDismissalTransitionDidEnd:(NSNotification *)notification
{
    if (!_webExtensionAction || !objectForKey<NSNumber>(notification.userInfo, UIPresentationControllerDismissalTransitionDidEndCompletedKey).boolValue)
        return;

    auto *dismissedViewController = dynamic_objc_cast<UIViewController>(notification.object);
    if (dismissedViewController != self && dismissedViewController != self.navigationController)
        return;

    _webExtensionAction->closePopupWebView();
}

- (void)_updatePopoverContentSize
{
    auto *presentationController = [self _existingPresentationControllerImmediate:NO effective:YES];
    if (!presentationController)
        return;

    BOOL wasPresentedAsSheet = _presentedAsSheet;
    _presentedAsSheet = [presentationController isKindOfClass:UISheetPresentationController.class];

    if (_presentedAsSheet != wasPresentedAsSheet)
        [_popupWebView setNeedsLayout];

    if (_presentedAsSheet) {
        // Clear any overriding of layout parameters set as a popover.
        [_popupWebView _clearOverrideLayoutParameters];

        if (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPhone) {
            CGFloat widthOfDeviceInPortrait = CGRectGetWidth(UIScreen.mainScreen._referenceBounds);

            CGSize contentSize = self.preferredContentSize;
            contentSize.width = std::max(contentSize.width, widthOfDeviceInPortrait);

            self.preferredContentSize = contentSize;
        }

        [self _updateDetentForSheetPresentationController:dynamic_objc_cast<UISheetPresentationController>(presentationController)];
    } else {
        CGSize boundsSize = _popupWebView.bounds.size;
        CGSize contentSize = _popupWebView.scrollView.contentSize;
        CGSize desiredSize = CGSizeMake(std::min(contentSize.width, maximumPopoverWidth), std::min(contentSize.height, maximumPopoverHeight));

        CGFloat minimumWidth = std::min(desiredSize.width, boundsSize.width);
        CGFloat minimumHeight = _popupWebView.contentSizeHasStabilized ? std::min(desiredSize.height, boundsSize.height) : minimumPopoverHeight;
        CGSize minimumLayoutSize = CGSizeMake(minimumWidth, minimumHeight);

        [_popupWebView _overrideLayoutParametersWithMinimumLayoutSize:minimumLayoutSize minimumUnobscuredSizeOverride:minimumLayoutSize maximumUnobscuredSizeOverride:CGSizeZero];

        self.preferredContentSize = desiredSize;
    }
}

- (void)_updateDetentForSheetPresentationController:(UISheetPresentationController *)sheetPresentationController
{
    ASSERT(sheetPresentationController);

    // FIXME: rdar://74753245 - This should choose either medium or large based on the desired height of the content.
    sheetPresentationController.detents = @[ UISheetPresentationControllerDetent.mediumDetent, UISheetPresentationControllerDetent.largeDetent ];

    sheetPresentationController.prefersEdgeAttachedInCompactHeight = YES;
    sheetPresentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
}

- (void)_dismissPopup
{
    [self.presentingViewController dismissViewControllerAnimated:YES completion:nil];
}

@end
#endif // PLATFORM(IOS_FAMILY)

namespace WebKit {

WebExtensionAction::WebExtensionAction(WebExtensionContext& extensionContext)
    : m_extensionContext(extensionContext)
{
}

WebExtensionAction::WebExtensionAction(WebExtensionContext& extensionContext, WebExtensionTab& tab)
    : WebExtensionAction(extensionContext)
{
    m_tab = &tab;
}

WebExtensionAction::WebExtensionAction(WebExtensionContext& extensionContext, WebExtensionWindow& window)
    : WebExtensionAction(extensionContext)
{
    m_window = &window;
}

bool WebExtensionAction::operator==(const WebExtensionAction& other) const
{
    return this == &other || (m_extensionContext == other.m_extensionContext && m_tab == other.m_tab && m_window == other.m_window);
}

WebExtensionContext* WebExtensionAction::extensionContext() const
{
    return m_extensionContext.get();
}

void WebExtensionAction::clearCustomizations()
{
    if (!m_customIcons && m_customPopupPath.isNull() && m_customLabel.isNull() && m_customBadgeText.isNull() && !m_customEnabled && !m_blockedResourceCount)
        return;

    m_customIcons = nil;
    m_customPopupPath = nullString();
    m_customLabel = nullString();
    m_customBadgeText = nullString();
    m_customEnabled = std::nullopt;
    m_blockedResourceCount = 0;

    propertiesDidChange();
}

void WebExtensionAction::clearBlockedResourceCount()
{
    m_blockedResourceCount = 0;

    propertiesDidChange();
}

void WebExtensionAction::propertiesDidChange()
{
    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        [NSNotificationCenter.defaultCenter postNotificationName:_WKWebExtensionActionPropertiesDidChangeNotification object:wrapper() userInfo:nil];
    }).get());
}

WebExtensionAction* WebExtensionAction::fallbackAction() const
{
    if (!extensionContext())
        return nullptr;

    // Tab actions fallback to the window action.
    if (m_tab)
        return extensionContext()->getAction(m_tab->window().get()).ptr();

    // Window actions fallback to the default action.
    if (m_window)
        return &extensionContext()->defaultAction();

    // Default actions have no fallback.
    return nullptr;
}

CocoaImage *WebExtensionAction::icon(CGSize idealSize)
{
    if (!extensionContext())
        return nil;

    if (m_customIcons)
        return extensionContext()->extension().bestImageInIconsDictionary(m_customIcons.get(), idealSize.width > idealSize.height ? idealSize.width : idealSize.height);

    if (RefPtr fallback = fallbackAction())
        return fallback->icon(idealSize);

    // Default
    return extensionContext()->extension().actionIcon(idealSize);
}

void WebExtensionAction::setIconsDictionary(NSDictionary *icons)
{
    if ([(m_customIcons ?: @{ }) isEqualToDictionary:(icons ?: @{ })])
        return;

    m_customIcons = icons.count ? icons : nil;

    propertiesDidChange();
}

String WebExtensionAction::popupPath() const
{
    if (!extensionContext())
        return emptyString();

    if (!m_customPopupPath.isNull())
        return m_customPopupPath;

    if (RefPtr fallback = fallbackAction())
        return fallback->popupPath();

    // Default
    return extensionContext()->extension().actionPopupPath();
}

void WebExtensionAction::setPopupPath(String path)
{
    if (m_customPopupPath == path)
        return;

    m_customPopupPath = path;

    propertiesDidChange();
}

#if PLATFORM(IOS_FAMILY)
UIViewController *WebExtensionAction::popupViewController()
{
    if (!presentsPopup())
        return nil;

    if (m_popupViewController)
        return m_popupViewController.get();

    m_popupViewController = [[_WKWebExtensionActionViewController alloc] initWithWebExtensionAction:*this];

    return m_popupViewController.get();
}
#endif

WKWebView *WebExtensionAction::popupWebView(LoadOnFirstAccess loadOnFirstAccess)
{
    if (!presentsPopup() || !extensionContext())
        return nil;

    if (m_popupWebView || loadOnFirstAccess == LoadOnFirstAccess::No)
        return m_popupWebView.get();

    auto *webViewConfiguration = extensionContext()->webViewConfiguration(WebExtensionContext::WebViewPurpose::Popup);
    webViewConfiguration.suppressesIncrementalRendering = YES;

    m_popupWebViewDelegate = [[_WKWebExtensionActionWebViewDelegate alloc] initWithWebExtensionAction:*this];

    m_popupWebView = [[_WKWebExtensionActionWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration webExtensionAction:*this];
    m_popupWebView.get().navigationDelegate = m_popupWebViewDelegate.get();
    m_popupWebView.get().UIDelegate = m_popupWebViewDelegate.get();

    m_popupWebView.get().inspectable = extensionContext()->isInspectable();

    m_popupWebView.get().accessibilityLabel = extensionContext()->extension().displayName();

#if PLATFORM(MAC)
    m_popupWebView.get().accessibilityRole = NSAccessibilityPopoverRole;

    m_popupWebView.get()._sizeToContentAutoSizeMaximumSize = CGSizeMake(maximumPopoverWidth, maximumPopoverHeight);
    m_popupWebView.get()._useSystemAppearance = YES;

    [m_popupWebView setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
    [m_popupWebView setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationVertical];

    [NSLayoutConstraint activateConstraints:@[
        [m_popupWebView.get().widthAnchor constraintGreaterThanOrEqualToConstant:minimumPopoverWidth],
        [m_popupWebView.get().widthAnchor constraintLessThanOrEqualToConstant:maximumPopoverWidth],
        [m_popupWebView.get().heightAnchor constraintGreaterThanOrEqualToConstant:minimumPopoverHeight],
        [m_popupWebView.get().heightAnchor constraintLessThanOrEqualToConstant:maximumPopoverHeight]
    ]];
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
    [m_popupWebView _overrideViewportWithArguments:@{ @"width": @"device-width", @"initial-scale": @"1", @"minimum-scale": @"1", @"maximum-scale": @"1", @"user-scalable": @"no" }];
#endif

    extensionContext()->addPopupPage(*m_popupWebView.get()._page.get(), *this);

    auto url = URL { extensionContext()->baseURL(), popupPath() };
    [m_popupWebView loadRequest:[NSURLRequest requestWithURL:url]];

    return m_popupWebView.get();
}

bool WebExtensionAction::canProgrammaticallyPresentPopup() const
{
    if (!extensionContext())
        return false;

    RefPtr extensionController = extensionContext()->extensionController();
    if (!extensionController)
        return false;

    return [extensionController->delegate() respondsToSelector:@selector(webExtensionController:presentPopupForAction:forExtensionContext:completionHandler:)];
}

void WebExtensionAction::presentPopupWhenReady()
{
    if (!extensionContext())
        return;

    if (!canProgrammaticallyPresentPopup()) {
        RELEASE_LOG_ERROR(Extensions, "Delegate does not implement the webExtensionController:presentPopupForAction:forExtensionContext:completionHandler: method");
        return;
    }

    m_popupPresented = false;

    if (m_popupWebView) {
        readyToPresentPopup();
        return;
    }

    // Delay showing the popup until a minimum size or a timeout is reached.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(popoverShowTimeout * NSEC_PER_SEC)), dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }] {
        readyToPresentPopup();
    }).get());

    popupWebView(LoadOnFirstAccess::Yes);
}

void WebExtensionAction::readyToPresentPopup()
{
    ASSERT(canProgrammaticallyPresentPopup());

    if (m_popupPresented || !m_popupWebView)
        return;

    setHasUnreadBadgeText(false);

    m_popupPresented = true;

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        if (!extensionContext())
            return;

        RefPtr extensionController = extensionContext()->extensionController();
        auto delegate = extensionController ? extensionController->delegate() : nil;

        if (!delegate || ![delegate respondsToSelector:@selector(webExtensionController:presentPopupForAction:forExtensionContext:completionHandler:)]) {
            closePopupWebView();
            return;
        }

        [delegate webExtensionController:extensionController->wrapper() presentPopupForAction:wrapper() forExtensionContext:extensionContext()->wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }](NSError *error) {
            if (error)
                closePopupWebView();
        }).get()];
    }).get());
}

void WebExtensionAction::popupSizeDidChange()
{
#if PLATFORM(IOS_FAMILY)
    [m_popupViewController _updatePopoverContentSize];
#endif

    [NSNotificationCenter.defaultCenter postNotificationName:_WKWebExtensionActionPopupWebViewContentSizeDidChangeNotification object:wrapper() userInfo:nil];
}

void WebExtensionAction::popupDidClose()
{
#if PLATFORM(IOS_FAMILY)
    m_popupViewController = nil;
#endif
    m_popupWebView = nil;
    m_popupPresented = false;
}

void WebExtensionAction::closePopupWebView()
{
    [m_popupWebView _close];
#if PLATFORM(IOS_FAMILY)
    [m_popupViewController dismissViewControllerAnimated:YES completion:nil];
#endif

    popupDidClose();
}

String WebExtensionAction::label(FallbackWhenEmpty fallback) const
{
    if (!extensionContext())
        return emptyString();

    if (!m_customLabel.isNull()) {
        if (!m_customLabel.isEmpty() || fallback == FallbackWhenEmpty::No)
            return m_customLabel;

        return extensionContext()->extension().displayName();
    }

    if (RefPtr fallback = fallbackAction())
        return fallback->label();

    // Default
    if (auto *defaultLabel = extensionContext()->extension().displayActionLabel(); defaultLabel.length || fallback == FallbackWhenEmpty::No)
        return defaultLabel;

    return extensionContext()->extension().displayName();
}

void WebExtensionAction::setLabel(String label)
{
    if (m_customLabel == label)
        return;

    m_customLabel = label;

    propertiesDidChange();
}

String WebExtensionAction::badgeText() const
{
    if (!extensionContext())
        return emptyString();

    if (!m_customBadgeText.isNull())
        return m_customBadgeText;

    if (m_blockedResourceCount)
        return String::number(m_blockedResourceCount);

    if (RefPtr fallback = fallbackAction())
        return fallback->badgeText();

    // Default
    return emptyString();
}

void WebExtensionAction::setBadgeText(String badgeText)
{
    if (m_customBadgeText == badgeText)
        return;

    m_customBadgeText = badgeText;

    if (!badgeText.isNull())
        m_hasUnreadBadgeText = !badgeText.isEmpty();
    else
        m_hasUnreadBadgeText = std::nullopt;

    propertiesDidChange();
}

bool WebExtensionAction::hasUnreadBadgeText() const
{
    if (!extensionContext())
        return false;

    if (m_hasUnreadBadgeText)
        return m_hasUnreadBadgeText.value();

    if (RefPtr fallback = fallbackAction())
        return fallback->hasUnreadBadgeText();

    // Default
    return false;
}

void WebExtensionAction::setHasUnreadBadgeText(bool hasUnreadBadgeText)
{
    auto newValue = !badgeText().isEmpty() ? std::optional(hasUnreadBadgeText) : std::nullopt;
    if (m_hasUnreadBadgeText == newValue)
        return;

    m_hasUnreadBadgeText = newValue;

    // Only propagate the change if we're setting it to false.
    if (RefPtr fallback = fallbackAction(); fallback && !hasUnreadBadgeText)
        fallback->setHasUnreadBadgeText(false);

    propertiesDidChange();
}

void WebExtensionAction::incrementBlockedResourceCount(ssize_t amount)
{
    if (!amount)
        return;

    m_blockedResourceCount += amount;

    if (m_blockedResourceCount < 0)
        m_blockedResourceCount = 0;

    if (!badgeText().isEmpty())
        m_hasUnreadBadgeText = true;

    propertiesDidChange();
}

bool WebExtensionAction::isEnabled() const
{
    if (!extensionContext())
        return false;

    if (m_customEnabled)
        return m_customEnabled.value();

    if (RefPtr fallback = fallbackAction())
        return fallback->isEnabled();

    // Default
    return true;
}

void WebExtensionAction::setEnabled(std::optional<bool> enabled)
{
    if (m_customEnabled == enabled)
        return;

    m_customEnabled = enabled;

    propertiesDidChange();
}

NSArray *WebExtensionAction::platformMenuItems() const
{
    if (!extensionContext())
        return @[ ];

    WebExtensionMenuItemContextParameters contextParameters;
    contextParameters.types = WebExtensionMenuItemContextType::Action;
    contextParameters.tabIdentifier = m_tab ? std::optional { m_tab->identifier() } : std::nullopt;

    return WebExtensionMenuItem::matchingPlatformMenuItems(extensionContext()->mainMenuItems(), contextParameters, webExtensionActionMenuItemTopLevelLimit);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
