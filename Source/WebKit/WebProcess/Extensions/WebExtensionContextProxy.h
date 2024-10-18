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

#include "MessageReceiver.h"
#include "WebExtensionContext.h"
#include "WebExtensionContextParameters.h"
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS JSValue;
OBJC_CLASS NSDictionary;
OBJC_CLASS _WKWebExtensionLocalization;

namespace WebKit {

class WebExtensionAPINamespace;
class WebExtensionAPIStorage;
class WebExtensionControllerProxy;
class WebExtensionMatchPattern;
class WebFrame;

struct WebExtensionAlarmParameters;
struct WebExtensionTabParameters;
struct WebExtensionWindowParameters;

class WebExtensionContextProxy final : public RefCounted<WebExtensionContextProxy>, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(WebExtensionContextProxy);
    WTF_MAKE_NONCOPYABLE(WebExtensionContextProxy);

public:
    static RefPtr<WebExtensionContextProxy> get(WebExtensionContextIdentifier);
    static Ref<WebExtensionContextProxy> getOrCreate(const WebExtensionContextParameters&, WebExtensionControllerProxy&, WebPage* = nullptr);

    ~WebExtensionContextProxy();

    using WeakFrameSet = WeakHashSet<WebFrame>;
    using TabWindowIdentifierPair = std::pair<std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>>;
    using WeakPageTabWindowMap = WeakHashMap<WebPage, TabWindowIdentifierPair>;
    using PermissionsMap = WebExtensionContext::PermissionsMap;

    WebExtensionContextIdentifier identifier() const { return m_identifier; }
    WebExtensionControllerProxy* extensionControllerProxy() const;

    bool operator==(const WebExtensionContextProxy& other) const { return (this == &other); }

    const URL& baseURL() const { return m_baseURL; }
    const String& uniqueIdentifier() const { return m_uniqueIdentifier; }

    NSDictionary *manifest() const { return m_manifest.get(); }

    double manifestVersion() const { return m_manifestVersion; }
    bool supportsManifestVersion(double version) const { return manifestVersion() >= version; }

    _WKWebExtensionLocalization *localization() const { return m_localization.get(); }

    bool isSessionStorageAllowedInContentScripts() const { return m_isSessionStorageAllowedInContentScripts; }

    bool inTestingMode() const;

    bool hasDOMWrapperWorld(WebExtensionContentWorldType contentWorldType) const { return contentWorldType != WebExtensionContentWorldType::ContentScript || hasContentScriptWorld(); }
    WebCore::DOMWrapperWorld& toDOMWrapperWorld(WebExtensionContentWorldType) const;

    static WebCore::DOMWrapperWorld& mainWorldSingleton() { return WebCore::mainThreadNormalWorld(); }

    bool hasContentScriptWorld() const { return !!m_contentScriptWorld; }
    WebCore::DOMWrapperWorld& contentScriptWorld() const { RELEASE_ASSERT(hasContentScriptWorld()); return *m_contentScriptWorld; }
    void setContentScriptWorld(WebCore::DOMWrapperWorld* world) { m_contentScriptWorld = world; }

    void addFrameWithExtensionContent(WebFrame&);

    std::optional<WebExtensionTabIdentifier> tabIdentifier(WebPage&) const;

    RefPtr<WebPage> backgroundPage() const;
    void setBackgroundPage(WebPage&);

    bool isUnsupportedAPI(const String& propertyPath, const ASCIILiteral& propertyName) const;

    bool hasPermission(const String& permission) const;

#if ENABLE(INSPECTOR_EXTENSIONS)
    void addInspectorPage(WebPage&, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>);

    void addInspectorBackgroundPage(WebPage&, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>);
    bool isInspectorBackgroundPage(WebPage&) const;

    Inspector::ExtensionAppearance inspectorAppearance() const { return m_inspectorAppearance; }
    void setInspectorAppearance(Inspector::ExtensionAppearance appearance) { m_inspectorAppearance = appearance; }
#endif

    Vector<Ref<WebPage>> popupPages(std::optional<WebExtensionTabIdentifier> = std::nullopt, std::optional<WebExtensionWindowIdentifier> = std::nullopt) const;
    void addPopupPage(WebPage&, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>);

    Vector<Ref<WebPage>> tabPages(std::optional<WebExtensionTabIdentifier> = std::nullopt, std::optional<WebExtensionWindowIdentifier> = std::nullopt) const;
    void addTabPage(WebPage&, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>);

    void enumerateFramesAndNamespaceObjects(const Function<void(WebFrame&, WebExtensionAPINamespace&)>&, WebCore::DOMWrapperWorld& = mainWorldSingleton());

    void enumerateNamespaceObjects(const Function<void(WebExtensionAPINamespace&)>& function)
    {
        enumerateFramesAndNamespaceObjects([&](auto&, auto& namespaceObject) {
            function(namespaceObject);
        });
    }

private:
    explicit WebExtensionContextProxy(const WebExtensionContextParameters&);

    static _WKWebExtensionLocalization *parseLocalization(API::Data&, const URL& baseURL);

    // Action
    void dispatchActionClickedEvent(const std::optional<WebExtensionTabParameters>&);

    // Alarms
    void dispatchAlarmsEvent(const WebExtensionAlarmParameters&);

    // Commands
    void dispatchCommandsCommandEvent(const String& identifier, const std::optional<WebExtensionTabParameters>&);
    void dispatchCommandsChangedEvent(const String& identifier, const String& oldShortcut, const String& newShortcut);

    // Cookies
    void dispatchCookiesChangedEvent();

#if ENABLE(INSPECTOR_EXTENSIONS)
    // DevTools
    void addInspectorPageIdentifier(WebCore::PageIdentifier, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>);
    void addInspectorBackgroundPageIdentifier(WebCore::PageIdentifier, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>);
    void dispatchDevToolsExtensionPanelShownEvent(Inspector::ExtensionTabID, WebCore::FrameIdentifier);
    void dispatchDevToolsExtensionPanelHiddenEvent(Inspector::ExtensionTabID);
    void dispatchDevToolsNetworkNavigatedEvent(const URL&);
    void dispatchDevToolsPanelsThemeChangedEvent(Inspector::ExtensionAppearance);
#endif

    // Extension
    void setBackgroundPageIdentifier(WebCore::PageIdentifier);
    void addPopupPageIdentifier(WebCore::PageIdentifier, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>);
    void addTabPageIdentifier(WebCore::PageIdentifier, WebExtensionTabIdentifier, std::optional<WebExtensionWindowIdentifier>);

    // Menus
    void dispatchMenusClickedEvent(const WebExtensionMenuItemParameters&, bool wasChecked, const WebExtensionMenuItemContextParameters&, const std::optional<WebExtensionTabParameters>&);

    // Permissions
    void updateGrantedPermissions(PermissionsMap&&);
    void dispatchPermissionsEvent(WebExtensionEventListenerType, HashSet<String> permissions, HashSet<String> origins);

    // Port
    void dispatchPortMessageEvent(std::optional<WebPageProxyIdentifier>, WebExtensionPortChannelIdentifier, const String& messageJSON);
    void dispatchPortDisconnectEvent(WebExtensionPortChannelIdentifier);

    // Runtime
    void internalDispatchRuntimeMessageEvent(WebExtensionContentWorldType, const String& messageJSON, const std::optional<WebExtensionMessageTargetParameters>&, const WebExtensionMessageSenderParameters&, CompletionHandler<void(String&& replyJSON)>&&);
    void internalDispatchRuntimeConnectEvent(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier, const String& name, const std::optional<WebExtensionMessageTargetParameters>&, const WebExtensionMessageSenderParameters&, CompletionHandler<void(HashCountedSet<WebPageProxyIdentifier>&&)>&&);
    void dispatchRuntimeMessageEvent(WebExtensionContentWorldType, const String& messageJSON, const std::optional<WebExtensionMessageTargetParameters>&, const WebExtensionMessageSenderParameters&, CompletionHandler<void(String&& replyJSON)>&&);
    void dispatchRuntimeConnectEvent(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier, const String& name, const std::optional<WebExtensionMessageTargetParameters>&, const WebExtensionMessageSenderParameters&, CompletionHandler<void(HashCountedSet<WebPageProxyIdentifier>&&)>&&);
    void dispatchRuntimeInstalledEvent(WebExtensionContext::InstallReason, String previousVersion);
    void dispatchRuntimeStartupEvent();

    // Storage
    void setStorageAccessLevel(bool);
    void dispatchStorageChangedEvent(const String& onChangedJSON, WebExtensionDataType, WebExtensionContentWorldType);

    // Tabs
    void dispatchTabsCreatedEvent(const WebExtensionTabParameters&);
    void dispatchTabsUpdatedEvent(const WebExtensionTabParameters&, const WebExtensionTabParameters& changedParameters);
    void dispatchTabsReplacedEvent(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier);
    void dispatchTabsDetachedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, size_t oldIndex);
    void dispatchTabsMovedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, size_t oldIndex, size_t newIndex);
    void dispatchTabsAttachedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, size_t newIndex);
    void dispatchTabsActivatedEvent(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier);
    void dispatchTabsHighlightedEvent(const Vector<WebExtensionTabIdentifier>&, WebExtensionWindowIdentifier);
    void dispatchTabsRemovedEvent(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, WebExtensionContext::WindowIsClosing);

    // Web Navigation
    void dispatchWebNavigationEvent(WebExtensionEventListenerType, WebExtensionTabIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);

    // Web Request
    void resourceLoadDidSendRequest(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceRequest&, const ResourceLoadInfo&);
    void resourceLoadDidPerformHTTPRedirection(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const ResourceLoadInfo&, const WebCore::ResourceRequest& newRequest);
    void resourceLoadDidReceiveChallenge(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::AuthenticationChallenge&, const ResourceLoadInfo&);
    void resourceLoadDidReceiveResponse(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const ResourceLoadInfo&);
    void resourceLoadDidCompleteWithError(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, const WebCore::ResourceResponse&, const WebCore::ResourceError&, const ResourceLoadInfo&);

    // Windows
    void dispatchWindowsEvent(WebExtensionEventListenerType, const std::optional<WebExtensionWindowParameters>&);

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    WebExtensionContextIdentifier m_identifier;
    WeakPtr<WebExtensionControllerProxy> m_extensionControllerProxy;
    URL m_baseURL;
    String m_uniqueIdentifier;
    HashSet<String> m_unsupportedAPIs;
    RetainPtr<_WKWebExtensionLocalization> m_localization;
    RetainPtr<NSDictionary> m_manifest;
    double m_manifestVersion { 0 };
    bool m_isSessionStorageAllowedInContentScripts { false };
    mutable PermissionsMap m_grantedPermissions;
    mutable WallTime m_nextGrantedPermissionsExpirationDate { WallTime::nan() };
    RefPtr<WebCore::DOMWrapperWorld> m_contentScriptWorld;
    WeakFrameSet m_extensionContentFrames;
    WeakPtr<WebPage> m_backgroundPage;
#if ENABLE(INSPECTOR_EXTENSIONS)
    WeakPageTabWindowMap m_inspectorPageMap;
    WeakPageTabWindowMap m_inspectorBackgroundPageMap;
    Inspector::ExtensionAppearance m_inspectorAppearance { Inspector::ExtensionAppearance::Light };
#endif
    WeakPageTabWindowMap m_popupPageMap;
    WeakPageTabWindowMap m_tabPageMap;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
