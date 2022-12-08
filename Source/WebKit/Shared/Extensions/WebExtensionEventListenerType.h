/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

// If you are adding a new event, you will also need to increase 'currentBackgroundPageListenerStateVersion'
// so that your new event gets fired to non-persistent background pages.
enum class WebExtensionEventListenerType : uint8_t {
    Unknown = 0,
    ActionOnClicked,
    AlarmsOnAlarm,
    CommandsOnCommand,
    ContextMenusOnClicked,
    CookiesOnChanged,
    DevToolsElementsPanelOnSelectionChanged,
    DevToolsExtensionPanelOnShown,
    DevToolsExtensionPanelOnHidden,
    DevToolsExtensionSidebarPaneOnShown,
    DevToolsExtensionSidebarPaneOnHidden,
    DevToolsExtensionPanelOnSearch,
    DevToolsNetworkOnNavigated,
    DevToolsNetworkOnRequestFinished,
    DevToolsPanelsOnThemeChanged,
    NotificationsOnButtonClicked,
    NotificationsOnClicked,
    PermissionsOnAdded,
    PermissionsOnRemoved,
    PortOnDisconnect,
    PortOnMessage,
    RuntimeOnConnect,
    RuntimeOnConnectExternal,
    RuntimeOnInstalled,
    RuntimeOnMessage,
    RuntimeOnMessageExternal,
    RuntimeOnStartup,
    StorageOnChanged,
    TabsOnActivated,
    TabsOnAttached,
    TabsOnCreated,
    TabsOnDetached,
    TabsOnHighlighted,
    TabsOnMoved,
    TabsOnRemoved,
    TabsOnReplaced,
    TabsOnUpdated,
    WebNavigationOnBeforeNavigate,
    WebNavigationOnCommitted,
    WebNavigationOnCompleted,
    WebNavigationOnDOMContentLoaded,
    WebNavigationOnErrorOccurred,
    WebRequestOnAuthRequired,
    WebRequestOnBeforeRedirect,
    WebRequestOnBeforeRequest,
    WebRequestOnBeforeSendHeaders,
    WebRequestOnCompleted,
    WebRequestOnErrorOccurred,
    WebRequestOnHeadersReceived,
    WebRequestOnResponseStarted,
    WebRequestOnSendHeaders,
    WindowsOnCreated,
    WindowsOnFocusChanged,
    WindowsOnRemoved,
    DevToolsInspectedWindowOnResourceAdded,
    DownloadsOnCreated,
    DownloadsOnChanged,
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::WebExtensionEventListenerType> {
    using values = EnumValues<
    WebKit::WebExtensionEventListenerType,
    WebKit::WebExtensionEventListenerType::Unknown,
    WebKit::WebExtensionEventListenerType::ActionOnClicked,
    WebKit::WebExtensionEventListenerType::AlarmsOnAlarm,
    WebKit::WebExtensionEventListenerType::CommandsOnCommand,
    WebKit::WebExtensionEventListenerType::ContextMenusOnClicked,
    WebKit::WebExtensionEventListenerType::CookiesOnChanged,
    WebKit::WebExtensionEventListenerType::DevToolsElementsPanelOnSelectionChanged,
    WebKit::WebExtensionEventListenerType::DevToolsExtensionPanelOnShown,
    WebKit::WebExtensionEventListenerType::DevToolsExtensionPanelOnHidden,
    WebKit::WebExtensionEventListenerType::DevToolsExtensionSidebarPaneOnShown,
    WebKit::WebExtensionEventListenerType::DevToolsExtensionSidebarPaneOnHidden,
    WebKit::WebExtensionEventListenerType::DevToolsExtensionPanelOnSearch,
    WebKit::WebExtensionEventListenerType::DevToolsNetworkOnNavigated,
    WebKit::WebExtensionEventListenerType::DevToolsNetworkOnRequestFinished,
    WebKit::WebExtensionEventListenerType::DevToolsPanelsOnThemeChanged,
    WebKit::WebExtensionEventListenerType::NotificationsOnButtonClicked,
    WebKit::WebExtensionEventListenerType::NotificationsOnClicked,
    WebKit::WebExtensionEventListenerType::PermissionsOnAdded,
    WebKit::WebExtensionEventListenerType::PermissionsOnRemoved,
    WebKit::WebExtensionEventListenerType::PortOnDisconnect,
    WebKit::WebExtensionEventListenerType::PortOnMessage,
    WebKit::WebExtensionEventListenerType::RuntimeOnConnect,
    WebKit::WebExtensionEventListenerType::RuntimeOnConnectExternal,
    WebKit::WebExtensionEventListenerType::RuntimeOnInstalled,
    WebKit::WebExtensionEventListenerType::RuntimeOnMessage,
    WebKit::WebExtensionEventListenerType::RuntimeOnMessageExternal,
    WebKit::WebExtensionEventListenerType::RuntimeOnStartup,
    WebKit::WebExtensionEventListenerType::StorageOnChanged,
    WebKit::WebExtensionEventListenerType::TabsOnActivated,
    WebKit::WebExtensionEventListenerType::TabsOnAttached,
    WebKit::WebExtensionEventListenerType::TabsOnCreated,
    WebKit::WebExtensionEventListenerType::TabsOnDetached,
    WebKit::WebExtensionEventListenerType::TabsOnHighlighted,
    WebKit::WebExtensionEventListenerType::TabsOnMoved,
    WebKit::WebExtensionEventListenerType::TabsOnRemoved,
    WebKit::WebExtensionEventListenerType::TabsOnReplaced,
    WebKit::WebExtensionEventListenerType::TabsOnUpdated,
    WebKit::WebExtensionEventListenerType::WebNavigationOnBeforeNavigate,
    WebKit::WebExtensionEventListenerType::WebNavigationOnCommitted,
    WebKit::WebExtensionEventListenerType::WebNavigationOnCompleted,
    WebKit::WebExtensionEventListenerType::WebNavigationOnDOMContentLoaded,
    WebKit::WebExtensionEventListenerType::WebNavigationOnErrorOccurred,
    WebKit::WebExtensionEventListenerType::WebRequestOnAuthRequired,
    WebKit::WebExtensionEventListenerType::WebRequestOnBeforeRedirect,
    WebKit::WebExtensionEventListenerType::WebRequestOnBeforeRequest,
    WebKit::WebExtensionEventListenerType::WebRequestOnBeforeSendHeaders,
    WebKit::WebExtensionEventListenerType::WebRequestOnCompleted,
    WebKit::WebExtensionEventListenerType::WebRequestOnErrorOccurred,
    WebKit::WebExtensionEventListenerType::WebRequestOnHeadersReceived,
    WebKit::WebExtensionEventListenerType::WebRequestOnResponseStarted,
    WebKit::WebExtensionEventListenerType::WebRequestOnSendHeaders,
    WebKit::WebExtensionEventListenerType::WindowsOnCreated,
    WebKit::WebExtensionEventListenerType::WindowsOnFocusChanged,
    WebKit::WebExtensionEventListenerType::WindowsOnRemoved,
    WebKit::WebExtensionEventListenerType::DevToolsInspectedWindowOnResourceAdded,
    WebKit::WebExtensionEventListenerType::DownloadsOnCreated,
    WebKit::WebExtensionEventListenerType::DownloadsOnChanged
    >;
};

} // namespace WTF

#endif // ENABLE(WK_WEB_EXTENSIONS)
