/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
#import "WebExtensionSidebar.h"

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#import "CocoaHelpers.h"
#import "WKNavigationAction.h"
#import "WKNavigationDelegate.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKNavigationPrivate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebViewInternal.h"
#import "WebExtensionContext.h"
#import "WebExtensionTab.h"
#import "WebExtensionWindow.h"
#import "WebExtensionWindowIdentifier.h"
#import "_WKWebExtensionSidebar.h"

#import <wtf/BlockPtr.h>

@interface _WKWebExtensionSidebarWebViewDelegate : NSObject <WKNavigationDelegatePrivate>
@end

@implementation _WKWebExtensionSidebarWebViewDelegate {
    WeakPtr<WebKit::WebExtensionSidebar> _webExtensionSidebar;
}

- (instancetype)initWithWebExtensionSidebar:(WebKit::WebExtensionSidebar&)sidebar
{
    if (!(self = [super init]))
        return nil;

    _webExtensionSidebar = sidebar;

    return self;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    RefPtr currentSidebar = _webExtensionSidebar.get();
    RefPtr context = currentSidebar ? currentSidebar->extensionContext() : nullptr;
    if (!context) {
        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }
    NSURL *targetURL = navigationAction.request.URL;
    bool isURLForThisExtension = context->isURLForThisExtension(targetURL);

    if (!navigationAction.targetFrame || (navigationAction.targetFrame.isMainFrame && !isURLForThisExtension)) {
        std::optional currentWindow = currentSidebar->window();
        std::optional currentTab = currentSidebar->tab();
        if (!currentWindow && currentTab)
            currentWindow = toOptional(currentTab.value()->window());
        else if (!currentWindow && !currentTab)
            currentWindow = toOptional(context->frontmostWindow());

        WebKit::WebExtensionTabParameters tabParameters;
        tabParameters.url = targetURL;
        tabParameters.windowIdentifier = currentWindow
            .transform([](auto const& window) { return window->identifier(); })
            .value_or(WebKit::WebExtensionWindowConstants::CurrentIdentifier);
        tabParameters.index = currentTab
            .transform([](auto const& tab) { return tab->index() + 1; });
        tabParameters.active = true;

        context->openNewTab(tabParameters, [](auto const& newTab) { });

        decisionHandler(WKNavigationActionPolicyCancel);
        return;
    }

    if (!navigationAction.targetFrame.isMainFrame) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    decisionHandler(WKNavigationActionPolicyAllow);
}

#if PLATFORM(MAC)
- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *URLs))completionHandler
{
    RefPtr sidebar = _webExtensionSidebar.get();
    RefPtr extensionContext = sidebar ? sidebar->extensionContext() : nullptr;
    if (!extensionContext) {
        completionHandler(nil);
        return;
    }

    extensionContext->runOpenPanel(webView, parameters, completionHandler);
}
#endif // PLATFORM(MAC)

@end

using WebKit::WebExtensionSidebar;
using WebKit::WebExtensionContext;
using WTF::WeakPtr;

@interface _WKWebExtensionSidebarViewController : SidebarViewControllerType
@end

@implementation _WKWebExtensionSidebarViewController {
    WeakPtr<WebExtensionSidebar> _webExtensionSidebar;
    WKWebView *_sidebarWebView;
}

- (instancetype)initWithWebExtensionSidebar:(WebExtensionSidebar&)sidebar
{
    if (!(self = [super init]))
        return nil;

    RefPtr extensionContext = sidebar.extensionContext();
    if (!extensionContext)
        return nil;

    _webExtensionSidebar = sidebar;
    self.view = sidebar.webView();
    self.title = extensionContext->extension().displayName().createNSString().get();

    return self;
}

- (void)viewWillAppear
{
    [super viewWillAppear];

    if (RefPtr sidebar = _webExtensionSidebar.get())
        sidebar->sidebarWillAppear();
}

- (void)viewWillDisappear
{
    [super viewWillDisappear];

    if (RefPtr sidebar = _webExtensionSidebar.get())
        sidebar->sidebarWillDisappear();
}
@end

@interface _WKWebExtensionSidebarWebView : WKWebView
@end

@implementation _WKWebExtensionSidebarWebView {
    WeakPtr<WebExtensionSidebar> _webExtensionSidebar;
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *) configuration webExtensionSidebar:(WebExtensionSidebar&)sidebar
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    _webExtensionSidebar = sidebar;

    return self;
}

- (void)mouseDown:(NSEvent *)event
{
    if (RefPtr sidebar = _webExtensionSidebar.get())
        sidebar->didReceiveUserInteraction();

    [super mouseDown:event];
}

- (void)rightMouseDown:(NSEvent *)event
{
    if (RefPtr sidebar = _webExtensionSidebar.get())
        sidebar->didReceiveUserInteraction();

    [super rightMouseDown:event];
}

- (void)otherMouseDown:(NSEvent *)event
{
    if (RefPtr sidebar = _webExtensionSidebar.get())
        sidebar->didReceiveUserInteraction();

    [super otherMouseDown:event];
}

@end

namespace WebKit {

static NSString * const fallbackPath = @"about:blank";
static NSString * const fallbackTitle = @"";

static std::optional<Ref<JSON::Object>> getDefaultIconsDictFromExtension(WebExtension& extensions)
{
    // FIXME: <https://webkit.org/b/276833> implement this
    return std::nullopt;
}

WebExtensionSidebar::WebExtensionSidebar(WebExtensionContext& context, IsDefault isDefault) : WebExtensionSidebar(context, std::nullopt, std::nullopt, isDefault) { };

WebExtensionSidebar::WebExtensionSidebar(WebExtensionContext& context, WebExtensionTab& tab) : WebExtensionSidebar(context, tab, std::nullopt, IsDefault::No) { };

WebExtensionSidebar::WebExtensionSidebar(WebExtensionContext& context, WebExtensionWindow& window) : WebExtensionSidebar(context, std::nullopt, window, IsDefault::No) { };

WebExtensionSidebar::WebExtensionSidebar(WebExtensionContext& context, std::optional<Ref<WebExtensionTab>> tab, std::optional<Ref<WebExtensionWindow>> window, IsDefault isDefault)
    : m_extensionContext(context), m_tab(tab), m_window(window), m_isDefault(isDefault)
{
    ASSERT(!(m_tab && m_window));

    // if this is the default action, initialize with default sidebar path / title if present
    if (isDefaultSidebar()) {
        auto& extension = context.extension();
        m_titleOverride = extension.sidebarTitle();
        m_sidebarPathOverride = extension.sidebarDocumentPath();
        m_iconsOverride = getDefaultIconsDictFromExtension(extension);
        m_isEnabled = true;
    }

    std::optional<Ref<WebExtensionSidebar>> parent;
    if (RefPtr<WebExtensionWindow> parentWindow; tab && (parentWindow = tab.value()->window()))
        parent = context.getOrCreateSidebar(*parentWindow);
    else if (window)
        parent = context.defaultSidebar();

    if (parent)
        parent.value()->addChild(*this);
}

RefPtr<WebExtensionContext> WebExtensionSidebar::extensionContext() const
{
    return m_extensionContext.get();
}

const std::optional<Ref<WebExtensionTab>> WebExtensionSidebar::tab() const
{
    return m_tab.and_then([](WeakPtr<WebExtensionTab> const& maybeTabPtr) {
        return toOptional(RefPtr(maybeTabPtr.get()));
    });
}

const std::optional<Ref<WebExtensionWindow>> WebExtensionSidebar::window() const
{
    return m_window.and_then([](WeakPtr<WebExtensionWindow> const& maybeWindowPtr) {
        return toOptional(RefPtr(maybeWindowPtr.get()));
    });
}

std::optional<Ref<WebExtensionSidebar>> WebExtensionSidebar::parent() const
{
    RefPtr context = extensionContext();
    if (!context || isDefaultSidebar())
        return std::nullopt;

    return tab().and_then([&](Ref<WebExtensionTab> const& tab) -> std::optional<Ref<WebExtensionSidebar>> {
        RefPtr window = tab->window();
        return window ? context->getSidebar(*window) : std::nullopt;
    }).or_else([&] -> std::optional<Ref<WebExtensionSidebar>> {
        return Ref { context->defaultSidebar() };
    });
}

void WebExtensionSidebar::propertiesDidChange()
{
    if (isParentSidebar())
        notifyChildrenOfPropertyUpdate(ShouldReloadWebView::No);
    else
        notifyDelegateOfPropertyUpdate();
}

std::optional<Ref<WebCore::Icon>> WebExtensionSidebar::icon(WebCore::FloatSize size)
{
    RefPtr context = extensionContext();
    if (!context)
        return std::nullopt;

    return m_iconsOverride
        .and_then([&](Ref<JSON::Object> icons) -> std::optional<Ref<WebCore::Icon>> {
            return toOptional(context->extension().bestIcon(icons.ptr(), size, [](Ref<API::Error> error) { }));
        })
        .or_else([&] -> std::optional<Ref<WebCore::Icon>> {
            return parent().and_then([&](auto const& parent) {
                return parent.get().icon(size);
            });
        })
        // using .or_else(..) is more efficient than value_or, since value_or will evaluate its argument
        // regardless of whether or not it's used. by switching to or_else(..) we instead lazily evaluate
        // the fallback value
        .or_else([&] {
            return toOptional(context->extension().actionIcon(size));
        });
}

void WebExtensionSidebar::setIconsDictionary(std::optional<Ref<JSON::Object>> icons)
{
    if (!icons || !icons.value()->size()) {
        m_iconsOverride = std::nullopt;
        return;
    }

    if (m_iconsOverride && m_iconsOverride.value() == icons.value())
        return;

    m_iconsOverride = WTF::move(icons);
    propertiesDidChange();
}

String WebExtensionSidebar::title() const
{
    // Per the sidebar_action spec, when no title is set anywhere the effective title is the extension's name.
    return m_titleOverride
        .or_else([this] {
            return parent().transform([](auto const& parent) {
                return parent->title();
            });
        })
        .or_else([this] -> std::optional<String> {
            RefPtr context = extensionContext();
            return context ? context->extension().displayName() : String { fallbackTitle };
        })
        .value();
}

void WebExtensionSidebar::setTitle(std::optional<String> titleOverride)
{
    RefPtr context = extensionContext();
    if (!titleOverride && isDefaultSidebar() && context)
        m_titleOverride = context->extension().sidebarTitle();
    else
        m_titleOverride = titleOverride;

    propertiesDidChange();
}

bool WebExtensionSidebar::isEnabled() const
{
    return m_isEnabled
        .or_else([this] { return parent().transform([](auto const& parent) { return parent->isEnabled(); }); })
        .value_or(false);
}

void WebExtensionSidebar::setEnabled(bool enabled)
{
    m_isEnabled = enabled;
    propertiesDidChange();
}

std::optional<String> WebExtensionSidebar::resolvedSidebarPath() const
{
    return m_sidebarPathOverride.or_else([this] {
        return parent().and_then([](auto const& parent) {
            return parent->resolvedSidebarPath();
        });
    });
}

String WebExtensionSidebar::sidebarPath() const
{
    return resolvedSidebarPath().value_or(fallbackPath);
}

void WebExtensionSidebar::setSidebarPath(std::optional<String> sidebarPath)
{
    RefPtr context = extensionContext();
    if (!sidebarPath && isDefaultSidebar() && context)
        m_sidebarPathOverride = context->extension().sidebarDocumentPath();
    else
        m_sidebarPathOverride = sidebarPath;

    if (isParentSidebar())
        notifyChildrenOfPropertyUpdate(ShouldReloadWebView::Yes);
    else
        reloadWebView();
}

void WebExtensionSidebar::willOpenSidebar()
{
    ASSERT(isEnabled());
    ASSERT(!isDefaultSidebar());
    ASSERT(!static_cast<bool>(m_window));

    RELEASE_LOG_ERROR_IF(!isEnabled(), Extensions, "willOpenSidebar was called on a sidebar object which is currently disabled");
    RELEASE_LOG_ERROR_IF(isDefaultSidebar(), Extensions, "willOpenSidebar was called on the default sidebar object");
    RELEASE_LOG_ERROR_IF(static_cast<bool>(m_window), Extensions, "willOpenSidebar was called on a window-global sidebar object");

    m_isOpen = true;
    didReceiveUserInteraction();
}

void WebExtensionSidebar::willCloseSidebar()
{
    ASSERT(!isDefaultSidebar());
    ASSERT(!static_cast<bool>(m_window));

    RELEASE_LOG_ERROR_IF(isDefaultSidebar(), Extensions, "willCloseSidebar was called on the default sidebar object");
    RELEASE_LOG_ERROR_IF(static_cast<bool>(m_window), Extensions, "willCloseSidebar was called on a window-global sidebar object");

    m_isOpen = false;
}

void WebExtensionSidebar::sidebarWillAppear()
{
    m_isOpen = true;
}

void WebExtensionSidebar::sidebarWillDisappear()
{
    m_isOpen = false;
}

void WebExtensionSidebar::addChild(WebExtensionSidebar const& child)
{
    ASSERT(&child != this);
    m_children.add(child);
}

void WebExtensionSidebar::removeChild(WebExtensionSidebar const& child)
{
    m_children.remove(child);
}

void WebExtensionSidebar::didReceiveUserInteraction()
{
    auto currentTab = tab();
    RefPtr currentContext = extensionContext();

    ASSERT(isOpen());
    ASSERT(currentTab);

    if (!(isOpen() && currentTab && currentContext))
        return;

    currentContext->userGesturePerformed(currentTab.value());
}

RetainPtr<SidebarViewControllerType> WebExtensionSidebar::viewController()
{
    // Only tab-specific sidebars should be rendered
    if (!m_tab)
        return nil;

    if (!m_viewController)
        m_viewController = [[_WKWebExtensionSidebarViewController alloc] initWithWebExtensionSidebar:*this];

    return m_viewController;
}

WKWebView *WebExtensionSidebar::webView()
{
    // Only tab-specific sidebars should be rendered
    if (!m_tab)
        return nil;

    RefPtr context = extensionContext();
    if (!opensSidebar() || !context)
        return nil;

    if (m_webView)
        return m_webView.get();

    auto *webViewConfiguration = context->webViewConfiguration(WebExtensionContext::WebViewPurpose::Sidebar);
    m_webView = [[_WKWebExtensionSidebarWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration webExtensionSidebar:*this];
    m_webView.get().inspectable = context->isInspectable();
    m_webView.get().accessibilityLabel = title().createNSString().get();
    m_webViewDelegate = [[_WKWebExtensionSidebarWebViewDelegate alloc] initWithWebExtensionSidebar:*this];
    m_webView.get().navigationDelegate = m_webViewDelegate.get();

    reloadWebView();

    return m_webView.get();
}

void WebExtensionSidebar::parentPropertiesWereUpdated(ShouldReloadWebView shouldReload)
{
    ASSERT(!isDefaultSidebar());

    // If we have local overrides on all properties, then a parent property update does not effect this sidebar
    if (m_iconsOverride.has_value() && m_titleOverride.has_value() && m_sidebarPathOverride.has_value() && m_isEnabled.has_value())
        return;

    // Delegate property update notifications should only come from non-parent (i.e. tab-specific) sidebars
    if (isParentSidebar())
        notifyChildrenOfPropertyUpdate(shouldReload);
    else if (shouldReload == ShouldReloadWebView::Yes)
        reloadWebView();
    else
        notifyDelegateOfPropertyUpdate();
}

void WebExtensionSidebar::notifyChildrenOfPropertyUpdate(ShouldReloadWebView shouldReload)
{
    for (auto& childSidebar : m_children)
        childSidebar.parentPropertiesWereUpdated(shouldReload);
}

void WebExtensionSidebar::notifyDelegateOfPropertyUpdate()
{
    RefPtr context = extensionContext();
    if (!context)
        return;

    RefPtr extensionController = context->extensionController();
    if (!extensionController)
        return;

    auto *delegate = extensionController->delegate();
    if (![delegate respondsToSelector:@selector(_webExtensionController:didUpdateSidebar:forExtensionContext:)])
        return;

    auto *extensionControllerWrapper = extensionController->wrapper();
    auto *sidebarWrapper = wrapper();
    auto *contextWrapper = context->wrapper();

    if (!(extensionControllerWrapper && sidebarWrapper && contextWrapper))
        return;

    [delegate _webExtensionController:extensionControllerWrapper didUpdateSidebar:sidebarWrapper forExtensionContext:contextWrapper];
}

void WebExtensionSidebar::reloadWebView()
{
    if (!m_webView)
        return;

    RefPtr context = extensionContext();
    if (!context)
        return;

    auto url = URL { context->baseURL(), sidebarPath() };
    [m_webView loadRequest:[NSURLRequest requestWithURL:url.createNSURL().get()]];
}

}

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
