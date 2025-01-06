/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#include "WebExtensionContentWorldType.h"

namespace WebKit {

// If you are adding a new event, you will also need to increase 'currentBackgroundContentListenerStateVersion'
// so that your new event gets fired to non-persistent background content.
enum class WebExtensionEventListenerType : uint8_t {
    Unknown = 0,
    ActionOnClicked,
    AlarmsOnAlarm,
    CommandsOnChanged,
    CommandsOnCommand,
    CookiesOnChanged,
    DevToolsElementsPanelOnSelectionChanged,
    DevToolsExtensionPanelOnHidden,
    DevToolsExtensionPanelOnSearch,
    DevToolsExtensionPanelOnShown,
    DevToolsExtensionSidebarPaneOnHidden,
    DevToolsExtensionSidebarPaneOnShown,
    DevToolsInspectedWindowOnResourceAdded,
    DevToolsNetworkOnNavigated,
    DevToolsNetworkOnRequestFinished,
    DevToolsPanelsOnThemeChanged,
    DownloadsOnChanged,
    DownloadsOnCreated,
    MenusOnClicked,
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
    TestOnMessage,
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
};

inline String toAPIString(WebExtensionEventListenerType eventType)
{
    switch (eventType) {
    case WebExtensionEventListenerType::Unknown:
        return nullString();
    case WebExtensionEventListenerType::ActionOnClicked:
        return "onClicked"_s;
    case WebExtensionEventListenerType::AlarmsOnAlarm:
        return "onAlarm"_s;
    case WebExtensionEventListenerType::CommandsOnChanged:
        return "onChanged"_s;
    case WebExtensionEventListenerType::CommandsOnCommand:
        return "onCommand"_s;
    case WebExtensionEventListenerType::CookiesOnChanged:
        return "onChanged"_s;
    case WebExtensionEventListenerType::DevToolsElementsPanelOnSelectionChanged:
        return "onSelectionChanged"_s;
    case WebExtensionEventListenerType::DevToolsExtensionPanelOnHidden:
        return "onHidden"_s;
    case WebExtensionEventListenerType::DevToolsExtensionPanelOnSearch:
        return "onSearch"_s;
    case WebExtensionEventListenerType::DevToolsExtensionPanelOnShown:
        return "onShown"_s;
    case WebExtensionEventListenerType::DevToolsExtensionSidebarPaneOnHidden:
        return "onHidden"_s;
    case WebExtensionEventListenerType::DevToolsExtensionSidebarPaneOnShown:
        return "onShown"_s;
    case WebExtensionEventListenerType::DevToolsInspectedWindowOnResourceAdded:
        return "onResourceAdded"_s;
    case WebExtensionEventListenerType::DevToolsNetworkOnNavigated:
        return "onNavigated"_s;
    case WebExtensionEventListenerType::DevToolsNetworkOnRequestFinished:
        return "onRequestFinished"_s;
    case WebExtensionEventListenerType::DevToolsPanelsOnThemeChanged:
        return "onThemeChanged"_s;
    case WebExtensionEventListenerType::DownloadsOnChanged:
        return "onChanged"_s;
    case WebExtensionEventListenerType::DownloadsOnCreated:
        return "onCreated"_s;
    case WebExtensionEventListenerType::MenusOnClicked:
        return "onClicked"_s;
    case WebExtensionEventListenerType::NotificationsOnButtonClicked:
        return "onButtonClicked"_s;
    case WebExtensionEventListenerType::NotificationsOnClicked:
        return "onClicked"_s;
    case WebExtensionEventListenerType::PermissionsOnAdded:
        return "onAdded"_s;
    case WebExtensionEventListenerType::PermissionsOnRemoved:
        return "onRemoved"_s;
    case WebExtensionEventListenerType::PortOnDisconnect:
        return "onDisconnect"_s;
    case WebExtensionEventListenerType::PortOnMessage:
        return "onMessage"_s;
    case WebExtensionEventListenerType::RuntimeOnConnect:
        return "onConnect"_s;
    case WebExtensionEventListenerType::RuntimeOnConnectExternal:
        return "onConnectExternal"_s;
    case WebExtensionEventListenerType::RuntimeOnInstalled:
        return "onInstalled"_s;
    case WebExtensionEventListenerType::RuntimeOnMessage:
        return "onMessage"_s;
    case WebExtensionEventListenerType::RuntimeOnMessageExternal:
        return "onMessageExternal"_s;
    case WebExtensionEventListenerType::RuntimeOnStartup:
        return "onStartup"_s;
    case WebExtensionEventListenerType::StorageOnChanged:
        return "onChanged"_s;
    case WebExtensionEventListenerType::TabsOnActivated:
        return "onActivated"_s;
    case WebExtensionEventListenerType::TabsOnAttached:
        return "onAttached"_s;
    case WebExtensionEventListenerType::TabsOnCreated:
        return "onCreated"_s;
    case WebExtensionEventListenerType::TabsOnDetached:
        return "onDetached"_s;
    case WebExtensionEventListenerType::TabsOnHighlighted:
        return "onHighlighted"_s;
    case WebExtensionEventListenerType::TabsOnMoved:
        return "onMoved"_s;
    case WebExtensionEventListenerType::TabsOnRemoved:
        return "onRemoved"_s;
    case WebExtensionEventListenerType::TabsOnReplaced:
        return "onReplaced"_s;
    case WebExtensionEventListenerType::TabsOnUpdated:
        return "onUpdated"_s;
    case WebExtensionEventListenerType::TestOnMessage:
        return "onMessage"_s;
    case WebExtensionEventListenerType::WebNavigationOnBeforeNavigate:
        return "onBeforeNavigate"_s;
    case WebExtensionEventListenerType::WebNavigationOnCommitted:
        return "onCommitted"_s;
    case WebExtensionEventListenerType::WebNavigationOnCompleted:
        return "onCompleted"_s;
    case WebExtensionEventListenerType::WebNavigationOnDOMContentLoaded:
        return "onDOMContentLoaded"_s;
    case WebExtensionEventListenerType::WebNavigationOnErrorOccurred:
        return "onErrorOccurred"_s;
    case WebExtensionEventListenerType::WebRequestOnAuthRequired:
        return "onAuthRequired"_s;
    case WebExtensionEventListenerType::WebRequestOnBeforeRedirect:
        return "onBeforeRedirect"_s;
    case WebExtensionEventListenerType::WebRequestOnBeforeRequest:
        return "onBeforeRequest"_s;
    case WebExtensionEventListenerType::WebRequestOnBeforeSendHeaders:
        return "onBeforeSendHeaders"_s;
    case WebExtensionEventListenerType::WebRequestOnCompleted:
        return "onCompleted"_s;
    case WebExtensionEventListenerType::WebRequestOnErrorOccurred:
        return "onErrorOccurred"_s;
    case WebExtensionEventListenerType::WebRequestOnHeadersReceived:
        return "onHeadersReceived"_s;
    case WebExtensionEventListenerType::WebRequestOnResponseStarted:
        return "onResponseStarted"_s;
    case WebExtensionEventListenerType::WebRequestOnSendHeaders:
        return "onSendHeaders"_s;
    case WebExtensionEventListenerType::WindowsOnCreated:
        return "onCreated"_s;
    case WebExtensionEventListenerType::WindowsOnFocusChanged:
        return "onFocusChanged"_s;
    case WebExtensionEventListenerType::WindowsOnRemoved:
        return "onRemoved"_s;
    }
}

using WebExtensionEventListenerTypeWorldPair = std::pair<WebExtensionEventListenerType, WebExtensionContentWorldType>;

} // namespace WebKit

namespace WTF {

template<> struct DefaultHash<WebKit::WebExtensionEventListenerType> : IntHash<WebKit::WebExtensionEventListenerType> { };
template<> struct HashTraits<WebKit::WebExtensionEventListenerType> : StrongEnumHashTraits<WebKit::WebExtensionEventListenerType> { };

} // namespace WTF

#endif // ENABLE(WK_WEB_EXTENSIONS)
