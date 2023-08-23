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
#import "WebExtensionWindow.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WebExtensionContext.h"
#import "_WKWebExtensionTab.h"
#import "_WKWebExtensionWindow.h"

namespace WebKit {

WebExtensionWindow::WebExtensionWindow(const WebExtensionContext& context, _WKWebExtensionWindow* delegate)
    : m_identifier(WebExtensionWindowIdentifier::generate())
    , m_extensionContext(context)
    , m_delegate(delegate)
    , m_respondsToTabs([delegate respondsToSelector:@selector(tabsForWebExtensionContext:)])
    , m_respondsToActiveTab([delegate respondsToSelector:@selector(activeTabForWebExtensionContext:)])
    , m_respondsToWindowType([delegate respondsToSelector:@selector(windowTypeForWebExtensionContext:)])
    , m_respondsToWindowState([delegate respondsToSelector:@selector(windowStateForWebExtensionContext:)])
    , m_respondsToIsEphemeral([delegate respondsToSelector:@selector(isEphemeralForWebExtensionContext:)])
    , m_respondsToFrame([delegate respondsToSelector:@selector(frameForWebExtensionContext:)])
{
    ASSERT([delegate conformsToProtocol:@protocol(_WKWebExtensionWindow)]);
}

bool WebExtensionWindow::operator==(const WebExtensionWindow& other) const
{
    return this == &other || (m_identifier == other.m_identifier && m_extensionContext == other.m_extensionContext && m_delegate.get() == other.m_delegate.get());
}

WebExtensionWindow::TabVector WebExtensionWindow::tabs() const
{
    TabVector result;

    if (!isValid() || !m_respondsToTabs || !m_respondsToActiveTab)
        return result;

    auto *tabs = [m_delegate tabsForWebExtensionContext:m_extensionContext->wrapper()];
    THROW_UNLESS([tabs isKindOfClass:NSArray.class], @"Object returned by tabsForWebExtensionContext: is not an array");

    if (!tabs.count)
        return result;

    result.reserveInitialCapacity(tabs.count);

    for (id<_WKWebExtensionTab> tab in tabs) {
        THROW_UNLESS([tab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object in array returned by tabsForWebExtensionContext: does not conform to the _WKWebExtensionTab protocol");
        result.uncheckedAppend(m_extensionContext->getOrCreateTab(tab));
    }

    if (auto activeTab = [m_delegate activeTabForWebExtensionContext:m_extensionContext->wrapper()]) {
        THROW_UNLESS([activeTab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object returned by activeTabForWebExtensionContext: does not conform to the _WKWebExtensionTab protocol");
        THROW_UNLESS([tabs containsObject:activeTab], @"Array returned by tabsForWebExtensionContext: does not contain the active tab");
    }

    return result;
}

RefPtr<WebExtensionTab> WebExtensionWindow::activeTab() const
{
    if (!isValid() || !m_respondsToActiveTab || !m_respondsToTabs)
        return nullptr;

    auto activeTab = [m_delegate activeTabForWebExtensionContext:m_extensionContext->wrapper()];
    if (!activeTab)
        return nullptr;

    THROW_UNLESS([activeTab conformsToProtocol:@protocol(_WKWebExtensionTab)], @"Object returned by activeTabForWebExtensionContext: does not conform to the _WKWebExtensionTab protocol");
    auto result = m_extensionContext->getOrCreateTab(activeTab);

    auto *tabs = [m_delegate tabsForWebExtensionContext:m_extensionContext->wrapper()];
    THROW_UNLESS([tabs isKindOfClass:NSArray.class], @"Object returned by tabsForWebExtensionContext: is not an array");
    THROW_UNLESS([tabs containsObject:activeTab], @"Array returned by tabsForWebExtensionContext: does not contain the active tab");

    return result;
}

static inline WebExtensionWindow::Type toImpl(_WKWebExtensionWindowType type)
{
    switch (type) {
    case _WKWebExtensionWindowTypeNormal:
        return WebExtensionWindow::Type::Normal;
    case _WKWebExtensionWindowTypePopup:
        return WebExtensionWindow::Type::Popup;
    }

    ASSERT_NOT_REACHED();
    return WebExtensionWindow::Type::Normal;
}

WebExtensionWindow::Type WebExtensionWindow::type() const
{
    if (!isValid() || !m_respondsToWindowType)
        return Type::Normal;

    return toImpl([m_delegate windowTypeForWebExtensionContext:m_extensionContext->wrapper()]);
}

static inline WebExtensionWindow::State toImpl(_WKWebExtensionWindowState state)
{
    switch (state) {
    case _WKWebExtensionWindowStateNormal:
        return WebExtensionWindow::State::Normal;
    case _WKWebExtensionWindowStateMinimized:
        return WebExtensionWindow::State::Minimized;
    case _WKWebExtensionWindowStateMaximized:
        return WebExtensionWindow::State::Maximized;
    case _WKWebExtensionWindowStateFullscreen:
        return WebExtensionWindow::State::Fullscreen;
    }

    ASSERT_NOT_REACHED();
    return WebExtensionWindow::State::Normal;
}

WebExtensionWindow::State WebExtensionWindow::state() const
{
    if (!isValid() || !m_respondsToWindowState)
        return State::Normal;

    return toImpl([m_delegate windowStateForWebExtensionContext:m_extensionContext->wrapper()]);
}

bool WebExtensionWindow::isFocused() const
{
    if (!isValid())
        return false;

    return this == m_extensionContext->focusedWindow();
}

bool WebExtensionWindow::isEphemeral() const
{
    if (!isValid() || !m_respondsToIsEphemeral)
        return false;

    return [m_delegate isEphemeralForWebExtensionContext:m_extensionContext->wrapper()];
}

CGRect WebExtensionWindow::frame() const
{
    if (!isValid() || !m_respondsToFrame)
        return CGRectZero;

    return [m_delegate frameForWebExtensionContext:m_extensionContext->wrapper()];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
