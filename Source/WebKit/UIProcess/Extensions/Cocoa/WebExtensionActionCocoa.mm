/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "WKNSError.h"
#import "WKNavigationActionPrivate.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebExtensionActionInternal.h"
#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import "WKWindowFeaturesPrivate.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionMenuItem.h"
#import "WebExtensionMenuItemContextParameters.h"
#import "WebExtensionMenuItemParameters.h"
#import "WebExtensionTabParameters.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

#if PLATFORM(MAC)
#import "AppKitSPI.h"
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
constexpr auto popoverShowTimeout = 250_ms;
constexpr auto popoverStableSizeDuration = 75_ms;
#else
// Debug builds are about 3x slower, so give rendering more time.
constexpr auto popoverShowTimeout = 750_ms;
constexpr auto popoverStableSizeDuration = 225_ms;
#endif

constexpr auto updateThrottleDuration = 15_ms;

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

        _webExtensionAction->extensionContext()->openNewTab(tabParameters, [](auto newTab) {
            // Nothing to do.
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

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    // This can't return a web view because it must use the supplied configuration,
    // or an exception will be thrown. Web extension URLs can't load in a web view
    // configured for HTTP-family URLs, and vice versa, making it imposible to
    // use the configuration for a new tab.

    if (!_webExtensionAction || !_webExtensionAction->extensionContext())
        return nil;

    WebExtensionTabParameters tabParameters;
    tabParameters.url = navigationAction.request.URL;

    if (_webExtensionAction->extensionContext()->canOpenNewWindow()) {
        WebExtensionWindowParameters windowParameters;
        windowParameters.focused = true;
        windowParameters.tabs = { WTFMove(tabParameters) };
        windowParameters.type = windowFeatures._wantsPopup ? WebExtensionWindow::Type::Popup : WebExtensionWindow::Type::Normal;
        windowParameters.state = windowFeatures._fullscreenDisplay.boolValue ? WebExtensionWindow::State::Fullscreen : WebExtensionWindow::State::Normal;

        static constexpr CGFloat NaN = std::numeric_limits<CGFloat>::quiet_NaN();

        CGRect frame;
        frame.origin.x = windowFeatures.x ? windowFeatures.x.doubleValue : NaN;
        frame.origin.y = windowFeatures.y ? windowFeatures.y.doubleValue : NaN;
        frame.size.width = windowFeatures.width ? windowFeatures.width.doubleValue : NaN;
        frame.size.height = windowFeatures.height ? windowFeatures.height.doubleValue : NaN;

        windowParameters.frame = frame;

        _webExtensionAction->extensionContext()->openNewWindow(windowParameters, [](auto newWindow) {
            // Nothing to do.
        });

        return nil;
    }

    RefPtr currentWindow = _webExtensionAction->window();
    RefPtr currentTab = _webExtensionAction->tab();
    if (!currentWindow && currentTab)
        currentWindow = currentTab->window();

    tabParameters.windowIdentifier = currentWindow ? currentWindow->identifier() : WebExtensionWindowConstants::CurrentIdentifier;
    tabParameters.index = currentTab ? std::optional(currentTab->index() + 1) : std::nullopt;
    tabParameters.active = true;

    _webExtensionAction->extensionContext()->openNewTab(tabParameters, [](auto newTab) {
        // Nothing to do.
    });

    return nil;
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (!_webExtensionAction)
        return;

    _webExtensionAction->closePopup();
}

- (void)webViewDidClose:(WKWebView *)webView
{
    if (!_webExtensionAction)
        return;

    _webExtensionAction->closePopup();
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (!_webExtensionAction)
        return;

    RELEASE_LOG_ERROR(Extensions, "Popup provisional load failed: %{public}@", privacyPreservingDescription(error));

    _webExtensionAction->closePopup();
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (!_webExtensionAction)
        return;

    RELEASE_LOG_ERROR(Extensions, "Popup load failed: %{public}@", privacyPreservingDescription(error));

    _webExtensionAction->closePopup();
}

- (void)_webView:(WKWebView *)webView navigationDidFinishDocumentLoad:(WKNavigation *)navigation
{
    if (!_webExtensionAction)
        return;

    _webExtensionAction->popupDidFinishDocumentLoad();
}

#if PLATFORM(MAC)
- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *URLs))completionHandler
{
    RefPtr extensionContext = _webExtensionAction ? _webExtensionAction->extensionContext() : nullptr;
    if (!extensionContext) {
        completionHandler(nil);
        return;
    }

    extensionContext->runOpenPanel(webView, parameters, completionHandler);
}
#endif // PLATFORM(MAC)

@end

#if PLATFORM(IOS_FAMILY)
static void* kvoContext = &kvoContext;
#endif

@interface _WKWebExtensionActionWebView : WKWebView

@property (nonatomic, readonly) CGSize contentSize;
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

- (CGSize)contentSize
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

    auto contentSize = self.contentSize;
    if (CGSizeEqualToSize(contentSize, _previousContentSize))
        return;

    SEL contentSizeStabilizedSelector = @selector(_contentSizeHasStabilized);
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:contentSizeStabilizedSelector object:nil];
    [self performSelector:contentSizeStabilizedSelector withObject:nil afterDelay:popoverStableSizeDuration.seconds()];

    _previousContentSize = contentSize;
}

- (void)_contentSizeHasStabilized
{
    if (!_webExtensionAction || self.loading)
        return;

    auto contentSize = self.contentSize;
    if (contentSize.width < minimumPopoverWidth || contentSize.height < minimumPopoverHeight)
        return;

    _contentSizeHasStabilized = YES;

    _webExtensionAction->popupSizeDidChange();
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

    _webExtensionAction->closePopup();
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
        CGSize contentSize = _popupWebView.scrollView.contentSize;
        CGSize desiredSize = CGSizeMake(std::min(contentSize.width, maximumPopoverWidth), std::min(contentSize.height, maximumPopoverHeight));

        CGFloat minimumWidth = desiredSize.width;
        CGFloat minimumHeight = _popupWebView.contentSizeHasStabilized ? std::min(desiredSize.height, minimumPopoverHeight) : minimumPopoverHeight;
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

#if PLATFORM(MAC)
@interface _WKWebExtensionActionPopover : NSPopover
@end

@implementation _WKWebExtensionActionPopover {
    WeakPtr<WebExtensionAction> _webExtensionAction;
    _WKWebExtensionActionWebView *_popupWebView;
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

    _popupWebView.accessibilityRole = NSAccessibilityPopoverRole;

    [_popupWebView setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
    [_popupWebView setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationVertical];

    [NSLayoutConstraint activateConstraints:@[
        [_popupWebView.widthAnchor constraintGreaterThanOrEqualToConstant:minimumPopoverWidth],
        [_popupWebView.widthAnchor constraintLessThanOrEqualToConstant:maximumPopoverWidth],
        [_popupWebView.heightAnchor constraintGreaterThanOrEqualToConstant:minimumPopoverHeight],
        [_popupWebView.heightAnchor constraintLessThanOrEqualToConstant:maximumPopoverHeight]
    ]];

    auto *viewController = [[NSViewController alloc] init];
    viewController.view = _popupWebView;
    viewController.preferredContentSize = _popupWebView.contentSize;

    self.contentViewController = viewController;
    self.behavior = NSPopoverBehaviorSemitransient;

    action.setPopupPopoverAppearance(action.popupPopoverAppearance());

    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(_otherPopoverWillShow:) name:NSPopoverWillShowNotification object:nil];

    return self;
}

- (void)popoverWillClose:(NSNotification *)notification
{
    ASSERT([self isEqual:notification.object]);

    auto *parentWindow = self.positioningView.window;
    [parentWindow makeFirstResponder:parentWindow.windowController];
}

- (void)popoverDidClose:(NSNotification *)notification
{
    ASSERT([self isEqual:notification.object]);

    if (!_webExtensionAction)
        return;

    _webExtensionAction->closePopup();
}

- (void)_otherPopoverWillShow:(NSNotification *)notification
{
    if (!self.shown)
        return;

    // Close if another popover is about to show in the same window.
    auto *incomingPopover = dynamic_objc_cast<NSPopover>(notification.object);
    auto *incomingPopoverWindow = incomingPopover.positioningView.window;
    if (![incomingPopoverWindow isEqual:self.positioningView.window])
        return;

    [self close];
}

@end
#endif // PLATFORM(MAC)

namespace WebKit {

WebExtensionAction::WebExtensionAction(WebExtensionContext& extensionContext)
    : m_extensionContext(extensionContext)
{
}

WebExtensionAction::WebExtensionAction(WebExtensionContext& extensionContext, WebExtensionTab& tab)
    : m_extensionContext(extensionContext)
    , m_tab(&tab)
{
}

WebExtensionAction::WebExtensionAction(WebExtensionContext& extensionContext, WebExtensionWindow& window)
    : m_extensionContext(extensionContext)
    , m_window(&window)
{
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
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (!m_customIcons && !m_customIconVariants && m_customPopupPath.isNull() && m_customLabel.isNull() && m_customBadgeText.isNull() && !m_customEnabled && !m_blockedResourceCount)
#else
    if (!m_customIcons && m_customPopupPath.isNull() && m_customLabel.isNull() && m_customBadgeText.isNull() && !m_customEnabled && !m_blockedResourceCount)
#endif
        return;

    m_customIcons = nullptr;
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    m_customIconVariants = nullptr;
#endif
    m_customPopupPath = nullString();
    m_customLabel = nullString();
    m_customBadgeText = nullString();
    m_customEnabled = std::nullopt;
    m_blockedResourceCount = 0;

#if PLATFORM(MAC)
    // Reset the popover appearance until the color-scheme CSS can be checked on the next load.
    m_popoverAppearance = Appearance::Default;
#endif

    clearIconCache();
    propertiesDidChange();
}

void WebExtensionAction::clearBlockedResourceCount()
{
    m_blockedResourceCount = 0;

    propertiesDidChange();
}

void WebExtensionAction::propertiesDidChange()
{
    if (m_updatePending)
        return;

    m_updatePending = true;

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, updateThrottleDuration.nanosecondsAs<int64_t>()), dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        m_updatePending = false;

        RefPtr extensionContext = m_extensionContext.get();
        if (!extensionContext)
            return;

        RefPtr extensionController = extensionContext->extensionController();
        if (!extensionController)
            return;

        auto *delegate = extensionController->delegate();
        if (![delegate respondsToSelector:@selector(webExtensionController:didUpdateAction:forExtensionContext:)])
            return;

        if (isTabAction()) {
            // Tab actions are not inherited by other actions, so just call the delegate directly.
            [delegate webExtensionController:extensionController->wrapper() didUpdateAction:wrapper() forExtensionContext:extensionContext->wrapper()];
            return;
        }

        auto callDelegateForTabs = [=](auto&& tabs) {
            for (Ref tab : tabs) {
                Ref tabAction = extensionContext->getAction(tab.ptr());

                // Skip tabs with no custom action (i.e., default or window action).
                if (!tabAction->isTabAction())
                    continue;

                [delegate webExtensionController:extensionController->wrapper() didUpdateAction:tabAction->wrapper() forExtensionContext:extensionContext->wrapper()];
            }
        };

        if (isWindowAction()) {
            // Call the delegate about tab-specific actions that inherit from the window.
            RefPtr window = protectedThis->window();
            callDelegateForTabs(window->tabs());
            return;
        }

        ASSERT(isDefaultAction());

        // Call the delegate about the default action, since it is retrievable via the API.
        [delegate webExtensionController:extensionController->wrapper() didUpdateAction:wrapper() forExtensionContext:extensionContext->wrapper()];

        // Call the delegate about tab-specific actions inheriting from the default.
        callDelegateForTabs(extensionContext->openTabs());
    }).get());
}

WebExtensionAction* WebExtensionAction::fallbackAction() const
{
    if (!extensionContext())
        return nullptr;

    // Tab actions whose tab references have not dropped fallback to the window action.
    if (RefPtr tab = this->tab())
        return extensionContext()->getAction(tab->window().get()).ptr();

    // Window actions and tab actions whose tab references have dropped fallback to the default action.
    if (m_window.has_value() || m_tab.has_value())
        return &extensionContext()->defaultAction();

    // Default actions have no fallback.
    return nullptr;
}

RefPtr<WebCore::Icon> WebExtensionAction::icon(WebCore::FloatSize idealSize)
{
    if (!extensionContext())
        return nil;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    if (m_customIconVariants || m_customIcons)
#else
    if (m_customIcons)
#endif
    {
        // Clear the cache if the display scales change (connecting display, etc.)
        auto currentScales = availableScreenScales();
        if (currentScales != m_cachedIconScales)
            clearIconCache();

        if (m_cachedIcon && idealSize == m_cachedIconIdealSize)
            return m_cachedIcon;

        RefPtr<WebCore::Icon> result;

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (m_customIconVariants) {
            result = extensionContext()->extension().bestIconVariant(m_customIconVariants, WebCore::FloatSize(idealSize), [&](Ref<API::Error> error) {
                extensionContext()->recordError(::WebKit::wrapper(error));
            });
        } else
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
        if (m_customIcons) {
            result = extensionContext()->extension().bestIcon(m_customIcons, WebCore::FloatSize(idealSize), [&](Ref<API::Error> error) {
                extensionContext()->recordError(::WebKit::wrapper(error));
            });
        }

        if (result) {
            m_cachedIcon = result;
            m_cachedIconScales = currentScales;
            m_cachedIconIdealSize = idealSize;

            return result;
        }

        clearIconCache();

        // If custom icons fail, fallback.
    }

    if (RefPtr fallback = fallbackAction())
        return fallback->icon(idealSize);

    // Default
    return extensionContext()->extension().actionIcon(WebCore::FloatSize(idealSize));
}

void WebExtensionAction::setIcons(RefPtr<JSON::Object> icons)
{
    if (m_customIcons == icons)
        return;

    m_customIcons = icons->size() ? icons : nullptr;
#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
    m_customIconVariants = nullptr;
#endif

    clearIconCache();
    propertiesDidChange();
}

#if ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)
void WebExtensionAction::setIconVariants(RefPtr<JSON::Array> iconVariants)
{
    if (m_customIconVariants == iconVariants)
        return;

    m_customIconVariants = iconVariants->length() ? iconVariants : nullptr;
    m_customIcons = nullptr;

    clearIconCache();
    propertiesDidChange();
}
#endif // ENABLE(WK_WEB_EXTENSIONS_ICON_VARIANTS)

void WebExtensionAction::clearIconCache()
{
    m_cachedIcon = nil;
    m_cachedIconScales = { };
    m_cachedIconIdealSize = { };
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

#if PLATFORM(MAC)
    // Reset the popover appearance until the color-scheme CSS can be checked on the next load.
    m_popoverAppearance = Appearance::Default;
#endif

    propertiesDidChange();
}

NSString *WebExtensionAction::popupWebViewInspectionName()
{
    if (m_popupWebViewInspectionName.isEmpty())
        m_popupWebViewInspectionName = WEB_UI_FORMAT_CFSTRING("%@ — Extension Popup Page", "Label for an inspectable Web Extension popup page", extensionContext()->extension().displayShortName().createCFString().get());

    return m_popupWebViewInspectionName;
}

void WebExtensionAction::setPopupWebViewInspectionName(const String& name)
{
    m_popupWebViewInspectionName = name;
    m_popupWebView.get()._remoteInspectionNameOverride = name;
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
#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)
NSPopover *WebExtensionAction::popupPopover()
{
    if (!presentsPopup())
        return nil;

    if (m_popupPopover)
        return m_popupPopover.get();

    m_popupPopover = [[_WKWebExtensionActionPopover alloc] initWithWebExtensionAction:*this];

    return m_popupPopover.get();
}

void WebExtensionAction::setPopupPopoverAppearance(Appearance appearance)
{
    m_popoverAppearance = appearance;

    if (!m_popupPopover)
        return;

    // Set the popover appearance to light when it is unknown. Dark mode support is checked in detectPopoverColorScheme().
    // This maintains the best compatibility for extensions that only support light mode.

    switch (appearance) {
    case Appearance::Default:
    case Appearance::Light:
        m_popupPopover.get().appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
        break;

    case Appearance::Dark:
        m_popupPopover.get().appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        break;

    case Appearance::Both:
        m_popupPopover.get().appearance = nil;
        break;
    }
}

void WebExtensionAction::detectPopoverColorScheme()
{
    if (!m_popupPopover || !m_popupPopover.get().shown)
        return;

    static NSString * const checkColorSchemeScript = @"(function() {"
    "  let computedStyle = window.getComputedStyle(document.body).colorScheme;"
    "  if (computedStyle && computedStyle !== 'normal' && computedStyle !== 'auto') {"
    "      let styleParts = computedStyle.split(/\\s+/);"
    "      if (styleParts?.length)"
    "        return styleParts;"
    "  }"

    "  let metaElements = document.querySelectorAll(`meta[name='color-scheme']`);"
    "  if (!metaElements.length) metaElements = document.querySelectorAll(`meta[name='supported-color-schemes']`);"

    "  let lastMetaElement = metaElements?.[metaElements.length - 1];"
    "  if (lastMetaElement?.content) {"
    "    let contentParts = lastMetaElement.content.toLowerCase().split(/\\s+/);"
    "    if (contentParts?.length)"
    "      return contentParts;"
    "  }"

    "  return null;"
    "})()";

    [m_popupWebView evaluateJavaScript:checkColorSchemeScript completionHandler:^(id result, NSError *error) {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error while checking popup color scheme: %{public}@", privacyPreservingDescription(error));
            return;
        }

        bool forceLightPopoverAppearance = false;
        bool forceDarkPopoverAppearance = false;

        if (auto *colorSchemesArray = dynamic_objc_cast<NSArray>(result)) {
            auto *supportedColorSchemes = [NSSet setWithArray:colorSchemesArray];
            if (([supportedColorSchemes containsObject:@"light"] || [supportedColorSchemes containsObject:@"only"]) && ![supportedColorSchemes containsObject:@"dark"])
                forceLightPopoverAppearance = true;
            else if ([supportedColorSchemes containsObject:@"dark"] && ![supportedColorSchemes containsObject:@"light"])
                forceDarkPopoverAppearance = true;
        } else
            forceLightPopoverAppearance = true;

        if (forceLightPopoverAppearance)
            setPopupPopoverAppearance(Appearance::Light);
        else if (forceDarkPopoverAppearance)
            setPopupPopoverAppearance(Appearance::Dark);
        else
            setPopupPopoverAppearance(Appearance::Both);
    }];
}
#endif // PLATFORM(MAC)

WKWebView *WebExtensionAction::popupWebView()
{
    if (!presentsPopup() || !extensionContext())
        return nil;

    if (m_popupWebView)
        return m_popupWebView.get();

    auto *webViewConfiguration = extensionContext()->webViewConfiguration(WebExtensionContext::WebViewPurpose::Popup);
    webViewConfiguration.suppressesIncrementalRendering = YES;

    m_popupWebViewDelegate = [[_WKWebExtensionActionWebViewDelegate alloc] initWithWebExtensionAction:*this];

    m_popupWebView = [[_WKWebExtensionActionWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration webExtensionAction:*this];
    m_popupWebView.get().navigationDelegate = m_popupWebViewDelegate.get();
    m_popupWebView.get().UIDelegate = m_popupWebViewDelegate.get();
    m_popupWebView.get().inspectable = extensionContext()->isInspectable();
    m_popupWebView.get().accessibilityLabel = extensionContext()->extension().displayName();
    m_popupWebView.get()._remoteInspectionNameOverride = popupWebViewInspectionName();

#if PLATFORM(MAC)
    m_popupWebView.get()._sizeToContentAutoSizeMaximumSize = CGSizeMake(maximumPopoverWidth, maximumPopoverHeight);
    m_popupWebView.get()._useSystemAppearance = YES;

    // Add the web view temporarily to a window to force it to layout. The window does not need to be shown on screen.
    auto *temporaryWindow = [[NSWindow alloc] initWithContentRect:NSZeroRect styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    [temporaryWindow.contentView addSubview:m_popupWebView.get()];
    [m_popupWebView removeFromSuperview];
    temporaryWindow = nil;
#endif

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

    // The popup might have presented already or is already scheduled to present when ready.
    if (popupPresented() || presentsPopupWhenReady())
        return;

    if (!canProgrammaticallyPresentPopup()) {
        RELEASE_LOG_ERROR(Extensions, "Delegate does not implement the webExtensionController:presentPopupForAction:forExtensionContext:completionHandler: method");
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Present popup when ready");

    m_presentsPopupWhenReady = true;

    // Access the web view to load it.
    popupWebView();
}

void WebExtensionAction::popupDidFinishDocumentLoad()
{
    if (!extensionContext())
        return;

    // The popup might have presented or closed already.
    if (popupPresented() || !hasPopupWebView() || !presentsPopupWhenReady())
        return;

    // Delay showing the popup until a minimum size or a timeout is reached.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, popoverShowTimeout.nanosecondsAs<int64_t>()), dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }] {
        if (popupPresented() || !hasPopupWebView() || !presentsPopupWhenReady() || !extensionContext())
            return;

        RELEASE_LOG_DEBUG(Extensions, "Presenting popup after %{public}.0fms timeout", popoverShowTimeout.milliseconds());

        readyToPresentPopup();
    }).get());
}

void WebExtensionAction::readyToPresentPopup()
{
    ASSERT(presentsPopupWhenReady());
    ASSERT(canProgrammaticallyPresentPopup());

    m_presentsPopupWhenReady = false;

    if (!extensionContext())
        return;

    // The popup might have presented or closed already.
    if (popupPresented() || !hasPopupWebView())
        return;

    setHasUnreadBadgeText(false);

    m_popupPresented = true;
    if (RefPtr extensionController = extensionContext()->extensionController())
        extensionController->setShowingActionPopup(true);

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        if (!extensionContext() || !popupPresented())
            return;

        ASSERT(hasPopupWebView());

        RefPtr extensionController = extensionContext()->extensionController();
        auto delegate = extensionController ? extensionController->delegate() : nil;

        if (!delegate || ![delegate respondsToSelector:@selector(webExtensionController:presentPopupForAction:forExtensionContext:completionHandler:)]) {
            closePopup();
            return;
        }

        [delegate webExtensionController:extensionController->wrapper() presentPopupForAction:wrapper() forExtensionContext:extensionContext()->wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }](NSError *error) {
            if (error) {
                closePopup();
                return;
            }

#if PLATFORM(MAC)
            // Perform the color scheme check after showing the popover, since this can cause a layout,
            // which can cause the popover content to misrender the first time.
            detectPopoverColorScheme();
#endif
        }).get()];
    }).get());
}

void WebExtensionAction::popupSizeDidChange()
{
    ASSERT(hasPopupWebView());

#if PLATFORM(IOS_FAMILY)
    [m_popupViewController _updatePopoverContentSize];
#endif

#if PLATFORM(MAC)
    m_popupPopover.get().contentViewController.preferredContentSize = m_popupWebView.get().contentSize;
#endif

    if (!presentsPopupWhenReady())
        return;

    auto contentSize = m_popupWebView.get().contentSize;
    RELEASE_LOG_DEBUG(Extensions, "Presenting popup with size { %{public}.0f, %{public}.0f }", contentSize.width, contentSize.height);

    readyToPresentPopup();
}

void WebExtensionAction::closePopup()
{
    if (!popupPresented())
        return;

    ASSERT(hasPopupWebView());

    RELEASE_LOG_DEBUG(Extensions, "Popup closed");

    m_popupPresented = false;
    m_presentsPopupWhenReady = false;

    if (RefPtr extensionController = extensionContext()->extensionController())
        extensionController->setShowingActionPopup(false);

    [m_popupWebView _close];
    m_popupWebView = nil;

#if PLATFORM(IOS_FAMILY)
    [m_popupViewController dismissViewControllerAnimated:YES completion:nil];
    m_popupViewController = nil;
#endif

#if PLATFORM(MAC)
    [m_popupPopover close];
    m_popupPopover = nil;
#endif
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
    if (auto defaultLabel = extensionContext()->extension().displayActionLabel(); !defaultLabel.isEmpty() || fallback == FallbackWhenEmpty::No)
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

    RefPtr tab = this->tab();
    if (!tab) {
        if (RefPtr window = this->window())
            tab = window->activeTab();
    }

    WebExtensionMenuItemContextParameters contextParameters;
    contextParameters.types = WebExtensionMenuItemContextType::Action;
    contextParameters.tabIdentifier = tab ? std::optional(tab->identifier()) : std::nullopt;
    contextParameters.frameURL = tab ? tab->url() : URL { };

    return WebExtensionMenuItem::matchingPlatformMenuItems(extensionContext()->mainMenuItems(), contextParameters, webExtensionActionMenuItemTopLevelLimit);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
