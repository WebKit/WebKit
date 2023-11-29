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
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import "_WKWebExtensionActionInternal.h"
#import "_WKWebExtensionControllerDelegatePrivate.h"
#import <wtf/BlockPtr.h>

#if USE(APPKIT)
constexpr CGFloat popoverMaximumWidth = 800;
constexpr CGFloat popoverMaximumHeight = 600;
#endif

constexpr CGFloat popoverMinimumWidth = 50;
constexpr CGFloat popoverMinimumHeight = 50;
constexpr NSTimeInterval popoverShowTimeout = 1;

@interface _WKWebExtensionActionWebViewDelegate : NSObject <WKNavigationDelegatePrivate, WKUIDelegatePrivate>
@end

@implementation _WKWebExtensionActionWebViewDelegate {
    WeakPtr<WebKit::WebExtensionAction> _webExtensionAction;
}

- (instancetype)initWithWebExtensionAction:(WebKit::WebExtensionAction&)action
{
    if (!(self = [super init]))
        return nil;

    _webExtensionAction = action;

    return self;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    NSURL *targetURL = navigationAction.request.URL;

    if (!navigationAction.targetFrame) {
        // FIXME: Handle new tab/window navigation.
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }

    // Allow subframes to load any URL.
    if (!navigationAction.targetFrame.isMainFrame) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    ASSERT(navigationAction.targetFrame.mainFrame);

    // Require an extension URL for the main frame.
    if (!_webExtensionAction->extensionContext()->isURLForThisExtension(targetURL)) {
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }

    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    _webExtensionAction->popupDidClose();
}

- (void)webViewDidClose:(WKWebView *)webView
{
    _webExtensionAction->popupDidClose();
}

@end

@interface _WKWebExtensionActionWebView : WKWebView
@end

@implementation _WKWebExtensionActionWebView {
    WeakPtr<WebKit::WebExtensionAction> _webExtensionAction;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration webExtensionAction:(WebKit::WebExtensionAction&)action
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    _webExtensionAction = action;

    return self;
}

- (void)invalidateIntrinsicContentSize
{
    auto intrinsicContentSize = self.intrinsicContentSize;

    [super invalidateIntrinsicContentSize];

    _webExtensionAction->popupSizeDidChange();

    if (intrinsicContentSize.width >= popoverMinimumWidth && intrinsicContentSize.height >= popoverMinimumHeight && !self.loading)
        _webExtensionAction->readyToPresentPopup();
}

@end

namespace WebKit {

WebExtensionAction::WebExtensionAction(WebExtensionContext& extensionContext)
    : m_extensionContext(extensionContext)
{
    auto delegate = extensionContext.extensionController()->delegate();
    m_respondsToPresentPopup = [delegate respondsToSelector:@selector(webExtensionController:presentPopupForAction:forExtensionContext:completionHandler:)];

    if (!m_respondsToPresentPopup)
        RELEASE_LOG_ERROR(Extensions, "%{public}@ does not implement the webExtensionController:presentPopupForAction:forExtensionContext:completionHandler: method", delegate.debugDescription);
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
    if (!m_customIcons && !m_customPopupPath.isNull() && !m_customLabel.isNull() && !m_customBadgeText.isNull() && !m_customEnabled)
        return;

    m_customIcons = nil;
    m_customPopupPath = nullString();
    m_customLabel = nullString();
    m_customBadgeText = nullString();
    m_customEnabled = std::nullopt;

    propertiesDidChange();
}

void WebExtensionAction::propertiesDidChange()
{
    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        [NSNotificationCenter.defaultCenter postNotificationName:_WKWebExtensionActionPropertiesDidChangeNotification object:wrapper() userInfo:nil];
    }).get());
}

CocoaImage *WebExtensionAction::icon(CGSize idealSize)
{
    if (!extensionContext())
        return nil;

    if (m_customIcons)
        return extensionContext()->extension().bestImageInIconsDictionary(m_customIcons.get(), idealSize.width > idealSize.height ? idealSize.width : idealSize.height);

    if (m_tab)
        return extensionContext()->getAction(m_tab->window().get())->icon(idealSize);

    if (m_window)
        return extensionContext()->defaultAction().icon(idealSize);

    return extensionContext()->extension().actionIcon(idealSize);
}

void WebExtensionAction::setIconsDictionary(NSDictionary *icons)
{
    m_customIcons = icons.count ? icons : nil;

    propertiesDidChange();
}

String WebExtensionAction::popupPath() const
{
    if (!extensionContext())
        return emptyString();

    if (!m_customPopupPath.isNull())
        return m_customPopupPath;

    if (m_tab)
        return extensionContext()->getAction(m_tab->window().get())->popupPath();

    if (m_window)
        return extensionContext()->defaultAction().popupPath();

    return extensionContext()->extension().actionPopupPath();
}

void WebExtensionAction::setPopupPath(String path)
{
    if (m_customPopupPath == path)
        return;

    m_customPopupPath = path;

    propertiesDidChange();
}

WKWebView *WebExtensionAction::popupWebView(LoadOnFirstAccess loadOnFirstAccess)
{
    if (!presentsPopup())
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

#if USE(APPKIT)
    m_popupWebView.get().accessibilityRole = NSAccessibilityPopoverRole;

    m_popupWebView.get()._sizeToContentAutoSizeMaximumSize = CGSizeMake(popoverMaximumWidth, popoverMaximumHeight);
    m_popupWebView.get()._useSystemAppearance = YES;

    [m_popupWebView setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationHorizontal];
    [m_popupWebView setContentHuggingPriority:NSLayoutPriorityDefaultHigh forOrientation:NSLayoutConstraintOrientationVertical];

    [NSLayoutConstraint activateConstraints:@[
        [m_popupWebView.get().widthAnchor constraintGreaterThanOrEqualToConstant:popoverMinimumWidth],
        [m_popupWebView.get().widthAnchor constraintLessThanOrEqualToConstant:popoverMaximumWidth],
        [m_popupWebView.get().heightAnchor constraintGreaterThanOrEqualToConstant:popoverMinimumHeight],
        [m_popupWebView.get().heightAnchor constraintLessThanOrEqualToConstant:popoverMaximumHeight]
    ]];
#endif // USE(APPKIT)

    auto popupPage = m_popupWebView.get()._page;
    auto tabIdentifier = m_tab ? std::optional(m_tab->identifier()) : std::nullopt;
    auto windowIdentifier = m_window ? std::optional(m_window->identifier()) : std::nullopt;
    popupPage->process().send(Messages::WebExtensionContextProxy::AddPopupPageIdentifier(popupPage->webPageID(), tabIdentifier, windowIdentifier), extensionContext()->identifier());

    auto url = URL { extensionContext()->baseURL(), popupPath() };
    [m_popupWebView loadRequest:[NSURLRequest requestWithURL:url]];

    return m_popupWebView.get();
}

void WebExtensionAction::presentPopupWhenReady()
{
    if (!extensionContext() || !m_respondsToPresentPopup)
        return;

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
    if (m_popupPresented || !m_respondsToPresentPopup)
        return;

    m_popupPresented = true;

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        auto* extensionController = extensionContext()->extensionController();
        auto delegate = extensionController->delegate();

        [delegate webExtensionController:extensionController->wrapper() presentPopupForAction:wrapper() forExtensionContext:extensionContext()->wrapper() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }](NSError *error) {
            if (error)
                closePopupWebView();
        }).get()];
    }).get());
}

void WebExtensionAction::popupSizeDidChange()
{
    [NSNotificationCenter.defaultCenter postNotificationName:_WKWebExtensionActionPopupWebViewContentSizeDidChangeNotification object:wrapper() userInfo:nil];
}

void WebExtensionAction::popupDidClose()
{
    m_popupWebView = nil;
    m_popupPresented = false;
}

void WebExtensionAction::closePopupWebView()
{
    [m_popupWebView _close];
    m_popupWebView = nil;
    m_popupPresented = false;
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

    if (m_tab)
        return extensionContext()->getAction(m_tab->window().get())->label();

    if (m_window)
        return extensionContext()->defaultAction().label();

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

    if (m_tab)
        return extensionContext()->getAction(m_tab->window().get())->badgeText();

    if (m_window)
        return extensionContext()->defaultAction().badgeText();

    return emptyString();
}

void WebExtensionAction::setBadgeText(String badgeText)
{
    if (m_customBadgeText == badgeText)
        return;

    m_customBadgeText = badgeText;

    propertiesDidChange();
}

bool WebExtensionAction::isEnabled() const
{
    if (!extensionContext())
        return false;

    if (m_customEnabled)
        return m_customEnabled.value();

    if (m_tab)
        return extensionContext()->getAction(m_tab->window().get())->isEnabled();

    if (m_window)
        return extensionContext()->defaultAction().isEnabled();

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
