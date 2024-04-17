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

#include "APIHTTPCookieStore.h"
#include "APIObject.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "MessageReceiver.h"
#include "WebExtension.h"
#include "WebExtensionAction.h"
#include "WebExtensionAlarm.h"
#include "WebExtensionCommand.h"
#include "WebExtensionContextIdentifier.h"
#include "WebExtensionController.h"
#include "WebExtensionDataType.h"
#include "WebExtensionDynamicScripts.h"
#include "WebExtensionEventListenerType.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionFrameParameters.h"
#include "WebExtensionMatchPattern.h"
#include "WebExtensionMatchedRuleParameters.h"
#include "WebExtensionMenuItem.h"
#include "WebExtensionMessagePort.h"
#include "WebExtensionPortChannelIdentifier.h"
#include "WebExtensionTab.h"
#include "WebExtensionTabIdentifier.h"
#include "WebExtensionUtilities.h"
#include "WebExtensionWindow.h"
#include "WebExtensionWindowIdentifier.h"
#include "WebExtensionWindowParameters.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcessProxy.h"
#include <WebCore/ContentRuleListResults.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Identified.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/URLHash.h>
#include <wtf/UUID.h>
#include <wtf/WeakHashCountedSet.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "APIInspectorExtension.h"
#include "APIInspectorExtensionClient.h"
#include "InspectorExtensionTypes.h"
#include "WebInspectorUIProxy.h"
#endif

OBJC_CLASS NSArray;
OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSMapTable;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSNumber;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
OBJC_CLASS NSUUID;
OBJC_CLASS WKContentRuleListStore;
OBJC_CLASS WKNavigation;
OBJC_CLASS WKNavigationAction;
OBJC_CLASS WKWebView;
OBJC_CLASS WKWebViewConfiguration;
OBJC_CLASS _WKWebExtensionContext;
OBJC_CLASS _WKWebExtensionContextDelegate;
OBJC_CLASS _WKWebExtensionDeclarativeNetRequestSQLiteStore;
OBJC_CLASS _WKWebExtensionRegisteredScriptsSQLiteStore;
OBJC_CLASS _WKWebExtensionStorageSQLiteStore;
OBJC_PROTOCOL(_WKWebExtensionTab);
OBJC_PROTOCOL(_WKWebExtensionWindow);

#if PLATFORM(MAC)
OBJC_CLASS NSEvent;
OBJC_CLASS NSMenu;
#endif

namespace PAL {
class SessionID;
}

namespace WebKit {

class ContextMenuContextData;
class WebExtension;
class WebUserContentControllerProxy;
struct WebExtensionContextParameters;
struct WebExtensionCookieFilterParameters;
struct WebExtensionCookieParameters;
struct WebExtensionCookieStoreParameters;
struct WebExtensionMenuItemContextParameters;

enum class WebExtensionContextInstallReason : uint8_t {
    None,
    ExtensionInstall,
    ExtensionUpdate,
    BrowserUpdate,
};

class WebExtensionContext : public API::ObjectImpl<API::Object::Type::WebExtensionContext>, public IPC::MessageReceiver, public Identified<WebExtensionContextIdentifier> {
    WTF_MAKE_NONCOPYABLE(WebExtensionContext);

public:
    template<typename... Args>
    static Ref<WebExtensionContext> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionContext(std::forward<Args>(args)...));
    }

    static String plistFileName() { return "State.plist"_s; };
    static NSMutableDictionary *readStateFromPath(const String&);
    static bool readLastBaseURLFromState(const String& filePath, URL& outLastBaseURL);
    static bool readDisplayNameFromState(const String& filePath, String& outDisplayName);

    static WebExtensionContext* get(WebExtensionContextIdentifier);

    explicit WebExtensionContext(Ref<WebExtension>&&);

    using PermissionsMap = HashMap<String, WallTime>;
    using PermissionMatchPatternsMap = HashMap<Ref<WebExtensionMatchPattern>, WallTime>;

    using UserScriptVector = Vector<Ref<API::UserScript>>;
    using UserStyleSheetVector = Vector<Ref<API::UserStyleSheet>>;

    using AlarmInfoMap = HashMap<String, double>;

    using DynamicInjectedContentsMap = HashMap<String, WebExtension::InjectedContentData>;

    using PermissionsSet = WebExtension::PermissionsSet;
    using MatchPatternSet = WebExtension::MatchPatternSet;
    using InjectedContentData = WebExtension::InjectedContentData;
    using InjectedContentVector = WebExtension::InjectedContentVector;
    using URLSet = HashSet<URL>;
    using URLVector = Vector<URL>;

    using WeakPageCountedSet = WeakHashCountedSet<WebPageProxy>;
    using EventListenerTypeCountedSet = HashCountedSet<WebExtensionEventListenerType>;
    using EventListenerTypePageMap = HashMap<WebExtensionEventListenerTypeWorldPair, WeakPageCountedSet>;
    using EventListenerTypeSet = HashSet<WebExtensionEventListenerType>;
    using ContentWorldTypeSet = HashSet<WebExtensionContentWorldType>;
    using VoidCompletionHandlerVector = Vector<CompletionHandler<void()>>;

    using WindowIdentifierMap = HashMap<WebExtensionWindowIdentifier, Ref<WebExtensionWindow>>;
    using WindowIdentifierVector = Vector<WebExtensionWindowIdentifier>;
    using TabIdentifierMap = HashMap<WebExtensionTabIdentifier, Ref<WebExtensionTab>>;
    using PageTabIdentifierMap = WeakHashMap<WebPageProxy, WebExtensionTabIdentifier>;
    using PopupPageActionMap = WeakHashMap<WebPageProxy, Ref<WebExtensionAction>>;

    using WindowVector = Vector<Ref<WebExtensionWindow>>;
    using TabVector = Vector<Ref<WebExtensionTab>>;
    using TabSet = HashSet<Ref<WebExtensionTab>>;

    using PopulateTabs = WebExtensionWindow::PopulateTabs;
    using WindowTypeFilter = WebExtensionWindow::TypeFilter;

    using WebProcessProxySet = HashSet<Ref<WebProcessProxy>>;

    using PortWorldTuple = std::tuple<WebExtensionContentWorldType, WebExtensionContentWorldType, WebExtensionPortChannelIdentifier>;
    using PortWorldPair = std::pair<WebExtensionContentWorldType, WebExtensionPortChannelIdentifier>;
    using MessagePageProxyIdentifierPair = std::pair<String, std::optional<WebPageProxyIdentifier>>;
    using PortCountedSet = HashCountedSet<PortWorldPair>;
    using PortTupleCountedSet = HashCountedSet<PortWorldTuple>;
    using PageProxyIdentifierPortMap = HashMap<WebPageProxyIdentifier, PortTupleCountedSet>;
    using PortQueuedMessageMap = HashMap<PortWorldPair, Vector<MessagePageProxyIdentifierPair>>;
    using NativePortMap = HashMap<WebExtensionPortChannelIdentifier, Ref<WebExtensionMessagePort>>;

    using PageIdentifierTuple = std::tuple<WebCore::PageIdentifier, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>>;

    using CommandsVector = Vector<Ref<WebExtensionCommand>>;

    using MenuItemVector = Vector<Ref<WebExtensionMenuItem>>;
    using MenuItemMap = HashMap<String, Ref<WebExtensionMenuItem>>;

    using DeclarativeNetRequestValidatedRulesets = Expected<WebExtension::DeclarativeNetRequestRulesetVector, WebExtensionError>;
    using DeclarativeNetRequestMatchedRuleVector = Vector<WebExtensionMatchedRuleParameters>;

    using UserContentControllerProxySet = WeakHashSet<WebUserContentControllerProxy>;

#if ENABLE(INSPECTOR_EXTENSIONS)
    using InspectorTabVector = Vector<std::pair<Ref<WebInspectorUIProxy>, RefPtr<WebExtensionTab>>>;
    using TabIdentifierWebViewPair = std::pair<WebExtensionTabIdentifier, RetainPtr<WKWebView>>;
#endif

    enum class EqualityOnly : bool { No, Yes };
    enum class WindowIsClosing : bool { No, Yes };
    enum class ReloadFromOrigin : bool { No, Yes };
    enum class UserTriggered : bool { No, Yes };
    enum class SuppressEvents : bool { No, Yes };
    enum class UpdateWindowOrder : bool { No, Yes };
    enum class IgnoreExtensionAccess : bool { No, Yes };
    enum class IncludeExtensionViews : bool { No, Yes };
    enum class GrantOnCompletion : bool { No, Yes };

    enum class Error : uint8_t {
        Unknown = 1,
        AlreadyLoaded,
        NotLoaded,
        BaseURLAlreadyInUse,
    };

    enum class PermissionState : int8_t {
        DeniedExplicitly    = -3,
        DeniedImplicitly    = -2,
        RequestedImplicitly = -1,
        Unknown             = 0,
        RequestedExplicitly = 1,
        GrantedImplicitly   = 2,
        GrantedExplicitly   = 3,
    };

    enum class PermissionStateOptions : uint8_t {
        RequestedWithTabsPermission = 1 << 0, // Request access to a URL if the extension also has the "tabs" permission.
        SkipRequestedPermissions    = 1 << 1, // Don't check requested permissions.
        IncludeOptionalPermissions  = 1 << 2, // Check the optional permissions, and count them as RequestedImplicitly.
    };

    using InstallReason = WebExtensionContextInstallReason;

    enum class WebViewPurpose : uint8_t {
        Any,
        Background,
        Inspector,
        Popup,
        Tab,
    };

    WebExtensionContextParameters parameters() const;

    bool operator==(const WebExtensionContext& other) const { return (this == &other); }

    NSError *createError(Error, NSString *customLocalizedDescription = nil, NSError *underlyingError = nil);

    bool storageIsPersistent() const { return !m_storageDirectory.isEmpty(); }
    const String& storageDirectory() const { return m_storageDirectory; }

    void invalidateStorage();

    _WKWebExtensionStorageSQLiteStore *storageForType(WebExtensionDataType);

    bool load(WebExtensionController&, String storageDirectory, NSError ** = nullptr);
    bool unload(NSError ** = nullptr);
    bool reload(NSError ** = nullptr);

    bool isLoaded() const { return !!m_extensionController; }

    WebExtension& extension() const { return *m_extension; }
    WebExtensionController* extensionController() const { return m_extensionController.get(); }

    const URL& baseURL() const { return m_baseURL; }
    void setBaseURL(URL&&);

    bool isURLForThisExtension(const URL&) const;
    bool extensionCanAccessWebPage(WebPageProxyIdentifier);

    bool hasCustomUniqueIdentifier() const { return m_customUniqueIdentifier; }

    const String& uniqueIdentifier() const { return m_uniqueIdentifier; }
    void setUniqueIdentifier(String&&);

    bool isInspectable() const { return m_inspectable; }
    void setInspectable(bool);

    HashSet<String> unsupportedAPIs() const { return m_unsupportedAPIs; }
    void setUnsupportedAPIs(HashSet<String>&&);

    InjectedContentVector injectedContents() const;
    bool hasInjectedContentForURL(const URL&);
    bool hasInjectedContent();

    bool hasContentModificationRules();

    URL optionsPageURL() const;
    URL overrideNewTabPageURL() const;

    const PermissionsMap& grantedPermissions();
    void setGrantedPermissions(PermissionsMap&&);

    const PermissionsMap& deniedPermissions();
    void setDeniedPermissions(PermissionsMap&&);

    const PermissionMatchPatternsMap& grantedPermissionMatchPatterns();
    void setGrantedPermissionMatchPatterns(PermissionMatchPatternsMap&&);

    const PermissionMatchPatternsMap& deniedPermissionMatchPatterns();
    void setDeniedPermissionMatchPatterns(PermissionMatchPatternsMap&&);

    bool requestedOptionalAccessToAllHosts() const { return m_requestedOptionalAccessToAllHosts; }
    void setRequestedOptionalAccessToAllHosts(bool requested) { m_requestedOptionalAccessToAllHosts = requested; }

    bool hasAccessInPrivateBrowsing() const { return m_hasAccessInPrivateBrowsing; }
    void setHasAccessInPrivateBrowsing(bool);

    void grantPermissions(PermissionsSet&&, WallTime expirationDate = WallTime::infinity());
    void denyPermissions(PermissionsSet&&, WallTime expirationDate = WallTime::infinity());

    void grantPermissionMatchPatterns(MatchPatternSet&&, WallTime expirationDate = WallTime::infinity(), EqualityOnly = EqualityOnly::Yes);
    void denyPermissionMatchPatterns(MatchPatternSet&&, WallTime expirationDate = WallTime::infinity(), EqualityOnly = EqualityOnly::Yes);

    bool removeGrantedPermissions(PermissionsSet&);
    bool removeGrantedPermissionMatchPatterns(MatchPatternSet&, EqualityOnly = EqualityOnly::Yes);

    bool removeDeniedPermissions(PermissionsSet&);
    bool removeDeniedPermissionMatchPatterns(MatchPatternSet&, EqualityOnly = EqualityOnly::Yes);

    void requestPermissionMatchPatterns(const MatchPatternSet&, RefPtr<WebExtensionTab> = nullptr, CompletionHandler<void(MatchPatternSet&& neededMatchPatterns, MatchPatternSet&& allowedMatchPatterns, WallTime expirationDate)>&& = nullptr, GrantOnCompletion = GrantOnCompletion::Yes, OptionSet<PermissionStateOptions> = { });
    void requestPermissionToAccessURLs(const URLVector&, RefPtr<WebExtensionTab> = nullptr, CompletionHandler<void(URLSet&& neededURLs, URLSet&& allowedURLs, WallTime expirationDate)>&& = nullptr, GrantOnCompletion = GrantOnCompletion::Yes, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void requestPermissions(const PermissionsSet&, RefPtr<WebExtensionTab> = nullptr, CompletionHandler<void(PermissionsSet&& neededPermissions, PermissionsSet&& allowedPermissions, WallTime expirationDate)>&& = nullptr, GrantOnCompletion = GrantOnCompletion::Yes, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });

    PermissionsMap::KeysConstIteratorRange currentPermissions() { return grantedPermissions().keys(); }
    PermissionMatchPatternsMap::KeysConstIteratorRange currentPermissionMatchPatterns() { return grantedPermissionMatchPatterns().keys(); }

    bool hasAccessToAllURLs();
    bool hasAccessToAllHosts();

    bool needsPermission(const String&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { });
    bool needsPermission(const URL&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    bool needsPermission(const WebExtensionMatchPattern&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });

    bool hasPermission(const String& permission, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { });
    bool hasPermission(const URL&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    bool hasPermission(const WebExtensionMatchPattern&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });

    bool hasPermissions(PermissionsSet, MatchPatternSet);

    PermissionState permissionState(const String& permission, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { });
    void setPermissionState(PermissionState, const String& permission, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(const URL&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, const URL&, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(const WebExtensionMatchPattern&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, const WebExtensionMatchPattern&, WallTime expirationDate = WallTime::infinity());

    void clearCachedPermissionStates();

    void removePage(WebPageProxy&);

    Ref<WebExtensionWindow> getOrCreateWindow(_WKWebExtensionWindow *) const;
    RefPtr<WebExtensionWindow> getWindow(WebExtensionWindowIdentifier, std::optional<WebPageProxyIdentifier> = std::nullopt, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    void forgetWindow(WebExtensionWindowIdentifier) const;

    Ref<WebExtensionTab> getOrCreateTab(_WKWebExtensionTab *) const;
    RefPtr<WebExtensionTab> getTab(WebExtensionTabIdentifier, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    RefPtr<WebExtensionTab> getTab(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> = std::nullopt, IncludeExtensionViews = IncludeExtensionViews::No, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    RefPtr<WebExtensionTab> getCurrentTab(WebPageProxyIdentifier, IncludeExtensionViews = IncludeExtensionViews::Yes, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    void forgetTab(WebExtensionTabIdentifier) const;

    void openNewTab(const WebExtensionTabParameters&, CompletionHandler<void(RefPtr<WebExtensionTab>)>&&);

    WindowVector openWindows() const;
    TabVector openTabs() const;

    RefPtr<WebExtensionWindow> focusedWindow(IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    RefPtr<WebExtensionWindow> frontmostWindow(IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;

    bool isValidWindow(const WebExtensionWindow&);
    bool isValidTab(const WebExtensionTab&);

    void didOpenWindow(WebExtensionWindow&, UpdateWindowOrder = UpdateWindowOrder::Yes, SuppressEvents = SuppressEvents::No);
    void didCloseWindow(WebExtensionWindow&);
    void didFocusWindow(const WebExtensionWindow*, SuppressEvents = SuppressEvents::No);

    void didOpenTab(WebExtensionTab&, SuppressEvents = SuppressEvents::No);
    void didCloseTab(WebExtensionTab&, WindowIsClosing = WindowIsClosing::No, SuppressEvents = SuppressEvents::No);
    void didActivateTab(const WebExtensionTab&, const WebExtensionTab* previousTab = nullptr);
    void didSelectOrDeselectTabs(const TabSet&);

    void didMoveTab(WebExtensionTab&, size_t oldIndex, const WebExtensionWindow* oldWindow = nullptr);
    void didReplaceTab(WebExtensionTab& oldTab, WebExtensionTab& newTab);
    void didChangeTabProperties(WebExtensionTab&, OptionSet<WebExtensionTab::ChangedProperties> = { });

    void didStartProvisionalLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);
    void didCommitLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);
    void didFinishLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);
    void didFailLoadForFrame(WebPageProxyIdentifier, WebExtensionFrameIdentifier, WebExtensionFrameIdentifier parentFrameID, const URL&, WallTime);

    void resourceLoadDidSendRequest(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceRequest&);
    void resourceLoadDidPerformHTTPRedirection(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&);
    void resourceLoadDidReceiveChallenge(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::AuthenticationChallenge&);
    void resourceLoadDidReceiveResponse(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceResponse&);
    void resourceLoadDidCompleteWithError(WebPageProxyIdentifier, const ResourceLoadInfo&, const WebCore::ResourceResponse&, const WebCore::ResourceError&);

#if ENABLE(INSPECTOR_EXTENSIONS)
    void inspectorWillOpen(WebInspectorUIProxy&, WebPageProxy&);
    void inspectorWillClose(WebInspectorUIProxy&, WebPageProxy&);

    void didShowInspectorExtensionPanel(API::InspectorExtension&, const Inspector::ExtensionTabID&, WebCore::FrameIdentifier) const;
    void didHideInspectorExtensionPanel(API::InspectorExtension&, const Inspector::ExtensionTabID&) const;
    void inspectedPageDidNavigate(API::InspectorExtension&, const URL&);
    void inspectorEffectiveAppearanceDidChange(API::InspectorExtension&, Inspector::ExtensionAppearance);
#endif

    WebExtensionAction& defaultAction();
    Ref<WebExtensionAction> getAction(WebExtensionWindow*);
    Ref<WebExtensionAction> getAction(WebExtensionTab*);
    Ref<WebExtensionAction> getOrCreateAction(WebExtensionWindow*);
    Ref<WebExtensionAction> getOrCreateAction(WebExtensionTab*);
    void performAction(WebExtensionTab*, UserTriggered = UserTriggered::No);

    const CommandsVector& commands();
    WebExtensionCommand* command(const String& identifier);
    void performCommand(WebExtensionCommand&, UserTriggered = UserTriggered::No);

#if USE(APPKIT)
    WebExtensionCommand* command(NSEvent *);
    bool performCommand(NSEvent *);
#endif

    NSArray *platformMenuItems(const WebExtensionTab&) const;

    const MenuItemVector& mainMenuItems() const { return m_mainMenuItems; }
    WebExtensionMenuItem* menuItem(const String& identifier) const;
    void performMenuItem(WebExtensionMenuItem&, const WebExtensionMenuItemContextParameters&, UserTriggered = UserTriggered::No);

    CocoaMenuItem *singleMenuItemOrExtensionItemWithSubmenu(const WebExtensionMenuItemContextParameters&) const;

#if PLATFORM(MAC)
    void addItemsToContextMenu(WebPageProxy&, const ContextMenuContextData&, NSMenu *);
#endif

    void userGesturePerformed(WebExtensionTab&);
    bool hasActiveUserGesture(WebExtensionTab&) const;
    void clearUserGesture(WebExtensionTab&);

    bool inTestingMode() const;

    URL backgroundContentURL();
    WKWebView *backgroundWebView() const { return m_backgroundWebView.get(); }
    bool safeToLoadBackgroundContent() const { return m_safeToLoadBackgroundContent; }

    bool decidePolicyForNavigationAction(WKWebView *, WKNavigationAction *);
    void didFinishDocumentLoad(WKWebView *, WKNavigation *);
    void didFailNavigation(WKWebView *, WKNavigation *, NSError *);
    void webViewWebContentProcessDidTerminate(WKWebView *);

    void addInjectedContent(WebUserContentControllerProxy&);
    void removeInjectedContent(WebUserContentControllerProxy&);

    bool handleContentRuleListNotificationForTab(WebExtensionTab&, const URL&, WebCore::ContentRuleListResults::Result);
    void incrementActionCountForTab(WebExtensionTab&, ssize_t incrementAmount);

    // Returns whether or not there are any matched rules after the purge.
    bool purgeMatchedRulesFromBefore(const WallTime&);

    UserStyleSheetVector& dynamicallyInjectedUserStyleSheets() { return m_dynamicallyInjectedUserStyleSheets; };

    std::optional<WebCore::PageIdentifier> backgroundPageIdentifier() const;
#if ENABLE(INSPECTOR_EXTENSIONS)
    Vector<PageIdentifierTuple> inspectorBackgroundPageIdentifiers() const;
    Vector<PageIdentifierTuple> inspectorPageIdentifiers() const;
#endif
    Vector<PageIdentifierTuple> popupPageIdentifiers() const;
    Vector<PageIdentifierTuple> tabPageIdentifiers() const;

    void addExtensionTabPage(WebPageProxy&, WebExtensionTab&);
    void addPopupPage(WebPageProxy&, WebExtensionAction&);

    void enumerateExtensionPages(Function<void(WebPageProxy&, bool& stop)>&&);

    WKWebView *relatedWebView();
    NSString *processDisplayName();
    NSArray *corsDisablingPatterns();
    WKWebViewConfiguration *webViewConfiguration(WebViewPurpose = WebViewPurpose::Any);

    WebsiteDataStore* websiteDataStore(std::optional<PAL::SessionID> = std::nullopt) const;

    void cookiesDidChange(API::HTTPCookieStore&);

    void wakeUpBackgroundContentIfNecessary(CompletionHandler<void()>&&);
    void wakeUpBackgroundContentIfNecessaryToFireEvents(EventListenerTypeSet&&, CompletionHandler<void()>&&);

    HashSet<Ref<WebProcessProxy>> processes(WebExtensionEventListenerType type, WebExtensionContentWorldType contentWorldType) const
    {
        return processes(EventListenerTypeSet { type }, contentWorldType);
    }

    HashSet<Ref<WebProcessProxy>> processes(EventListenerTypeSet&& typeSet, WebExtensionContentWorldType contentWorldType) const
    {
        return processes(WTFMove(typeSet), ContentWorldTypeSet { contentWorldType });
    }

    HashSet<Ref<WebProcessProxy>> processes(EventListenerTypeSet&&, ContentWorldTypeSet&&) const;

    const UserContentControllerProxySet& userContentControllers() const;

    bool pageListensForEvent(const WebPageProxy&, WebExtensionEventListenerType, WebExtensionContentWorldType) const;

    template<typename T>
    void sendToProcesses(const WebProcessProxySet&, const T& message) const;

    template<typename T>
    void sendToProcessesForEvent(WebExtensionEventListenerType, const T& message) const;

    template<typename T>
    void sendToProcessesForEvents(EventListenerTypeSet&&, const T& message) const;

    template<typename T>
    void sendToContentScriptProcessesForEvent(WebExtensionEventListenerType, const T& message) const;

#ifdef __OBJC__
    _WKWebExtensionContext *wrapper() const { return (_WKWebExtensionContext *)API::ObjectImpl<API::Object::Type::WebExtensionContext>::wrapper(); }
#endif

private:
    friend class WebExtensionCommand;
    friend class WebExtensionMessagePort;

    explicit WebExtensionContext();

    String stateFilePath() const;
    NSDictionary *currentState() const;
    NSDictionary *readStateFromStorage();
    void writeStateToStorage() const;

    void determineInstallReasonDuringLoad();
    void moveLocalStorageIfNeeded(const URL& previousBaseURL, CompletionHandler<void()>&&);

    void permissionsDidChange(const PermissionsSet&);

    void postAsyncNotification(NSString *notificationName, const PermissionsSet&);
    void postAsyncNotification(NSString *notificationName, const MatchPatternSet&);

    bool removePermissions(PermissionsMap&, PermissionsSet&, WallTime& nextExpirationDate, NSString *notificationName);
    bool removePermissionMatchPatterns(PermissionMatchPatternsMap&, MatchPatternSet&, EqualityOnly, WallTime& nextExpirationDate, NSString *notificationName);

    PermissionsMap& removeExpired(PermissionsMap&, WallTime& nextExpirationDate, NSString *notificationName = nil);
    PermissionMatchPatternsMap& removeExpired(PermissionMatchPatternsMap&, WallTime& nextExpirationDate, NSString *notificationName = nil);

    void populateWindowsAndTabs();

    bool isBackgroundPage(WebPageProxyIdentifier) const;
    bool backgroundContentIsLoaded() const;

    void loadBackgroundWebViewDuringLoad();
    void loadBackgroundWebViewIfNeeded();
    void loadBackgroundWebView();
    void unloadBackgroundWebView();
    void scheduleBackgroundContentToUnload();
    void unloadBackgroundContentIfPossible();

    uint64_t loadBackgroundPageListenersVersionNumberFromStorage();
    void loadBackgroundPageListenersFromStorage();
    void saveBackgroundPageListenersToStorage();

    void performTasksAfterBackgroundContentLoads();

#if ENABLE(INSPECTOR_EXTENSIONS)
    URL inspectorBackgroundPageURL() const;

    InspectorTabVector openInspectors(Function<bool(WebExtensionTab&, WebInspectorUIProxy&)>&& = nullptr) const;
    InspectorTabVector loadedInspectors() const;

    bool isInspectorBackgroundPage(WKWebView *) const;

    void loadInspectorBackgroundPagesDuringLoad();
    void unloadInspectorBackgroundPages();

    void loadInspectorBackgroundPagesForPrivateBrowsing();
    void unloadInspectorBackgroundPagesForPrivateBrowsing();

    void loadInspectorBackgroundPage(WebInspectorUIProxy&, WebExtensionTab&);
    void unloadInspectorBackgroundPage(WebInspectorUIProxy&);

    RefPtr<API::InspectorExtension> inspectorExtension(WebPageProxyIdentifier) const;
    RefPtr<WebInspectorUIProxy> inspector(const API::InspectorExtension&) const;
    HashSet<Ref<WebProcessProxy>> processes(const API::InspectorExtension&) const;
#endif // ENABLE(INSPECTOR_EXTENSIONS)

    API::ContentWorld& toContentWorld(WebExtensionContentWorldType) const;

    void addInjectedContent() { addInjectedContent(injectedContents()); }
    void addInjectedContent(const InjectedContentVector&);
    void addInjectedContent(const InjectedContentVector&, MatchPatternSet&);
    void addInjectedContent(const InjectedContentVector&, WebExtensionMatchPattern&);

    void updateInjectedContent() { removeInjectedContent(); addInjectedContent(); }

    void removeInjectedContent();
    void removeInjectedContent(MatchPatternSet&);
    void removeInjectedContent(WebExtensionMatchPattern&);

    // DeclarativeNetRequest methods.
    // Loading/unloading static rules
    void loadDeclarativeNetRequestRules(CompletionHandler<void(bool)>&&);
    void compileDeclarativeNetRequestRules(NSArray *, CompletionHandler<void(bool)>&&);
    void unloadDeclarativeNetRequestState();
    String declarativeNetRequestContentRuleListFilePath();

    // Updating user content controllers with new rules.
    void addDeclarativeNetRequestRulesToPrivateUserContentControllers();
    void removeDeclarativeNetRequestRules();

    // Customizing static rulesets.
    void saveDeclarativeNetRequestRulesetStateToStorage(NSDictionary *rulesetState);
    void loadDeclarativeNetRequestRulesetStateFromStorage();
    void clearDeclarativeNetRequestRulesetState();

    // Displaying action count as badge text.
    bool shouldDisplayBlockedResourceCountAsBadgeText();
    void saveShouldDisplayBlockedResourceCountAsBadgeText(bool);

    // Session and dynamic rules.
    _WKWebExtensionDeclarativeNetRequestSQLiteStore *declarativeNetRequestDynamicRulesStore();
    _WKWebExtensionDeclarativeNetRequestSQLiteStore *declarativeNetRequestSessionRulesStore();
    void updateDeclarativeNetRequestRulesInStorage(_WKWebExtensionDeclarativeNetRequestSQLiteStore *, NSString *storageType, NSString *apiName, NSArray *rulesToAdd, NSArray *ruleIDsToRemove, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

    DeclarativeNetRequestMatchedRuleVector matchedRules() { return m_matchedRules; }

    // Registered content scripts methods.
    void loadRegisteredContentScripts();
    void clearRegisteredContentScripts();
    _WKWebExtensionRegisteredScriptsSQLiteStore *registeredContentScriptsStore();

    // Storage
    void setSessionStorageAllowedInContentScripts(bool);
    bool isSessionStorageAllowedInContentScripts() const { return m_isSessionStorageAllowedInContentScripts; }
    size_t quoataForStorageType(WebExtensionDataType);

    _WKWebExtensionStorageSQLiteStore *localStorageStore();
    _WKWebExtensionStorageSQLiteStore *sessionStorageStore();
    _WKWebExtensionStorageSQLiteStore *syncStorageStore();

    void fetchCookies(WebsiteDataStore&, const URL&, const WebExtensionCookieFilterParameters&, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&)>&&);

    // Action APIs
    bool isActionMessageAllowed();
    void actionGetTitle(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void actionSetTitle(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& title, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void actionSetIcon(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& iconDictionaryJSON, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void actionGetPopup(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void actionSetPopup(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& popupPath, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void actionOpenPopup(WebPageProxyIdentifier, std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void actionGetBadgeText(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void actionSetBadgeText(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& text, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void actionGetEnabled(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<bool, WebExtensionError>&&)>&&);
    void actionSetEnabled(std::optional<WebExtensionTabIdentifier>, bool enabled, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void fireActionClickedEventIfNeeded(WebExtensionTab*);

    // Alarms APIs
    bool isAlarmsMessageAllowed();
    void alarmsCreate(const String& name, Seconds initialInterval, Seconds repeatInterval);
    void alarmsGet(const String& name, CompletionHandler<void(std::optional<WebExtensionAlarmParameters>&&)>&&);
    void alarmsClear(const String& name, CompletionHandler<void()>&&);
    void alarmsGetAll(CompletionHandler<void(Vector<WebExtensionAlarmParameters>&&)>&&);
    void alarmsClearAll(CompletionHandler<void()>&&);
    void fireAlarmsEventIfNeeded(const WebExtensionAlarm&);

    // Commands APIs
    bool isCommandsMessageAllowed();
    void commandsGetAll(CompletionHandler<void(Vector<WebExtensionCommandParameters>)>&&);
    void fireCommandEventIfNeeded(const WebExtensionCommand&, WebExtensionTab*);
    void fireCommandChangedEventIfNeeded(const WebExtensionCommand&, const String& oldShortcut);

    // Cookies APIs
    bool isCookiesMessageAllowed();
    void cookiesGet(std::optional<PAL::SessionID>, const String& name, const URL&, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&)>&&);
    void cookiesGetAll(std::optional<PAL::SessionID>, const URL&, const WebExtensionCookieFilterParameters&, CompletionHandler<void(Expected<Vector<WebExtensionCookieParameters>, WebExtensionError>&&)>&&);
    void cookiesSet(std::optional<PAL::SessionID>, const WebExtensionCookieParameters&, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&)>&&);
    void cookiesRemove(std::optional<PAL::SessionID>, const String& name, const URL&, CompletionHandler<void(Expected<std::optional<WebExtensionCookieParameters>, WebExtensionError>&&)>&&);
    void cookiesGetAllCookieStores(CompletionHandler<void(Expected<HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>>, WebExtensionError>&&)>&&);
    void fireCookiesChangedEventIfNeeded();

    // DeclarativeNetRequest APIs
    bool isDeclarativeNetRequestMessageAllowed();
    void declarativeNetRequestGetEnabledRulesets(CompletionHandler<void(Vector<String>&&)>&&);
    void declarativeNetRequestUpdateEnabledRulesets(const Vector<String>& rulesetIdentifiersToEnable, const Vector<String>& rulesetIdentifiersToDisable, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void declarativeNetRequestDisplayActionCountAsBadgeText(bool displayActionCountAsBadgeText, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void declarativeNetRequestIncrementActionCount(WebExtensionTabIdentifier, double increment, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    DeclarativeNetRequestValidatedRulesets declarativeNetRequestValidateRulesetIdentifiers(const Vector<String>&);
    size_t declarativeNetRequestEnabledRulesetCount();
    void declarativeNetRequestToggleRulesets(const Vector<String>& rulesetIdentifiers, bool newValue, NSMutableDictionary *rulesetIdentifiersToEnabledState);
    void declarativeNetRequestGetMatchedRules(std::optional<WebExtensionTabIdentifier>, std::optional<WallTime> minTimeStamp, CompletionHandler<void(Expected<Vector<WebExtensionMatchedRuleParameters>, WebExtensionError>&&)>&&);
    void declarativeNetRequestGetDynamicRules(CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void declarativeNetRequestUpdateDynamicRules(String&& rulesToAddJSON, Vector<double>&& ruleIDsToDelete, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void declarativeNetRequestGetSessionRules(CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void declarativeNetRequestUpdateSessionRules(String&& rulesToAddJSON, Vector<double>&& ruleIDsToDelete, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);

#if ENABLE(INSPECTOR_EXTENSIONS)
    // DevTools APIs
    bool isDevToolsMessageAllowed();
    void devToolsPanelsCreate(WebPageProxyIdentifier, const String& title, const String& iconPath, const String& pagePath, CompletionHandler<void(Expected<Inspector::ExtensionTabID, WebExtensionError>&&)>&&);
    void devToolsInspectedWindowEval(WebPageProxyIdentifier, const String& scriptSource, const std::optional<URL>& frameURL, CompletionHandler<void(Expected<Expected<std::span<const uint8_t>, WebCore::ExceptionDetails>, WebExtensionError>&&)>&&);
    void devToolsInspectedWindowReload(WebPageProxyIdentifier, const std::optional<bool>& ignoreCache);
#endif

    // Event APIs
    void addListener(WebPageProxyIdentifier, WebExtensionEventListenerType, WebExtensionContentWorldType);
    void removeListener(WebPageProxyIdentifier, WebExtensionEventListenerType, WebExtensionContentWorldType, size_t removedCount);

    // Extension APIs
    void extensionIsAllowedIncognitoAccess(CompletionHandler<void(bool)>&&);

    // Menus APIs
    bool isMenusMessageAllowed();
    void menusCreate(const WebExtensionMenuItemParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void menusUpdate(const String& identifier, const WebExtensionMenuItemParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void menusRemove(const String& identifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void menusRemoveAll(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void fireMenusClickedEventIfNeeded(const WebExtensionMenuItem&, bool wasChecked, const WebExtensionMenuItemContextParameters&);

    // Permissions APIs
    void permissionsGetAll(CompletionHandler<void(Vector<String>&& permissions, Vector<String>&& origins)>&&);
    void permissionsContains(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&&);
    void permissionsRequest(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&&);
    void permissionsRemove(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&&);
    void firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType, const PermissionsSet&, const MatchPatternSet&);

    // Port APIs
    void portPostMessage(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, std::optional<WebKit::WebPageProxyIdentifier>, WebExtensionPortChannelIdentifier, const String& messageJSON);
    void portRemoved(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebPageProxyIdentifier, WebExtensionPortChannelIdentifier);
    bool pageHasOpenPorts(WebPageProxy&);
    void disconnectPortsForPage(WebPageProxy&);
    void addPorts(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier, HashCountedSet<WebPageProxyIdentifier>&&);
    void removePort(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier, WebPageProxyIdentifier);
    void addNativePort(WebExtensionMessagePort&);
    void removeNativePort(WebExtensionMessagePort&);
    unsigned openPortCount(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier);
    bool isPortConnected(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier);
    void clearQueuedPortMessages(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier);
    Vector<MessagePageProxyIdentifierPair> portQueuedMessages(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier);
    void fireQueuedPortMessageEventsIfNeeded(WebProcessProxy&, WebExtensionContentWorldType, WebExtensionPortChannelIdentifier);
    void sendQueuedNativePortMessagesIfNeeded(WebExtensionPortChannelIdentifier);
    void firePortDisconnectEventIfNeeded(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier);

    // Runtime APIs
    void runtimeGetBackgroundPage(CompletionHandler<void(Expected<std::optional<WebCore::PageIdentifier>, WebExtensionError>&&)>&&);
    void runtimeOpenOptionsPage(CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void runtimeReload();
    void runtimeSendMessage(const String& extensionID, const String& messageJSON, const WebExtensionMessageSenderParameters&, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void runtimeConnect(const String& extensionID, WebExtensionPortChannelIdentifier, const String& name, const WebExtensionMessageSenderParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void runtimeSendNativeMessage(const String& applicationID, const String& messageJSON, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void runtimeConnectNative(const String& applicationID, WebExtensionPortChannelIdentifier, WebPageProxyIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void runtimeWebPageSendMessage(const String& extensionID, const String& messageJSON, const WebExtensionMessageSenderParameters&, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void runtimeWebPageConnect(const String& extensionID, WebExtensionPortChannelIdentifier, const String& name, const WebExtensionMessageSenderParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void fireRuntimeStartupEventIfNeeded();
    void fireRuntimeInstalledEventIfNeeded();

    // Scripting APIs
    bool isScriptingMessageAllowed();
    void scriptingExecuteScript(const WebExtensionScriptInjectionParameters&, CompletionHandler<void(Expected<Vector<WebExtensionScriptInjectionResultParameters>, WebExtensionError>&&)>&&);
    void scriptingInsertCSS(const WebExtensionScriptInjectionParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void scriptingRemoveCSS(const WebExtensionScriptInjectionParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void scriptingRegisterContentScripts(const Vector<WebExtensionRegisteredScriptParameters>& scripts, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void scriptingUpdateRegisteredScripts(const Vector<WebExtensionRegisteredScriptParameters>& scripts, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void scriptingGetRegisteredScripts(const Vector<String>&, CompletionHandler<void(Expected<Vector<WebExtensionRegisteredScriptParameters>, WebExtensionError>&&)>&&);
    void scriptingUnregisterContentScripts(const Vector<String>&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    bool createInjectedContentForScripts(const Vector<WebExtensionRegisteredScriptParameters>&, WebExtensionDynamicScripts::WebExtensionRegisteredScript::FirstTimeRegistration, DynamicInjectedContentsMap&, NSString *callingAPIName, NSString **errorMessage);

    // Storage APIs
    bool isStorageMessageAllowed();
    void storageGet(WebPageProxyIdentifier, WebExtensionDataType, const Vector<String>& keys, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void storageGetBytesInUse(WebPageProxyIdentifier, WebExtensionDataType, const Vector<String>& keys, CompletionHandler<void(Expected<size_t, WebExtensionError>&&)>&&);
    void storageSet(WebPageProxyIdentifier, WebExtensionDataType, const String& dataJSON, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void storageRemove(WebPageProxyIdentifier, WebExtensionDataType, const Vector<String>& keys, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void storageClear(WebPageProxyIdentifier, WebExtensionDataType, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void storageSetAccessLevel(WebPageProxyIdentifier, WebExtensionDataType, WebExtensionStorageAccessLevel, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void fireStorageChangedEventIfNeeded(NSDictionary *oldKeysAndValues, NSDictionary *newKeysAndValues, WebExtensionDataType);

    // Tabs APIs
    void tabsCreate(std::optional<WebPageProxyIdentifier>, const WebExtensionTabParameters&, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&&);
    void tabsUpdate(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, const WebExtensionTabParameters&, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&&);
    void tabsDuplicate(WebExtensionTabIdentifier, const WebExtensionTabParameters&, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&&);
    void tabsGet(WebExtensionTabIdentifier, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&&);
    void tabsGetCurrent(WebPageProxyIdentifier, CompletionHandler<void(Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&&)>&&);
    void tabsQuery(WebPageProxyIdentifier, const WebExtensionTabQueryParameters&, CompletionHandler<void(Expected<Vector<WebExtensionTabParameters>, WebExtensionError>&&)>&&);
    void tabsReload(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, ReloadFromOrigin, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsGoBack(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsGoForward(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsDetectLanguage(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void tabsCaptureVisibleTab(WebPageProxyIdentifier, std::optional<WebExtensionWindowIdentifier>, WebExtensionTab::ImageFormat, uint8_t imageQuality, CompletionHandler<void(Expected<URL, WebExtensionError>&&)>&&);
    void tabsToggleReaderMode(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsSendMessage(WebExtensionTabIdentifier, const String& messageJSON, std::optional<WebExtensionFrameIdentifier>, const WebExtensionMessageSenderParameters&, CompletionHandler<void(Expected<String, WebExtensionError>&&)>&&);
    void tabsConnect(WebExtensionTabIdentifier, WebExtensionPortChannelIdentifier, String name, std::optional<WebExtensionFrameIdentifier>, const WebExtensionMessageSenderParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsGetZoom(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<double, WebExtensionError>&&)>&&);
    void tabsSetZoom(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, double, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsRemove(Vector<WebExtensionTabIdentifier>, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsExecuteScript(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, const WebExtensionScriptInjectionParameters&, CompletionHandler<void(Expected<Vector<WebExtensionScriptInjectionResultParameters>, WebExtensionError>&&)>&&);
    void tabsInsertCSS(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, const WebExtensionScriptInjectionParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void tabsRemoveCSS(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, const WebExtensionScriptInjectionParameters&, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void fireTabsCreatedEventIfNeeded(const WebExtensionTabParameters&);
    void fireTabsUpdatedEventIfNeeded(const WebExtensionTabParameters&, const WebExtensionTabParameters& changedParameters);
    void fireTabsReplacedEventIfNeeded(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier);
    void fireTabsDetachedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, size_t oldIndex);
    void fireTabsMovedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, size_t oldIndex, size_t newIndex);
    void fireTabsAttachedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, size_t newIndex);
    void fireTabsActivatedEventIfNeeded(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier);
    void fireTabsHighlightedEventIfNeeded(Vector<WebExtensionTabIdentifier>, WebExtensionWindowIdentifier);
    void fireTabsRemovedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, WindowIsClosing);

    // WebNavigation APIs
    bool isWebNavigationMessageAllowed();
    void webNavigationGetFrame(WebExtensionTabIdentifier, WebExtensionFrameIdentifier, CompletionHandler<void(Expected<std::optional<WebExtensionFrameParameters>, WebExtensionError>&&)>&&);
    void webNavigationGetAllFrames(WebExtensionTabIdentifier, CompletionHandler<void(Expected<Vector<WebExtensionFrameParameters>, WebExtensionError>&&)>&&);
    void webNavigationTraverseFrameTreeForFrame(_WKFrameTreeNode *, _WKFrameTreeNode *parentFrame, WebExtensionTab*, Vector<WebExtensionFrameParameters>&);
    std::optional<WebExtensionFrameParameters> webNavigationFindFrameIdentifierInFrameTree(_WKFrameTreeNode *, _WKFrameTreeNode *parentFrame, WebExtensionTab*, WebExtensionFrameIdentifier);

    // Windows APIs
    void windowsCreate(const WebExtensionWindowParameters&, CompletionHandler<void(Expected<std::optional<WebExtensionWindowParameters>, WebExtensionError>&&)>&&);
    void windowsGet(WebPageProxyIdentifier, WebExtensionWindowIdentifier, OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(Expected<WebExtensionWindowParameters, WebExtensionError>&&)>&&);
    void windowsGetLastFocused(OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(Expected<WebExtensionWindowParameters, WebExtensionError>&&)>&&);
    void windowsGetAll(OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(Expected<Vector<WebExtensionWindowParameters>, WebExtensionError>&&)>&&);
    void windowsUpdate(WebExtensionWindowIdentifier, WebExtensionWindowParameters, CompletionHandler<void(Expected<WebExtensionWindowParameters, WebExtensionError>&&)>&&);
    void windowsRemove(WebExtensionWindowIdentifier, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&&);
    void fireWindowsEventIfNeeded(WebExtensionEventListenerType, std::optional<WebExtensionWindowParameters>);

    // webRequest support.
    bool hasPermissionToSendWebRequestEvent(WebExtensionTab*, const URL& resourceURL, const ResourceLoadInfo&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    String m_storageDirectory;

    RetainPtr<NSMutableDictionary> m_state;

    RefPtr<WebExtension> m_extension;
    WeakPtr<WebExtensionController> m_extensionController;

    URL m_baseURL;
    String m_uniqueIdentifier = WTF::UUID::createVersion4().toString();
    bool m_customUniqueIdentifier { false };

    bool m_inspectable { false };

    HashSet<String> m_unsupportedAPIs;

    RefPtr<API::ContentWorld> m_contentScriptWorld;

    PermissionsMap m_grantedPermissions;
    PermissionsMap m_deniedPermissions;

    WallTime m_nextGrantedPermissionsExpirationDate { WallTime::nan() };
    WallTime m_nextDeniedPermissionsExpirationDate { WallTime::nan() };

    PermissionMatchPatternsMap m_grantedPermissionMatchPatterns;
    PermissionMatchPatternsMap m_deniedPermissionMatchPatterns;

    WallTime m_nextGrantedPermissionMatchPatternsExpirationDate { WallTime::nan() };
    WallTime m_nextDeniedPermissionMatchPatternsExpirationDate { WallTime::nan() };

    ListHashSet<URL> m_cachedPermissionURLs;
    HashMap<URL, PermissionState> m_cachedPermissionStates;

    size_t m_pendingPermissionRequests { 0 };

    bool m_requestedOptionalAccessToAllHosts { false };
    bool m_hasAccessInPrivateBrowsing { false };

    VoidCompletionHandlerVector m_actionsToPerformAfterBackgroundContentLoads;
    EventListenerTypeCountedSet m_backgroundContentEventListeners;
    EventListenerTypePageMap m_eventListenerPages;

    bool m_shouldFireStartupEvent { false };
    InstallReason m_installReason { InstallReason::None };
    String m_previousVersion;

    RetainPtr<WKWebView> m_backgroundWebView;
    RetainPtr<_WKWebExtensionContextDelegate> m_delegate;

    std::unique_ptr<WebCore::Timer> m_unloadBackgroundWebViewTimer;
    MonotonicTime m_lastBackgroundPortActivityTime;
    bool m_backgroundContentIsLoaded { false };
    bool m_safeToLoadBackgroundContent { false };

#if ENABLE(INSPECTOR_EXTENSIONS)
    WeakHashMap<WebInspectorUIProxy, TabIdentifierWebViewPair> m_inspectorBackgroundPageMap;
    WeakHashMap<WebInspectorUIProxy, Ref<API::InspectorExtension>> m_inspectorExtensionMap;
#endif

    HashMap<Ref<WebExtensionMatchPattern>, UserScriptVector> m_injectedScriptsPerPatternMap;
    HashMap<Ref<WebExtensionMatchPattern>, UserStyleSheetVector> m_injectedStyleSheetsPerPatternMap;

    HashMap<String, Ref<WebExtensionDynamicScripts::WebExtensionRegisteredScript>> m_registeredScriptsMap;
    RetainPtr<_WKWebExtensionRegisteredScriptsSQLiteStore> m_registeredContentScriptsStorage;

    UserStyleSheetVector m_dynamicallyInjectedUserStyleSheets;

    HashMap<String, Ref<WebExtensionAlarm>> m_alarmMap;
    WeakHashMap<WebExtensionWindow, Ref<WebExtensionAction>> m_actionWindowMap;
    WeakHashMap<WebExtensionTab, Ref<WebExtensionAction>> m_actionTabMap;
    RefPtr<WebExtensionAction> m_defaultAction;

    PortCountedSet m_ports;
    PageProxyIdentifierPortMap m_pagePortMap;
    PortQueuedMessageMap m_portQueuedMessages;
    NativePortMap m_nativePortMap;

    mutable WindowIdentifierMap m_windowMap;
    mutable WindowIdentifierVector m_windowOrderVector;
    mutable std::optional<WebExtensionWindowIdentifier> m_focusedWindowIdentifier;

    mutable TabIdentifierMap m_tabMap;
    PageTabIdentifierMap m_extensionPageTabMap;
    PopupPageActionMap m_popupPageActionMap;

    RetainPtr<NSMapTable> m_tabDelegateToIdentifierMap;

    CommandsVector m_commands;
    bool m_populatedCommands { false };

    String m_declarativeNetRequestContentRuleListFilePath;
    DeclarativeNetRequestMatchedRuleVector m_matchedRules;
    RetainPtr<_WKWebExtensionDeclarativeNetRequestSQLiteStore> m_declarativeNetRequestDynamicRulesStore;
    RetainPtr<_WKWebExtensionDeclarativeNetRequestSQLiteStore> m_declarativeNetRequestSessionRulesStore;
    HashSet<double> m_sessionRulesIDs;
    HashSet<double> m_dynamicRulesIDs;

    MenuItemMap m_menuItems;
    MenuItemVector m_mainMenuItems;

    bool m_isSessionStorageAllowedInContentScripts { false };
    RetainPtr<_WKWebExtensionStorageSQLiteStore> m_localStorageStore;
    RetainPtr<_WKWebExtensionStorageSQLiteStore> m_sessionStorageStore;
    RetainPtr<_WKWebExtensionStorageSQLiteStore> m_syncStorageStore;
};

template<typename T>
void WebExtensionContext::sendToProcesses(const WebProcessProxySet& processes, const T& message) const
{
    if (!isLoaded())
        return;

    for (auto& process : processes)
        process->send(T(message), identifier());
}

template<typename T>
void WebExtensionContext::sendToProcessesForEvent(WebExtensionEventListenerType type, const T& message) const
{
    sendToProcesses(processes(type, WebExtensionContentWorldType::Main), message);
}

template<typename T>
void WebExtensionContext::sendToProcessesForEvents(EventListenerTypeSet&& typeSet, const T& message) const
{
    sendToProcesses(processes(WTFMove(typeSet), WebExtensionContentWorldType::Main), message);
}

template<typename T>
void WebExtensionContext::sendToContentScriptProcessesForEvent(WebExtensionEventListenerType type, const T& message) const
{
    sendToProcesses(processes(type, WebExtensionContentWorldType::ContentScript), message);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
