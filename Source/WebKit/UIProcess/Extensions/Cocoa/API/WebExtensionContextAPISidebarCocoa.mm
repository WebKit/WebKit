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
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#import "WebExtensionSidebar.h"

namespace WebKit {

template<typename T>
static Expected<T, WebExtensionError> toExpected(std::optional<T>&& optional, NSString * const errorMessage = @"value not found")
{
    if (optional)
        return WTFMove(optional.value());
    return toWebExtensionError(nil, nil, errorMessage);
}

static Expected<Ref<WebExtensionSidebar>, WebExtensionError> getSidebarWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context)
{
    if (windowIdentifier && tabIdentifier)
        return toWebExtensionError(nil, nil, @"cannot specify both windowId and tabId");

    if (windowIdentifier) {
        RefPtr window = context.getWindow(*windowIdentifier);
        if (!window)
            return toWebExtensionError(nil, nil, @"window not found");
        return context.getSidebar(*window).value_or(context.defaultSidebar());
    }

    if (tabIdentifier) {
        RefPtr tab = context.getTab(*tabIdentifier);
        if (!tab)
            return toWebExtensionError(nil, nil, @"tab not found");
        return context.getSidebar(*tab)
            .or_else([&] { return context.getSidebar(*(tab->window())); })
            .value_or(context.defaultSidebar());
    }

    return Ref { context.defaultSidebar() };
}

static Expected<Ref<WebExtensionSidebar>, WebExtensionError> getOrCreateSidebarWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context)
{
    if (windowIdentifier && tabIdentifier)
        return toWebExtensionError(nil, nil, @"cannot specify both windowId and tabId");

    if (windowIdentifier) {
        RefPtr window = context.getWindow(*windowIdentifier);
        if (!window)
            return toWebExtensionError(nil, nil, @"window not found");
        return toExpected(context.getOrCreateSidebar(*window), @"could not create sidebar");
    }

    if (tabIdentifier) {
        RefPtr tab = context.getTab(*tabIdentifier);
        if (!tab)
            return toWebExtensionError(nil, nil, @"tab not found");
        return toExpected(context.getOrCreateSidebar(*tab), @"could not create sidebar");
    }

    return Ref { context.defaultSidebar() };
}

using UserTriggered = WebExtensionContext::UserTriggered;
void WebExtensionContext::openSidebarForTab(WebExtensionTab& tab, const UserTriggered userTriggered)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    if (userTriggered == UserTriggered::Yes)
        userGesturePerformed(tab);

    auto maybeSidebar = getOrCreateSidebar(tab);
    if (!maybeSidebar)
        return;

    auto& sidebar = maybeSidebar.value().get();
    if (sidebar.opensSidebar())
        sidebar.openSidebarWhenReady();

    fireActionClickedEventIfNeeded(&tab);
}

void WebExtensionContext::closeSidebarForTab(WebExtensionTab& tab, const UserTriggered userTriggered)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    if (userTriggered == UserTriggered::Yes)
        userGesturePerformed(tab);

    auto maybeSidebar = getOrCreateSidebar(tab);
    if (!maybeSidebar)
        return;

    auto& sidebar = maybeSidebar.value().get();
    if (sidebar.canProgrammaticallyCloseSidebar())
        sidebar.closeSidebarWhenReady();
}

void WebExtensionContext::sidebarOpen(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    // the error below is a placeholder to fill sidebar, since we cannot instantiate it empty
    Expected<Ref<WebExtensionSidebar>, WebExtensionError> sidebar = toWebExtensionError(nil, nil, @"Placeholder error");
    RefPtr<WebExtensionTab> tab;

    if (!tabIdentifier && !windowIdentifier) {
        // In the case where we have neither identifier, we must be servicing sidebarAction rather than sidepanel
        // since sidePanel requires one of the identifiers to be set in calls to open.
        // The firefox API specifies that open() will just toggle the sidebar in the active window.
        if (auto window = frontmostWindow()) {
            auto maybeSidebar = getOrCreateSidebar(*window);
            if (!maybeSidebar) {
                completionHandler(toWebExtensionError(nil, nil, @"could not create sidebar"));
                return;
            }

            sidebar = WTFMove(maybeSidebar.value());
            tab = window->activeTab();
        } else {
            completionHandler(toWebExtensionError(nil, nil, @"no windows are open"));
            return;
        }
    } else {
        // chrome's sidePanel allows both windowIdentifier and tabIdentifier to be specified for sidePanel.open(), so we will discard windowIdentifier if they are both set
        // since firefox's sidebarAction takes no arguments to sidebarAction.open(), this does not break behavior for that API
        auto correctedWindowIdentifier = tabIdentifier ? std::nullopt : windowIdentifier;

        auto maybeSidebar = getOrCreateSidebarWithIdentifiers(correctedWindowIdentifier, tabIdentifier, *this);
        if (!maybeSidebar) {
            completionHandler(toWebExtensionError(nil, nil, @"could not create sidebar"));
            return;
        }

        sidebar = WTFMove(maybeSidebar.value());

        if (auto window = sidebar.value()->window())
            tab = window->get().activeTab();
        else if (auto maybeTab = sidebar.value()->tab())
            tab = RefPtr { maybeTab.value().ptr() };
        else if (auto window = frontmostWindow())
            tab = window->activeTab();
        else {
            completionHandler(toWebExtensionError(nil, nil, @"no windows are open"));
            return;
        }
    }

    if (!tab) {
        completionHandler(toWebExtensionError(nil, nil, @"could not determine active tab"));
        return;
    }

    if (!sidebar) {
        completionHandler(makeUnexpected(sidebar.error()));
        return;
    }

    if (!sidebar.value()->canProgrammaticallyOpenSidebar()) {
        completionHandler(toWebExtensionError(nil, nil, @"it is not implemented"));
        return;
    }

    if (sidebar.value()->opensSidebar())
        openSidebarForTab(*tab, UserTriggered::No);

    completionHandler({ });
}

void WebExtensionContext::sidebarClose(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    // This method services a firefox-only API which will just close the sidebar in the active window
    auto window = frontmostWindow();
    if (!window) {
        completionHandler(toWebExtensionError(nil, nil, @"no windows are open"));
        return;
    }

    auto tab = window->activeTab();
    if (!tab) {
        completionHandler(toWebExtensionError(nil, nil, @"unable to determine the active tab"));
        return;
    }

    auto maybeSidebar = getOrCreateSidebar(*window);
    if (!maybeSidebar) {
        completionHandler(toWebExtensionError(nil, nil, @"the sidebar could not be created"));
        return;
    }

    auto& sidebar = maybeSidebar.value();
    if (!sidebar->canProgrammaticallyCloseSidebar()) {
        completionHandler(toWebExtensionError(nil, nil, @"it is not implemented"));
        return;
    }

    closeSidebarForTab(*tab, UserTriggered::No);

    completionHandler({ });
}

void WebExtensionContext::sidebarIsOpen(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, CompletionHandler<void(Expected<bool, WebExtensionError>&&)>&& completionHandler)
{
    // This method services a Firefox-only API which will check if a sidebar is open in the specified window, or the active window if no window is specified
    RefPtr<WebExtensionWindow> window;
    if (windowIdentifier)
        window = getWindow(*windowIdentifier);
    else
        window = frontmostWindow();

    // if no windows are open, then no sidebars are open
    if (!window) {
        completionHandler(false);
        return;
    }

    bool isOpen = false;
    if (auto currentTab = window->activeTab()) {
        if (auto currentTabSidebar = getSidebar(*currentTab))
            isOpen |= currentTabSidebar.value()->isOpen();
    }

    if (auto currentWindowSidebar = getSidebar(*window))
        isOpen |= currentWindowSidebar.value()->isOpen();

    completionHandler(isOpen);
}

void WebExtensionContext::sidebarToggle(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    // this method services a Firefox-only API which toggles the sidebar in the currently active window
    auto window = frontmostWindow();
    if (!window) {
        completionHandler(toWebExtensionError(nil, nil, @"no windows are open"));
        return;
    }

    auto tab = window->activeTab();
    if (!tab) {
        completionHandler(toWebExtensionError(nil, nil, @"unable to determine the active tab"));
        return;
    }

    auto maybeSidebar = getOrCreateSidebar(*window);
    if (!maybeSidebar) {
        completionHandler(toWebExtensionError(nil, nil, @"could not create sidebar"));
        return;
    }

    auto& sidebar = maybeSidebar.value();
    if (sidebar->isOpen()) {
        if (!sidebar->canProgrammaticallyCloseSidebar()) {
            completionHandler(toWebExtensionError(nil, nil, @"it is not implemented"));
            return;
        }

        closeSidebarForTab(*tab, UserTriggered::No);
    } else {
        if (!sidebar->canProgrammaticallyOpenSidebar()) {
            completionHandler(toWebExtensionError(nil, nil, @"it is not implemented"));
            return;
        }

        openSidebarForTab(*tab, UserTriggered::No);
    }

    completionHandler({ });
}

void WebExtensionContext::sidebarSetIcon(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& iconJSON, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    // FIXME: <https://webkit.org/b/276833> implement icon-related methods
}

void WebExtensionContext::sidebarGetTitle(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    auto sidebar = getSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!sidebar) {
        completionHandler(makeUnexpected(sidebar.error()));
        return;
    }

    completionHandler(sidebar.value()->title());
}

void WebExtensionContext::sidebarSetTitle(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, const std::optional<String>& title, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    auto sidebar = getOrCreateSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!sidebar) {
        completionHandler(makeUnexpected(sidebar.error()));
        return;
    }
    sidebar.value()->setTitle(title);

    completionHandler({ });
}

void WebExtensionContext::sidebarGetOptions(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<WebExtensionSidebarParameters, WebExtensionError>&&)>&& completionHandler)
{
    auto maybeSidebar = getOrCreateSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!maybeSidebar) {
        completionHandler(makeUnexpected(maybeSidebar.error()));
        return;
    }

    auto& sidebar = maybeSidebar.value().get();
    completionHandler(WebExtensionSidebarParameters {
        .enabled       = sidebar.isEnabled(),
        .panelPath     = sidebar.sidebarPath(),
        .tabIdentifier = sidebar.tab().transform([](auto tab) { return tab.get().identifier(); }),
    });
}

void WebExtensionContext::sidebarSetOptions(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, const std::optional<String>& panelSourcePath, const std::optional<bool> enabled, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    auto maybeSidebar = getOrCreateSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!maybeSidebar) {
        completionHandler(makeUnexpected(maybeSidebar.error()));
        return;
    }

    auto& sidebar = maybeSidebar.value().get();
    sidebar.setSidebarPath(panelSourcePath);
    // according to the sidePanel docs, `enabled` is optional with default value `true`
    // we only need to be concerned with chrome's semantics here since sidebarAction does not have any concept of enablement
    // see: https://developer.chrome.com/docs/extensions/reference/api/sidePanel#type-PanelOptions
    sidebar.setEnabled(enabled.value_or(true));
    completionHandler({ });
}

bool WebExtensionContext::isSidebarMessageAllowed()
{
    if (auto *controller = extensionController())
        return controller->isFeatureEnabled(@"WebExtensionSidebarEnabled");
    return false;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
