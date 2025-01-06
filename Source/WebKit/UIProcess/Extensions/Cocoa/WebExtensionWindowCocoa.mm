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
#import "WebExtensionWindow.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "WKWebExtensionTab.h"
#import "WKWebExtensionWindow.h"
#import "WebExtensionContext.h"
#import "WebExtensionTabQueryParameters.h"
#import "WebExtensionUtilities.h"
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionWindow);

WebExtensionWindow::WebExtensionWindow(const WebExtensionContext& context, WKWebExtensionWindow* delegate)
    : m_extensionContext(context)
    , m_delegate(delegate)
    , m_respondsToTabs([delegate respondsToSelector:@selector(tabsForWebExtensionContext:)])
    , m_respondsToActiveTab([delegate respondsToSelector:@selector(activeTabForWebExtensionContext:)])
    , m_respondsToWindowType([delegate respondsToSelector:@selector(windowTypeForWebExtensionContext:)])
    , m_respondsToWindowState([delegate respondsToSelector:@selector(windowStateForWebExtensionContext:)])
    , m_respondsToSetWindowState([delegate respondsToSelector:@selector(setWindowState:forWebExtensionContext:completionHandler:)])
    , m_respondsToIsPrivate([delegate respondsToSelector:@selector(isPrivateForWebExtensionContext:)])
    , m_respondsToFrame([delegate respondsToSelector:@selector(frameForWebExtensionContext:)])
    , m_respondsToSetFrame([delegate respondsToSelector:@selector(setFrame:forWebExtensionContext:completionHandler:)])
#if PLATFORM(MAC)
    , m_respondsToScreenFrame([delegate respondsToSelector:@selector(screenFrameForWebExtensionContext:)])
#endif
    , m_respondsToFocus([delegate respondsToSelector:@selector(focusForWebExtensionContext:completionHandler:)])
    , m_respondsToClose([delegate respondsToSelector:@selector(closeForWebExtensionContext:completionHandler:)])
{
}

WebExtensionContext* WebExtensionWindow::extensionContext() const
{
    return m_extensionContext.get();
}

bool WebExtensionWindow::operator==(const WebExtensionWindow& other) const
{
    return this == &other || (identifier() == other.identifier() && m_extensionContext == other.m_extensionContext && m_delegate.get() == other.m_delegate.get());
}

WebExtensionWindowParameters WebExtensionWindow::parameters(PopulateTabs populate) const
{
    Vector<WebExtensionTabParameters> tabParameters;

    if (populate == PopulateTabs::Yes) {
        auto tabs = this->tabs();
        tabParameters = WTF::map(tabs, [](auto& tab) {
            return tab->parameters();
        });
    }

    auto frame = this->normalizedFrame();

    return {
        identifier(),
        state(),
        type(),

        populate == PopulateTabs::Yes ? std::optional(WTFMove(tabParameters)) : std::nullopt,
        !CGRectIsNull(frame) ? std::optional(frame) : std::nullopt,

        isFocused(),
        isPrivate()
    };
}

WebExtensionWindowParameters WebExtensionWindow::minimalParameters() const
{
    return {
        identifier(),
        std::nullopt,
        type(),
        std::nullopt,
        std::nullopt,
        std::nullopt,
        std::nullopt
    };
}

bool WebExtensionWindow::matches(OptionSet<TypeFilter> filter) const
{
    if (!extensionHasAccess())
        return false;

    switch (type()) {
    case Type::Normal:
        return filter.contains(TypeFilter::Normal);

    case Type::Popup:
        return filter.contains(TypeFilter::Popup);
    }
}

bool WebExtensionWindow::matches(const WebExtensionTabQueryParameters& parameters, std::optional<WebPageProxyIdentifier> webPageProxyIdentifier) const
{
    if (!extensionHasAccess())
        return false;

    if (parameters.windowIdentifier && identifier() != parameters.windowIdentifier.value() && !isCurrent(parameters.windowIdentifier.value()))
        return false;

    if (parameters.windowType && !matches(parameters.windowType.value()))
        return false;

    if (parameters.frontmostWindow && isFrontmost() != parameters.frontmostWindow.value())
        return false;

    if (parameters.currentWindow || isCurrent(parameters.windowIdentifier)) {
        RefPtr extensionContext = this->extensionContext();
        if (!extensionContext)
            return false;

        auto currentWindow = extensionContext->getWindow(WebExtensionWindowConstants::CurrentIdentifier, webPageProxyIdentifier);
        if (!currentWindow)
            return false;

        if (identifier() != currentWindow->identifier())
            return false;
    }

    return true;
}

bool WebExtensionWindow::extensionHasAccess() const
{
    bool isPrivate = this->isPrivate();
    return !isPrivate || (isPrivate && extensionContext()->hasAccessToPrivateData());
}

WebExtensionWindow::TabVector WebExtensionWindow::tabs(SkipValidation skipValidation) const
{
    if (!isValid() || !m_respondsToTabs || !m_respondsToActiveTab)
        return { };

    RefPtr extensionContext = m_extensionContext.get();
    if (!extensionContext)
        return { };

    auto *tabs = [m_delegate tabsForWebExtensionContext:extensionContext->wrapper()];
    THROW_UNLESS([tabs isKindOfClass:NSArray.class], @"Object returned by tabsForWebExtensionContext: is not an array");

    if (!tabs.count)
        return { };

    TabVector result;
    result.reserveInitialCapacity(tabs.count);

    for (id tab in tabs)
        result.append(extensionContext->getOrCreateTab(tab));

    if (skipValidation == SkipValidation::No) {
        // SkipValidation::Yes is needed to avoid reentry, since activeTab() calls tabs().
        RefPtr activeTab = this->activeTab(SkipValidation::Yes);
        if (!activeTab || !result.contains(*activeTab)) {
            RELEASE_LOG_ERROR(Extensions, "Array returned by tabsForWebExtensionContext: does not contain the active tab; %{sensitive}@ not in %{sensitive}@", activeTab ? activeTab->delegate() : nil, tabs);
            ASSERT_NOT_REACHED();
            return { };
        }
    }

    return result;
}

RefPtr<WebExtensionTab> WebExtensionWindow::activeTab(SkipValidation skipValidation) const
{
    if (!isValid() || !m_respondsToActiveTab || !m_respondsToTabs)
        return nullptr;

    RefPtr extensionContext = m_extensionContext.get();
    if (!extensionContext)
        return nullptr;

    auto activeTab = [m_delegate activeTabForWebExtensionContext:extensionContext->wrapper()];
    if (!activeTab)
        return nullptr;

    Ref result = extensionContext->getOrCreateTab(activeTab);

    if (skipValidation == SkipValidation::No) {
        // SkipValidation::Yes is needed to avoid reentry, since tabs() calls activeTab().
        auto tabs = this->tabs(SkipValidation::Yes);
        if (!tabs.contains(result)) {
            RELEASE_LOG_ERROR(Extensions, "Array returned by tabsForWebExtensionContext: does not contain the active tab; %{sensitive}@ not in %{sensitive}@", result->delegate(), [m_delegate tabsForWebExtensionContext:m_extensionContext->wrapper()] ?: @[ ]);
            ASSERT_NOT_REACHED();
            return nullptr;
        }
    }

    return result;
}

WKWebExtensionWindowType toAPI(WebExtensionWindow::Type type)
{
    switch (type) {
    case WebExtensionWindow::Type::Normal:
        return WKWebExtensionWindowTypeNormal;
    case WebExtensionWindow::Type::Popup:
        return WKWebExtensionWindowTypePopup;
    }

    ASSERT_NOT_REACHED();
    return WKWebExtensionWindowTypeNormal;
}

static inline WebExtensionWindow::Type toImpl(WKWebExtensionWindowType type)
{
    switch (type) {
    case WKWebExtensionWindowTypeNormal:
        return WebExtensionWindow::Type::Normal;
    case WKWebExtensionWindowTypePopup:
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

static inline WebExtensionWindow::State toImpl(WKWebExtensionWindowState state)
{
    switch (state) {
    case WKWebExtensionWindowStateNormal:
        return WebExtensionWindow::State::Normal;
    case WKWebExtensionWindowStateMinimized:
        return WebExtensionWindow::State::Minimized;
    case WKWebExtensionWindowStateMaximized:
        return WebExtensionWindow::State::Maximized;
    case WKWebExtensionWindowStateFullscreen:
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

WKWebExtensionWindowState toAPI(WebExtensionWindow::State state)
{
    switch (state) {
    case WebExtensionWindow::State::Normal:
        return WKWebExtensionWindowStateNormal;
    case WebExtensionWindow::State::Minimized:
        return WKWebExtensionWindowStateMinimized;
    case WebExtensionWindow::State::Maximized:
        return WKWebExtensionWindowStateMaximized;
    case WebExtensionWindow::State::Fullscreen:
        return WKWebExtensionWindowStateFullscreen;
    }

    ASSERT_NOT_REACHED();
    return WKWebExtensionWindowStateNormal;
}

void WebExtensionWindow::setState(WebExtensionWindow::State state, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.update()";

    if (!isValid() || !m_respondsToSetWindowState || !m_respondsToWindowState) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'state'"));
        return;
    }

    [m_delegate setWindowState:toAPI(state) forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for setWindowState: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

bool WebExtensionWindow::isOpen() const
{
    return m_isOpen && isValid();
}

bool WebExtensionWindow::isFocused() const
{
    if (!isValid())
        return false;

    RefPtr extensionContext = m_extensionContext.get();
    if (!extensionContext)
        return false;

    return this == extensionContext->focusedWindow();
}

bool WebExtensionWindow::isFrontmost() const
{
    if (!isValid())
        return false;

    RefPtr extensionContext = m_extensionContext.get();
    if (!extensionContext)
        return false;

    return this == extensionContext->frontmostWindow();
}

void WebExtensionWindow::focus(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.update()";

    if (!isValid() || !m_respondsToFocus) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'focused'"));
        return;
    }

    [m_delegate focusForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for window focus: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

bool WebExtensionWindow::isPrivate() const
{
    if (m_cachedPrivate)
        return m_private;

    if (!isValid() || !m_respondsToIsPrivate)
        return false;

    // Private can't change after the fact, so cache it for quick access and to ensure it does not change.
    m_private = [m_delegate isPrivateForWebExtensionContext:m_extensionContext->wrapper()];
    m_cachedPrivate = true;

    return m_private;
}

CGRect WebExtensionWindow::normalizedFrame() const
{
    auto frame = this->frame();

#if PLATFORM(MAC)
    // Window coordinates on macOS have the origin in the bottom-left corner.
    // Web Extensions have window coordinates in the top-left corner.
    auto screenFrame = this->screenFrame();
    if (!CGRectIsNull(frame) && !CGRectIsEmpty(screenFrame))
        frame.origin.y = screenFrame.size.height - frame.origin.y - frame.size.height;
#endif

    return frame;
}

CGRect WebExtensionWindow::frame() const
{
    if (!isValid() || !m_respondsToFrame)
        return CGRectNull;

    return CGRectStandardize([m_delegate frameForWebExtensionContext:m_extensionContext->wrapper()]);
}

void WebExtensionWindow::setFrame(CGRect frame, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.update()";

#if PLATFORM(MAC)
    if (!isValid() || !m_respondsToSetFrame || !m_respondsToFrame || !m_respondsToScreenFrame)
#else
    if (!isValid() || !m_respondsToSetFrame || !m_respondsToFrame)
#endif
    {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented for 'top', 'left', 'width', and 'height'"));
        return;
    }

    ASSERT(!std::isnan(frame.origin.x) && !std::isnan(frame.origin.y) && !std::isnan(frame.size.width) && !std::isnan(frame.size.height));

    frame = CGRectStandardize(frame);

    [m_delegate setFrame:frame forWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for setFrame: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

#if PLATFORM(MAC)
CGRect WebExtensionWindow::screenFrame() const
{
    if (!isValid() || !m_respondsToScreenFrame)
        return CGRectNull;

    return CGRectStandardize([m_delegate screenFrameForWebExtensionContext:m_extensionContext->wrapper()]);
}
#endif

void WebExtensionWindow::close(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"windows.remove()";

    if (!isValid() || !m_respondsToClose) {
        completionHandler(toWebExtensionError(apiName, nil, @"it is not implemented"));
        return;
    }

    [m_delegate closeForWebExtensionContext:m_extensionContext->wrapper() completionHandler:makeBlockPtr([protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](NSError *error) mutable {
        if (error) {
            RELEASE_LOG_ERROR(Extensions, "Error for window close: %{public}@", privacyPreservingDescription(error));
            completionHandler(toWebExtensionError(apiName, nil, error.localizedDescription));
            return;
        }

        completionHandler({ });
    }).get()];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
