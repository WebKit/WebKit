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
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WebExtensionSidebar.h"

namespace WebKit {

static NSString * const unknownErrorString = @"an unknown error occurred";

template<typename T>
static Expected<T, WebExtensionError> toExpected(std::optional<T>&& optional, NSString * const errorMessage = @"value not found")
{
    if (optional)
        return WTF::move(optional.value());
    return makeUnexpected(errorMessage);
}

static Expected<Ref<WebExtensionSidebar>, WebExtensionError> getSidebarWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context)
{
    if (windowIdentifier && tabIdentifier)
        return makeUnexpected(@"it cannot specify both 'windowId' and 'tabId'");

    if (windowIdentifier) {
        RefPtr window = context.getWindow(*windowIdentifier);
        if (!window)
            return makeUnexpected(@"the window was not found");
        return context.getSidebar(*window).value_or(context.defaultSidebar());
    }

    if (tabIdentifier) {
        RefPtr tab = context.getTab(*tabIdentifier);
        if (!tab)
            return makeUnexpected(@"the tab was not found");
        return context.getSidebar(*tab)
            .or_else([&] -> std::optional<Ref<WebExtensionSidebar>> {
                RefPtr window = tab->window();
                return window ? context.getSidebar(*window) : std::nullopt;
            })
            .value_or(context.defaultSidebar());
    }

    return Ref { context.defaultSidebar() };
}

static Expected<Ref<WebExtensionSidebar>, WebExtensionError> getOrCreateSidebarWithIdentifiers(std::optional<WebExtensionWindowIdentifier> windowIdentifier, std::optional<WebExtensionTabIdentifier> tabIdentifier, WebExtensionContext& context)
{
    if (windowIdentifier && tabIdentifier)
        return makeUnexpected(@"it cannot to specify both 'windowId' and 'tabId'");

    if (windowIdentifier) {
        RefPtr window = context.getWindow(*windowIdentifier);
        if (!window)
            return makeUnexpected(@"the window was not found");
        return toExpected(context.getOrCreateSidebar(*window), unknownErrorString);
    }

    if (tabIdentifier) {
        RefPtr tab = context.getTab(*tabIdentifier);
        if (!tab)
            return makeUnexpected(@"the tab was not found");
        return toExpected(context.getOrCreateSidebar(*tab), unknownErrorString);
    }

    return Ref { context.defaultSidebar() };
}

static NSString *scopedAPINameFor(NSString *topLevelAPIMethod, WebExtensionContext& context)
{
    if (!topLevelAPIMethod)
        return nil;

    if (context.extension().hasSidebarAction())
        return [NSString stringWithFormat:@"sidebarAction.%@", topLevelAPIMethod];
    if (context.extension().hasSidePanel())
        return [NSString stringWithFormat:@"sidePanel.%@", topLevelAPIMethod];

    return topLevelAPIMethod;
}

void WebExtensionContext::openSidebar(WebExtensionSidebar& sidebar)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    auto *sidebarWrapper = sidebar.wrapper();
    auto *contextWrapper = wrapper();
    if (!(controllerDelegate && controllerWrapper && sidebarWrapper && contextWrapper))
        return;

    [controllerDelegate _webExtensionController:controllerWrapper presentSidebar:sidebarWrapper forExtensionContext:contextWrapper completionHandler:^(NSError *error) { }];
}

void WebExtensionContext::closeSidebar(WebExtensionSidebar& sidebar)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    auto *controllerWrapper = controller->wrapper();
    auto *sidebarWrapper = sidebar.wrapper();
    auto *contextWrapper = wrapper();
    if (!(controllerDelegate && controllerWrapper && sidebarWrapper && contextWrapper))
        return;

    [controllerDelegate _webExtensionController:controllerWrapper closeSidebar:sidebarWrapper forExtensionContext:contextWrapper completionHandler:^(NSError *error) { }];
}

void WebExtensionContext::notifyDelegateOfSidebarUpdate(WebExtensionSidebar& sidebar)
{
    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:didUpdateSidebar:forExtensionContext:)])
        return;

    auto *controllerWrapper = controller->wrapper();
    auto *sidebarWrapper = sidebar.wrapper();
    auto *contextWrapper = wrapper();
    if (!(controllerWrapper && sidebarWrapper && contextWrapper))
        return;

    [controllerDelegate _webExtensionController:controllerWrapper didUpdateSidebar:sidebarWrapper forExtensionContext:contextWrapper];
}

void WebExtensionContext::notifyDelegateOfSidebarInvalidation(WebExtensionSidebar& sidebar)
{
    RefPtr controller = extensionController();
    if (!controller)
        return;

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:didInvalidateSidebar:forExtensionContext:)])
        return;

    auto *controllerWrapper = controller->wrapper();
    auto *sidebarWrapper = sidebar.wrapper();
    auto *contextWrapper = wrapper();
    if (!(controllerWrapper && sidebarWrapper && contextWrapper))
        return;

    [controllerDelegate _webExtensionController:controllerWrapper didInvalidateSidebar:sidebarWrapper forExtensionContext:contextWrapper];
}

bool WebExtensionContext::canProgrammaticallyOpenSidebar()
{
    if (!extension().hasAnySidebar())
        return false;

    RefPtr controller = extensionController();
    if (!controller)
        return false;

    auto *controllerDelegate = controller->delegate();
    if (!controllerDelegate)
        return false;

    return [controllerDelegate respondsToSelector:@selector(_webExtensionController:presentSidebar:forExtensionContext:completionHandler:)];
}

bool WebExtensionContext::canProgrammaticallyCloseSidebar()
{
    if (!extension().hasAnySidebar())
        return false;

    RefPtr controller = extensionController();
    if (!controller)
        return false;

    auto *controllerDelegate = controller->delegate();
    if (!controllerDelegate)
        return false;

    return [controllerDelegate respondsToSelector:@selector(_webExtensionController:closeSidebar:forExtensionContext:completionHandler:)];
}

void WebExtensionContext::sidebarOpen(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    static NSString * const apiName = @"open()";

    if (!canProgrammaticallyOpenSidebar()) {
        completionHandler(toWebExtensionError(scopedAPINameFor(apiName, *this), nullString(), @"it is not implemented"));
        return;
    }

    auto tabResult = getTabFromIdentifiers(windowIdentifier, tabIdentifier);
    if (!tabResult) {
        completionHandler(toWebExtensionError(scopedAPINameFor(apiName, *this), nullString(), tabResult.error()));
        return;
    }

    Ref tab = WTF::move(tabResult.value());

    std::optional<Ref<WebExtensionSidebar>> sidebar = sidebarForTab(tab.get());
    if (!sidebar) {
        completionHandler(toWebExtensionError(scopedAPINameFor(apiName, *this), nullString(), unknownErrorString));
        return;
    }

    if (!sidebar.value()->isEnabled()) {
        completionHandler(toWebExtensionError(scopedAPINameFor(apiName, *this), nullString(), @"the sidebar is not enabled"));
        return;
    }

    // A sidebar with no panel has nothing to display, so there is no sidebar object to hand the browser.
    if (!sidebar.value()->opensSidebar()) {
        completionHandler(toWebExtensionError(scopedAPINameFor(apiName, *this), nullString(), @"no sidebar panel is set"));
        return;
    }

    openSidebar(sidebar.value().get());
    completionHandler({ });
}

void WebExtensionContext::sidebarClose(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    NSString * const apiName = scopedAPINameFor(@"close()", *this);

    if (!canProgrammaticallyCloseSidebar()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    auto tabResult = getTabFromIdentifiers(windowIdentifier, tabIdentifier);
    if (!tabResult) {
        completionHandler(toWebExtensionError(apiName, nullString(), tabResult.error()));
        return;
    }

    Ref tab = WTF::move(tabResult.value());

    // Chrome rejects a tab-specific close if there is no sidebar specifically for that tab
    if (tabIdentifier && !getSidebar(tab.get())) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"no side panel is open for the specified tab"));
        return;
    }

    auto maybeSidebar = sidebarForTab(tab.get());
    if (!maybeSidebar) {
        completionHandler(toWebExtensionError(apiName, nullString(), unknownErrorString));
        return;
    }

    Ref sidebar = WTF::move(maybeSidebar.value());
    closeSidebar(sidebar.get());
    completionHandler({ });
}

void WebExtensionContext::sidebarGetLayout(CompletionHandler<void(Expected<WebExtensionSidebarSide, WebExtensionError>&&)>&& completionHandler)
{
    // This method services a sidePanel-only API; sidebarAction has no layout/side concept.
    static NSString * const apiName = @"sidePanel.getLayout()";

    RefPtr controller = extensionController();
    if (!controller) {
        completionHandler(toWebExtensionError(apiName, nullString(), unknownErrorString));
        return;
    }

    auto *controllerDelegate = controller->delegate();
    if (![controllerDelegate respondsToSelector:@selector(_webExtensionController:sidebarSideForExtensionContext:)]) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    auto side = [controllerDelegate _webExtensionController:controller->wrapper() sidebarSideForExtensionContext:wrapper()];
    completionHandler(side == _WKWebExtensionSidebarSideRight ? WebExtensionSidebarSide::Right : WebExtensionSidebarSide::Left);
}

void WebExtensionContext::sidebarIsOpen(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, CompletionHandler<void(Expected<bool, WebExtensionError>&&)>&& completionHandler)
{
    // This method services a sidebarAction-only API which will check if a sidebar is open in the specified window, or the active window if no window is specified
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
        if (auto currentTabSidebar = sidebarForTab(*currentTab))
            isOpen = currentTabSidebar.value()->isOpen();
    }

    completionHandler(isOpen);
}

void WebExtensionContext::sidebarToggle(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    // this method services a sidebarAction-only API which toggles the sidebar in the currently active window
    // as such, we do not need to use scopedAPINameFor(...) here, we can just assume we're servicing sidebarAction
    static NSString * const apiName = @"sidebarAction.toggle()";

    if (!canProgrammaticallyCloseSidebar() || !canProgrammaticallyOpenSidebar()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"it is not implemented"));
        return;
    }

    auto tabResult = getTabFromIdentifiers(std::nullopt, std::nullopt);
    if (!tabResult) {
        completionHandler(toWebExtensionError(apiName, nullString(), tabResult.error()));
        return;
    }

    Ref tab = WTF::move(tabResult.value());

    auto maybeSidebar = sidebarForTab(tab.get());
    if (!maybeSidebar) {
        completionHandler(toWebExtensionError(apiName, nullString(), unknownErrorString));
        return;
    }

    Ref sidebar = WTF::move(maybeSidebar.value());

    if (!sidebar->opensSidebar()) {
        completionHandler(toWebExtensionError(apiName, nullString(), @"no sidebar panel is set"));
        return;
    }

    if (sidebar->isOpen())
        closeSidebar(sidebar.get());
    else
        openSidebar(sidebar.get());

    completionHandler({ });
}

void WebExtensionContext::sidebarSetIcon(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, const String& iconJSON, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    // FIXME: <https://webkit.org/b/276833> implement icon-related methods
}

void WebExtensionContext::sidebarGetTitle(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&& completionHandler)
{
    // this method services a sidebarAction-only API method
    static NSString * const apiName = @"sidebarAction.getTitle()";

    auto sidebar = getSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!sidebar) {
        completionHandler(toWebExtensionError(apiName, @"details", sidebar.error()));
        return;
    }

    completionHandler(sidebar.value()->title());
}

void WebExtensionContext::sidebarSetTitle(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, const std::optional<String>& title, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    // this method services a sidebarAction-only API method
    static NSString * const apiName = @"sidebarAction.setTitle()";

    // A clear on a tab which has no sidebar of its own has nothing to change.
    if (tabIdentifier && !title) {
        if (RefPtr tab = getTab(*tabIdentifier); tab && !getSidebar(*tab)) {
            completionHandler({ });
            return;
        }
    }

    auto sidebar = getOrCreateSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!sidebar) {
        completionHandler(toWebExtensionError(apiName, @"details", sidebar.error()));
        return;
    }
    sidebar.value()->setTitle(title);

    completionHandler({ });
}

void WebExtensionContext::sidebarGetOptions(const std::optional<WebExtensionWindowIdentifier> windowIdentifier, const std::optional<WebExtensionTabIdentifier> tabIdentifier, CompletionHandler<void(Expected<WebExtensionSidebarParameters, WebExtensionError>&&)>&& completionHandler)
{
    NSString *apiName;
    NSString *objectName;
    if (extension().hasSidePanel()) {
        apiName = @"sidePanel.getOptions()";
        objectName = @"options";
    } else {
        apiName = @"sidebarAction.getPanel()";
        objectName = @"details";
    }

    auto maybeSidebar = getSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!maybeSidebar) {
        completionHandler(toWebExtensionError(apiName, objectName, maybeSidebar.error()));
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
    NSString *apiName;
    NSString *objectName;
    if (extension().hasSidePanel()) {
        apiName = @"sidePanel.setOptions()";
        objectName = @"options";
    } else {
        apiName = @"sidebarAction.setPanel()";
        objectName = @"details";
    }

    // A clear-only call (empty path and no enabled) on a tab which has no sidebar of its own has nothing to change.
    if (tabIdentifier && !panelSourcePath && !enabled) {
        if (RefPtr tab = getTab(*tabIdentifier); tab && !getSidebar(*tab)) {
            completionHandler({ });
            return;
        }
    }

    auto maybeSidebar = getOrCreateSidebarWithIdentifiers(windowIdentifier, tabIdentifier, *this);
    if (!maybeSidebar) {
        completionHandler(toWebExtensionError(apiName, objectName, maybeSidebar.error()));
        return;
    }

    auto& sidebar = maybeSidebar.value().get();
    sidebar.setOptions(panelSourcePath, enabled);
    completionHandler({ });
}

void WebExtensionContext::sidebarSetActionClickBehavior(WebExtensionActionClickBehavior behavior, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    m_actionClickBehavior = behavior;
    completionHandler({ });
}

void WebExtensionContext::sidebarGetActionClickBehavior(CompletionHandler<void(Expected<WebExtensionActionClickBehavior, WebExtensionError>&&)>&& completionHandler)
{
    completionHandler(m_actionClickBehavior);
}

bool WebExtensionContext::isSidebarMessageAllowed(IPC::Decoder& message)
{
    if (RefPtr controller = extensionController())
        return isLoadedAndPrivilegedMessage(message) && controller->isFeatureEnabled(@"WebExtensionSidebarEnabled");
    return false;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
