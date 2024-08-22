/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WebExtensionContext.h"
#import "WebExtensionTab.h"
#import "WebExtensionWindow.h"

namespace WebKit {

static NSString * const fallbackPath = @"about:blank";
static NSString * const fallbackTitle = @"";


static std::optional<String> getDefaultSidebarTitleFromExtension(WebExtension& extension)
{
    return toOptional(extension.sidebarTitle())
        .value_or(extension.displayName());
}

static std::optional<String> getDefaultSidebarPathFromExtension(WebExtension& extension)
{
    return toOptional(extension.sidebarDocumentPath());
}

static std::optional<NSDictionary *> getDefaultIconsDictFromExtension(WebExtension& extensions)
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
        m_titleOverride = getDefaultSidebarTitleFromExtension(extension);
        m_sidebarPathOverride = getDefaultSidebarPathFromExtension(extension);
        m_iconsOverride = getDefaultIconsDictFromExtension(extension);
    }
}

std::optional<Ref<WebExtensionContext>> WebExtensionSidebar::extensionContext() const
{
    if (auto *context = m_extensionContext.get())
        return *context;
    return std::nullopt;
}

const std::optional<Ref<WebExtensionTab>> WebExtensionSidebar::tab() const
{
    return m_tab;
}

const std::optional<Ref<WebExtensionWindow>> WebExtensionSidebar::window() const
{
    return m_window;
}

std::optional<Ref<WebExtensionSidebar>> WebExtensionSidebar::parent() const
{
    if (!extensionContext() || isDefaultSidebar())
        return std::nullopt;

    return m_tab.and_then([this](auto const& tab) -> std::optional<Ref<WebExtensionSidebar>> {
        return tab->window() ? m_extensionContext->getSidebar(*(tab->window())) : std::nullopt;
    }).value_or(m_extensionContext->defaultSidebar());
}

void WebExtensionSidebar::propertiesDidChange()
{
    // FIXME: <https://webkit.org/b/277575> notify the delegate that something has changed (implement this)
}

RetainPtr<CocoaImage> WebExtensionSidebar::icon(CGSize size)
{
    if (!extensionContext())
        return nil;

    const auto largestDim = [](CGSize size) {
        return size.width > size.height ? size.width : size.height;
    };

    auto& context = extensionContext().value().get();
    return m_iconsOverride
        .and_then([&](RetainPtr<NSDictionary> icons) -> std::optional<RetainPtr<CocoaImage>> {
            return toOptional(context.extension().bestImageInIconsDictionary(icons.get(), largestDim(size)));
        })
        .or_else([&] -> std::optional<RetainPtr<CocoaImage>> {
            return parent().transform([&](auto const& parent) { return parent.get().icon(size); });
        })
        // using .or_else(..).value() is more efficient than value_or, since value_or will evaluate its argument
        // regardless of whether or not it's used. by switching to or_else(..).value() we instead lazily evaluate
        // the fallback value
        .or_else([&] { return std::optional { RetainPtr(context.extension().actionIcon(size)) }; })
        .value();
}

void WebExtensionSidebar::setIconsDictionary(NSDictionary *icons)
{
    if (!icons || !icons.count) {
        m_iconsOverride = std::nullopt;
        return;
    }

    if (m_iconsOverride && [m_iconsOverride.value() isEqualToDictionary:icons])
        return;

    m_iconsOverride = icons;
    propertiesDidChange();
}

String WebExtensionSidebar::title() const
{
    return m_titleOverride
        .or_else([this] { return parent().transform([](auto const& parent) { return parent->title(); }); })
        .value_or(fallbackTitle);
}

void WebExtensionSidebar::setTitle(std::optional<String> titleOverride)
{
    if (!titleOverride && isDefaultSidebar() && extensionContext())
        m_titleOverride = getDefaultSidebarTitleFromExtension(extensionContext().value()->extension());
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

bool WebExtensionSidebar::canProgrammaticallyOpenSidebar() const
{
    return extensionContext().transform([](auto const& context) -> bool { return !!context.get().extensionController(); })
        .value_or(false);

    // FIXME: <https://webkit.org/b/277575> also check that the controller delegate responds to whatever selector we use for this
}

void WebExtensionSidebar::openSidebarWhenReady()
{
    // FIXME: <https://webkit.org/b/277575> implement openSidebarWhenReady
}

bool WebExtensionSidebar::canProgrammaticallyCloseSidebar() const
{
    return extensionContext().transform([](auto const& context) -> bool { return !!context.get().extensionController(); })
        .value_or(false);

    // FIXME: <https://webkit.org/b/277575> also check that the controller delegate responds to whatever selector we use for this
}

void WebExtensionSidebar::closeSidebarWhenReady()
{
    // FIXME: <https://webkit.org/b/277575> implement closeSidebarWhenReady
}

String WebExtensionSidebar::sidebarPath() const
{
    return m_sidebarPathOverride
        .or_else([this] { return parent().transform([](auto const& parent) { return parent->sidebarPath(); }); })
        .value_or(fallbackPath);
}

void WebExtensionSidebar::setSidebarPath(std::optional<String> sidebarPath)
{
    if (!sidebarPath && isDefaultSidebar() && extensionContext())
        m_sidebarPathOverride = getDefaultSidebarPathFromExtension(extensionContext().value()->extension());
    else
        m_sidebarPathOverride = sidebarPath;

    propertiesDidChange();
}

WKWebView *WebExtensionSidebar::webView()
{
    return m_webView.get();
}

}

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
