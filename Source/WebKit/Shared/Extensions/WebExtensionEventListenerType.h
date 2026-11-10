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
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
    DeclarativeNetRequestOnRuleMatchedDebug,
#endif
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
    NotificationsOnClosed,
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
    TestOnTestStarted,
    TestOnTestFinished,
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

inline RefPtr<StringImpl> toAPIString(WebExtensionEventListenerType eventType)
{
    switch (eventType) {
    case WebExtensionEventListenerType::Unknown:
        return nullString();
    case WebExtensionEventListenerType::ActionOnClicked:
        return "onClicked"_si;
    case WebExtensionEventListenerType::AlarmsOnAlarm:
        return "onAlarm"_si;
    case WebExtensionEventListenerType::CommandsOnChanged:
        return "onChanged"_si;
    case WebExtensionEventListenerType::CommandsOnCommand:
        return "onCommand"_si;
    case WebExtensionEventListenerType::CookiesOnChanged:
        return "onChanged"_si;
#if ENABLE(DNR_ON_RULE_MATCHED_DEBUG)
    case WebExtensionEventListenerType::DeclarativeNetRequestOnRuleMatchedDebug:
        return "onRuleMatchedDebug"_s;
#endif
    case WebExtensionEventListenerType::DevToolsElementsPanelOnSelectionChanged:
        return "onSelectionChanged"_si;
    case WebExtensionEventListenerType::DevToolsExtensionPanelOnHidden:
        return "onHidden"_si;
    case WebExtensionEventListenerType::DevToolsExtensionPanelOnSearch:
        return "onSearch"_si;
    case WebExtensionEventListenerType::DevToolsExtensionPanelOnShown:
        return "onShown"_si;
    case WebExtensionEventListenerType::DevToolsExtensionSidebarPaneOnHidden:
        return "onHidden"_si;
    case WebExtensionEventListenerType::DevToolsExtensionSidebarPaneOnShown:
        return "onShown"_si;
    case WebExtensionEventListenerType::DevToolsInspectedWindowOnResourceAdded:
        return "onResourceAdded"_si;
    case WebExtensionEventListenerType::DevToolsNetworkOnNavigated:
        return "onNavigated"_si;
    case WebExtensionEventListenerType::DevToolsNetworkOnRequestFinished:
        return "onRequestFinished"_si;
    case WebExtensionEventListenerType::DevToolsPanelsOnThemeChanged:
        return "onThemeChanged"_si;
    case WebExtensionEventListenerType::DownloadsOnChanged:
        return "onChanged"_si;
    case WebExtensionEventListenerType::DownloadsOnCreated:
        return "onCreated"_si;
    case WebExtensionEventListenerType::MenusOnClicked:
        return "onClicked"_si;
    case WebExtensionEventListenerType::NotificationsOnButtonClicked:
        return "onButtonClicked"_si;
    case WebExtensionEventListenerType::NotificationsOnClicked:
        return "onClicked"_si;
    case WebExtensionEventListenerType::NotificationsOnClosed:
        return "onClosed"_si;
    case WebExtensionEventListenerType::PermissionsOnAdded:
        return "onAdded"_si;
    case WebExtensionEventListenerType::PermissionsOnRemoved:
        return "onRemoved"_si;
    case WebExtensionEventListenerType::PortOnDisconnect:
        return "onDisconnect"_si;
    case WebExtensionEventListenerType::PortOnMessage:
        return "onMessage"_si;
    case WebExtensionEventListenerType::RuntimeOnConnect:
        return "onConnect"_si;
    case WebExtensionEventListenerType::RuntimeOnConnectExternal:
        return "onConnectExternal"_si;
    case WebExtensionEventListenerType::RuntimeOnInstalled:
        return "onInstalled"_si;
    case WebExtensionEventListenerType::RuntimeOnMessage:
        return "onMessage"_si;
    case WebExtensionEventListenerType::RuntimeOnMessageExternal:
        return "onMessageExternal"_si;
    case WebExtensionEventListenerType::RuntimeOnStartup:
        return "onStartup"_si;
    case WebExtensionEventListenerType::StorageOnChanged:
        return "onChanged"_si;
    case WebExtensionEventListenerType::TabsOnActivated:
        return "onActivated"_si;
    case WebExtensionEventListenerType::TabsOnAttached:
        return "onAttached"_si;
    case WebExtensionEventListenerType::TabsOnCreated:
        return "onCreated"_si;
    case WebExtensionEventListenerType::TabsOnDetached:
        return "onDetached"_si;
    case WebExtensionEventListenerType::TabsOnHighlighted:
        return "onHighlighted"_si;
    case WebExtensionEventListenerType::TabsOnMoved:
        return "onMoved"_si;
    case WebExtensionEventListenerType::TabsOnRemoved:
        return "onRemoved"_si;
    case WebExtensionEventListenerType::TabsOnReplaced:
        return "onReplaced"_si;
    case WebExtensionEventListenerType::TabsOnUpdated:
        return "onUpdated"_si;
    case WebExtensionEventListenerType::TestOnMessage:
        return "onMessage"_si;
    case WebExtensionEventListenerType::TestOnTestStarted:
        return "onTestStarted"_si;
    case WebExtensionEventListenerType::TestOnTestFinished:
        return "onTestFinished"_si;
    case WebExtensionEventListenerType::WebNavigationOnBeforeNavigate:
        return "onBeforeNavigate"_si;
    case WebExtensionEventListenerType::WebNavigationOnCommitted:
        return "onCommitted"_si;
    case WebExtensionEventListenerType::WebNavigationOnCompleted:
        return "onCompleted"_si;
    case WebExtensionEventListenerType::WebNavigationOnDOMContentLoaded:
        return "onDOMContentLoaded"_si;
    case WebExtensionEventListenerType::WebNavigationOnErrorOccurred:
        return "onErrorOccurred"_si;
    case WebExtensionEventListenerType::WebRequestOnAuthRequired:
        return "onAuthRequired"_si;
    case WebExtensionEventListenerType::WebRequestOnBeforeRedirect:
        return "onBeforeRedirect"_si;
    case WebExtensionEventListenerType::WebRequestOnBeforeRequest:
        return "onBeforeRequest"_si;
    case WebExtensionEventListenerType::WebRequestOnBeforeSendHeaders:
        return "onBeforeSendHeaders"_si;
    case WebExtensionEventListenerType::WebRequestOnCompleted:
        return "onCompleted"_si;
    case WebExtensionEventListenerType::WebRequestOnErrorOccurred:
        return "onErrorOccurred"_si;
    case WebExtensionEventListenerType::WebRequestOnHeadersReceived:
        return "onHeadersReceived"_si;
    case WebExtensionEventListenerType::WebRequestOnResponseStarted:
        return "onResponseStarted"_si;
    case WebExtensionEventListenerType::WebRequestOnSendHeaders:
        return "onSendHeaders"_si;
    case WebExtensionEventListenerType::WindowsOnCreated:
        return "onCreated"_si;
    case WebExtensionEventListenerType::WindowsOnFocusChanged:
        return "onFocusChanged"_si;
    case WebExtensionEventListenerType::WindowsOnRemoved:
        return "onRemoved"_si;
    }

    ASSERT_NOT_REACHED();
    return emptyString();
}

using WebExtensionEventListenerTypeWorldPair = std::pair<WebExtensionEventListenerType, WebExtensionContentWorldType>;

} // namespace WebKit

namespace WTF {

template<> struct DefaultHash<WebKit::WebExtensionEventListenerType> : IntHash<WebKit::WebExtensionEventListenerType> { };
template<> struct HashTraits<WebKit::WebExtensionEventListenerType> : StrongEnumHashTraits<WebKit::WebExtensionEventListenerType> { };

} // namespace WTF

#endif // ENABLE(WK_WEB_EXTENSIONS)
