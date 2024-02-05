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
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/URLHash.h>
#include <wtf/UUID.h>
#include <wtf/WeakHashCountedSet.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSMutableDictionary;
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

class WebExtensionContext : public API::ObjectImpl<API::Object::Type::WebExtensionContext>, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebExtensionContext);

public:
    template<typename... Args>
    static Ref<WebExtensionContext> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionContext(std::forward<Args>(args)...));
    }

    static WebExtensionContext* get(WebExtensionContextIdentifier);

    explicit WebExtensionContext(Ref<WebExtension>&&);

    using ErrorString = std::optional<String>;

    using PermissionsMap = HashMap<String, WallTime>;
    using PermissionMatchPatternsMap = HashMap<Ref<WebExtensionMatchPattern>, WallTime>;

    using UserScriptVector = Vector<Ref<API::UserScript>>;
    using UserStyleSheetVector = Vector<Ref<API::UserStyleSheet>>;

    using AlarmInfoMap = HashMap<String, double>;

    using PermissionsSet = WebExtension::PermissionsSet;
    using MatchPatternSet = WebExtension::MatchPatternSet;
    using InjectedContentData = WebExtension::InjectedContentData;
    using InjectedContentVector = WebExtension::InjectedContentVector;

    using WeakPageCountedSet = WeakHashCountedSet<WebPageProxy>;
    using EventListenerTypeCountedSet = HashCountedSet<WebExtensionEventListenerType>;
    using EventListenerTypePageMap = HashMap<WebExtensionEventListenerTypeWorldPair, WeakPageCountedSet>;
    using EventListenerTypeSet = HashSet<WebExtensionEventListenerType>;
    using VoidCompletionHandlerVector = Vector<CompletionHandler<void()>>;

    using WindowIdentifierMap = HashMap<WebExtensionWindowIdentifier, Ref<WebExtensionWindow>>;
    using WindowIdentifierVector = Vector<WebExtensionWindowIdentifier>;
    using TabIdentifierMap = HashMap<WebExtensionTabIdentifier, Ref<WebExtensionTab>>;
    using TabMapValueIterator = TabIdentifierMap::ValuesIteratorRange;
    using PageTabIdentifierMap = WeakHashMap<WebPageProxy, WebExtensionTabIdentifier>;
    using PopupPageActionMap = WeakHashMap<WebPageProxy, Ref<WebExtensionAction>>;

    using WindowVector = Vector<Ref<WebExtensionWindow>>;
    using TabVector = Vector<Ref<WebExtensionTab>>;
    using TabSet = HashSet<Ref<WebExtensionTab>>;

    using PopulateTabs = WebExtensionWindow::PopulateTabs;
    using WindowTypeFilter = WebExtensionWindow::TypeFilter;

    using WebProcessProxySet = HashSet<Ref<WebProcessProxy>>;

    using PortWorldPair = std::pair<WebExtensionContentWorldType, WebExtensionPortChannelIdentifier>;
    using PortCountedSet = HashCountedSet<PortWorldPair>;
    using PortQueuedMessageMap = HashMap<PortWorldPair, Vector<String>>;
    using NativePortMap = HashMap<WebExtensionPortChannelIdentifier, Ref<WebExtensionMessagePort>>;

    using PageIdentifierTuple = std::tuple<WebCore::PageIdentifier, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>>;

    using CommandsVector = Vector<Ref<WebExtensionCommand>>;

    using MenuItemVector = Vector<Ref<WebExtensionMenuItem>>;
    using MenuItemMap = HashMap<String, Ref<WebExtensionMenuItem>>;

    using DeclarativeNetRequestValidatedRulesets = std::pair<std::optional<WebExtension::DeclarativeNetRequestRulesetVector>, std::optional<String>>;
    using DeclarativeNetRequestMatchedRuleVector = Vector<WebExtensionMatchedRuleParameters>;

    using UserContentControllerProxySet = WeakHashSet<WebUserContentControllerProxy>;

    enum class EqualityOnly : bool { No, Yes };
    enum class WindowIsClosing : bool { No, Yes };
    enum class ReloadFromOrigin : bool { No, Yes };
    enum class UserTriggered : bool { No, Yes };
    enum class SuppressEvents : bool { No, Yes };
    enum class UpdateWindowOrder : bool { No, Yes };
    enum class IgnoreExtensionAccess : bool { No, Yes };

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
    };

    using InstallReason = WebExtensionContextInstallReason;

    enum class WebViewPurpose : uint8_t {
        Any,
        Background,
        Popup,
        Tab,
    };

    WebExtensionContextIdentifier identifier() const { return m_identifier; }
    WebExtensionContextParameters parameters() const;

    bool operator==(const WebExtensionContext& other) const { return (this == &other); }

    NSError *createError(Error, NSString *customLocalizedDescription = nil, NSError *underlyingError = nil);

    bool storageIsPersistent() const { return !m_storageDirectory.isEmpty(); }
    const String& storageDirectory() const { return m_storageDirectory; }

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

    const InjectedContentVector& injectedContents();
    bool hasInjectedContentForURL(const URL&);
    bool hasInjectedContent();

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

    PermissionsMap::KeysConstIteratorRange currentPermissions() { return grantedPermissions().keys(); }
    PermissionMatchPatternsMap::KeysConstIteratorRange currentPermissionMatchPatterns() { return grantedPermissionMatchPatterns().keys(); }

    bool hasAccessToAllURLs();
    bool hasAccessToAllHosts();

    bool hasPermission(const String& permission, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { });
    bool hasPermission(const URL&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    bool hasPermissions(PermissionsSet, MatchPatternSet);

    PermissionState permissionState(const String& permission, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { });
    void setPermissionState(PermissionState, const String& permission, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(const URL&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, const URL&, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(WebExtensionMatchPattern&, WebExtensionTab* = nullptr, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, WebExtensionMatchPattern&, WallTime expirationDate = WallTime::infinity());

    void clearCachedPermissionStates();

    Ref<WebExtensionWindow> getOrCreateWindow(_WKWebExtensionWindow *) const;
    RefPtr<WebExtensionWindow> getWindow(WebExtensionWindowIdentifier, std::optional<WebPageProxyIdentifier> = std::nullopt, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    void forgetWindow(WebExtensionWindowIdentifier) const;

    Ref<WebExtensionTab> getOrCreateTab(_WKWebExtensionTab *) const;
    RefPtr<WebExtensionTab> getTab(WebExtensionTabIdentifier, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    RefPtr<WebExtensionTab> getTab(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> = std::nullopt, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    RefPtr<WebExtensionTab> getCurrentTab(WebPageProxyIdentifier, IgnoreExtensionAccess = IgnoreExtensionAccess::No) const;
    void forgetTab(WebExtensionTabIdentifier) const;

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

    void didMoveTab(const WebExtensionTab&, size_t oldIndex, const WebExtensionWindow* oldWindow = nullptr);
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

    bool inTestingMode() const { return m_testingMode; }
    void setTestingMode(bool);

    URL backgroundContentURL();
    WKWebView *backgroundWebView() const { return m_backgroundWebView.get(); }

    bool decidePolicyForNavigationAction(WKWebView *, WKNavigationAction *);
    void didFinishNavigation(WKWebView *, WKNavigation *);
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
    Vector<PageIdentifierTuple> popupPageIdentifiers() const;
    Vector<PageIdentifierTuple> tabPageIdentifiers() const;

    void addExtensionTabPage(WebPageProxy&, WebExtensionTab&);
    void addPopupPage(WebPageProxy&, WebExtensionAction&);

    WKWebView *relatedWebView();
    NSString *processDisplayName();
    NSArray *corsDisablingPatterns();
    WKWebViewConfiguration *webViewConfiguration(WebViewPurpose = WebViewPurpose::Any);

    WebsiteDataStore* websiteDataStore(std::optional<PAL::SessionID> = std::nullopt) const;

    void cookiesDidChange(API::HTTPCookieStore&);

    void wakeUpBackgroundContentIfNecessary(CompletionHandler<void()>&&);
    void wakeUpBackgroundContentIfNecessaryToFireEvents(EventListenerTypeSet, CompletionHandler<void()>&&);

    HashSet<Ref<WebProcessProxy>> processes(WebExtensionEventListenerType, WebExtensionContentWorldType) const;
    HashSet<Ref<WebProcessProxy>> processes(EventListenerTypeSet, WebExtensionContentWorldType) const;

    const UserContentControllerProxySet& userContentControllers() const;

    bool pageListensForEvent(const WebPageProxy&, WebExtensionEventListenerType, WebExtensionContentWorldType) const;

    template<typename T>
    void sendToProcesses(const WebProcessProxySet&, const T& message);

    template<typename T>
    void sendToProcessesForEvent(WebExtensionEventListenerType, const T& message);

    template<typename T>
    void sendToProcessesForEvents(EventListenerTypeSet, const T& message);

    template<typename T>
    void sendToContentScriptProcessesForEvent(WebExtensionEventListenerType, const T& message);

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

    void moveLocalStorageIfNeeded(const URL& previousBaseURL, CompletionHandler<void()>&&);

    void invalidateStorage();

    void postAsyncNotification(NSString *notificationName, PermissionsSet&);
    void postAsyncNotification(NSString *notificationName, MatchPatternSet&);

    bool removePermissions(PermissionsMap&, PermissionsSet&, WallTime& nextExpirationDate, NSString *notificationName);
    bool removePermissionMatchPatterns(PermissionMatchPatternsMap&, MatchPatternSet&, EqualityOnly, WallTime& nextExpirationDate, NSString *notificationName);

    PermissionsMap& removeExpired(PermissionsMap&, WallTime& nextExpirationDate, NSString *notificationName = nil);
    PermissionMatchPatternsMap& removeExpired(PermissionMatchPatternsMap&, WallTime& nextExpirationDate, NSString *notificationName = nil);

    void populateWindowsAndTabs();

    void loadBackgroundWebViewDuringLoad();
    void loadBackgroundWebView();
    void unloadBackgroundWebView();
    void queueStartupAndInstallEventsForExtensionIfNecessary();
    void scheduleBackgroundContentToUnload();

    uint64_t loadBackgroundPageListenersVersionNumberFromStorage();
    void loadBackgroundPageListenersFromStorage();
    void saveBackgroundPageListenersToStorage();
    void queueEventToFireAfterBackgroundContentLoads(CompletionHandler<void()>&&);

    void performTasksAfterBackgroundContentLoads();

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
    void updateDeclarativeNetRequestRulesInStorage(_WKWebExtensionDeclarativeNetRequestSQLiteStore *, NSString *storageType, NSArray *rulesToAdd, NSArray *ruleIDsToRemove, CompletionHandler<void(std::optional<String>)>&&);

    DeclarativeNetRequestMatchedRuleVector matchedRules() { return m_matchedRules; }

    // Registered content scripts methods.
    void loadRegisteredContentScripts();
    _WKWebExtensionRegisteredScriptsSQLiteStore *registeredContentScriptsStore();

    // Storage
    void setSessionStorageAllowedInContentScripts(bool);
    bool isSessionStorageAllowedInContentScripts() const { return m_isSessionStorageAllowedInContentScripts; }
    size_t quoataForStorageType(WebExtensionStorageType);

    _WKWebExtensionStorageSQLiteStore *localStorageStore();
    _WKWebExtensionStorageSQLiteStore *sessionStorageStore();
    _WKWebExtensionStorageSQLiteStore *syncStorageStore();
    _WKWebExtensionStorageSQLiteStore *storageForType(WebExtensionStorageType);


    void fetchCookies(WebsiteDataStore&, const URL&, const WebExtensionCookieFilterParameters&, CompletionHandler<void(Vector<WebExtensionCookieParameters>&&, ErrorString)>&&);

    // Action APIs
    void actionGetTitle(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(std::optional<String>, std::optional<String>)>&&);
    void actionSetTitle(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& title, CompletionHandler<void(std::optional<String>)>&&);
    void actionSetIcon(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& iconDictionaryJSON, CompletionHandler<void(std::optional<String>)>&&);
    void actionGetPopup(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(std::optional<String>, std::optional<String>)>&&);
    void actionSetPopup(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& popupPath, CompletionHandler<void(std::optional<String>)>&&);
    void actionOpenPopup(WebPageProxyIdentifier, std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(std::optional<String>)>&&);
    void actionGetBadgeText(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(std::optional<String>, std::optional<String>)>&&);
    void actionSetBadgeText(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, const String& text, CompletionHandler<void(std::optional<String>)>&&);
    void actionGetEnabled(std::optional<WebExtensionWindowIdentifier>, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(std::optional<bool>, std::optional<String>)>&&);
    void actionSetEnabled(std::optional<WebExtensionTabIdentifier>, bool enabled, CompletionHandler<void(std::optional<String>)>&&);
    void fireActionClickedEventIfNeeded(WebExtensionTab*);

    // Alarms APIs
    void alarmsCreate(const String& name, Seconds initialInterval, Seconds repeatInterval);
    void alarmsGet(const String& name, CompletionHandler<void(std::optional<WebExtensionAlarmParameters>)>&&);
    void alarmsClear(const String& name, CompletionHandler<void()>&&);
    void alarmsGetAll(CompletionHandler<void(Vector<WebExtensionAlarmParameters>&&)>&&);
    void alarmsClearAll(CompletionHandler<void()>&&);
    void fireAlarmsEventIfNeeded(const WebExtensionAlarm&);

    // Commands APIs
    void commandsGetAll(CompletionHandler<void(Vector<WebExtensionCommandParameters>)>&&);
    void fireCommandEventIfNeeded(const WebExtensionCommand&, WebExtensionTab*);
    void fireCommandChangedEventIfNeeded(const WebExtensionCommand&, const String& oldShortcut);

    // Cookies APIs
    void cookiesGet(std::optional<PAL::SessionID>, const String& name, const URL&, CompletionHandler<void(const std::optional<WebExtensionCookieParameters>&, ErrorString)>&&);
    void cookiesGetAll(std::optional<PAL::SessionID>, const URL&, const WebExtensionCookieFilterParameters&, CompletionHandler<void(Vector<WebExtensionCookieParameters>&&, ErrorString)>&&);
    void cookiesSet(std::optional<PAL::SessionID>, const WebExtensionCookieParameters&, CompletionHandler<void(const std::optional<WebExtensionCookieParameters>&, ErrorString)>&&);
    void cookiesRemove(std::optional<PAL::SessionID>, const String& name, const URL&, CompletionHandler<void(const std::optional<WebExtensionCookieParameters>&, ErrorString)>&&);
    void cookiesGetAllCookieStores(CompletionHandler<void(HashMap<PAL::SessionID, Vector<WebExtensionTabIdentifier>>&&, ErrorString)>&&);
    void fireCookiesChangedEventIfNeeded();

    // DeclarativeNetRequest APIs
    void declarativeNetRequestGetEnabledRulesets(CompletionHandler<void(const Vector<String>&)>&&);
    void declarativeNetRequestUpdateEnabledRulesets(const Vector<String>& rulesetIdentifiersToEnable, const Vector<String>& rulesetIdentifiersToDisable, CompletionHandler<void(std::optional<String>)>&&);
    void declarativeNetRequestDisplayActionCountAsBadgeText(bool displayActionCountAsBadgeText, CompletionHandler<void(std::optional<String>)>&&);
    void declarativeNetRequestIncrementActionCount(WebExtensionTabIdentifier, double increment, CompletionHandler<void(std::optional<String>)>&&);
    DeclarativeNetRequestValidatedRulesets declarativeNetRequestValidateRulesetIdentifiers(const Vector<String>&);
    size_t declarativeNetRequestEnabledRulesetCount();
    void declarativeNetRequestToggleRulesets(const Vector<String>& rulesetIdentifiers, bool newValue, NSMutableDictionary *rulesetIdentifiersToEnabledState);
    void declarativeNetRequestGetMatchedRules(std::optional<WebExtensionTabIdentifier>, std::optional<WallTime> minTimeStamp, CompletionHandler<void(std::optional<Vector<WebExtensionMatchedRuleParameters>> matchedRules, std::optional<String>)>&&);
    void declarativeNetRequestGetDynamicRules(CompletionHandler<void(std::optional<String>, std::optional<String>)>&&);
    void declarativeNetRequestUpdateDynamicRules(std::optional<String> rulesToAddJSON, std::optional<Vector<double>> ruleIDsToDelete, CompletionHandler<void(std::optional<String>)>&&);
    void declarativeNetRequestGetSessionRules(CompletionHandler<void(std::optional<String>, std::optional<String>)>&&);
    void declarativeNetRequestUpdateSessionRules(std::optional<String> rulesToAddJSON, std::optional<Vector<double>> ruleIDsToDelete, CompletionHandler<void(std::optional<String>)>&&);

    // Event APIs
    void addListener(WebPageProxyIdentifier, WebExtensionEventListenerType, WebExtensionContentWorldType);
    void removeListener(WebPageProxyIdentifier, WebExtensionEventListenerType, WebExtensionContentWorldType, size_t removedCount);

    // Extension APIs
    void extensionIsAllowedIncognitoAccess(CompletionHandler<void(bool)>&&);

    // Menus APIs
    void menusCreate(const WebExtensionMenuItemParameters&, CompletionHandler<void(std::optional<String>)>&&);
    void menusUpdate(const String& identifier, const WebExtensionMenuItemParameters&, CompletionHandler<void(std::optional<String>)>&&);
    void menusRemove(const String& identifier, CompletionHandler<void(std::optional<String>)>&&);
    void menusRemoveAll(CompletionHandler<void(std::optional<String>)>&&);
    void fireMenusClickedEventIfNeeded(const WebExtensionMenuItem&, bool wasChecked, const WebExtensionMenuItemContextParameters&);

    // Permissions APIs
    void permissionsGetAll(CompletionHandler<void(Vector<String> permissions, Vector<String> origins)>&&);
    void permissionsContains(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&&);
    void permissionsRequest(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&&);
    void permissionsRemove(HashSet<String> permissions, HashSet<String> origins, CompletionHandler<void(bool)>&&);
    void firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType, const PermissionsSet&, const MatchPatternSet&);

    // Port APIs
    void portPostMessage(WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier, const String& messageJSON);
    void portDisconnect(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier);
    void addPorts(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier, size_t totalPortObjects);
    void removePort(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier);
    void addNativePort(WebExtensionMessagePort&);
    void removeNativePort(WebExtensionMessagePort&);
    bool isPortConnected(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier);
    void clearQueuedPortMessages(WebExtensionContentWorldType, WebExtensionPortChannelIdentifier);
    void fireQueuedPortMessageEventsIfNeeded(WebProcessProxy&, WebExtensionContentWorldType, WebExtensionPortChannelIdentifier);
    void sendQueuedNativePortMessagesIfNeeded(WebExtensionPortChannelIdentifier);
    void firePortDisconnectEventIfNeeded(WebExtensionContentWorldType sourceContentWorldType, WebExtensionContentWorldType targetContentWorldType, WebExtensionPortChannelIdentifier);

    // Runtime APIs
    void runtimeGetBackgroundPage(CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<String> error)>&&);
    void runtimeOpenOptionsPage(CompletionHandler<void(std::optional<String> error)>&&);
    void runtimeReload();
    void runtimeSendMessage(const String& extensionID, const String& messageJSON, const WebExtensionMessageSenderParameters&, CompletionHandler<void(std::optional<String> replyJSON, std::optional<String> error)>&&);
    void runtimeConnect(const String& extensionID, WebExtensionPortChannelIdentifier, const String& name, const WebExtensionMessageSenderParameters&, CompletionHandler<void(std::optional<String> error)>&&);
    void runtimeSendNativeMessage(const String& applicationID, const String& messageJSON, CompletionHandler<void(std::optional<String> replyJSON, std::optional<String> error)>&&);
    void runtimeConnectNative(const String& applicationID, WebExtensionPortChannelIdentifier, CompletionHandler<void(std::optional<String> error)>&&);
    void fireRuntimeStartupEventIfNeeded();
    void fireRuntimeInstalledEventIfNeeded();

    // Scripting APIs
    void scriptingExecuteScript(const WebExtensionScriptInjectionParameters&, CompletionHandler<void(std::optional<Vector<WebExtensionScriptInjectionResultParameters>>, WebExtensionDynamicScripts::Error)>&&);
    void scriptingInsertCSS(const WebExtensionScriptInjectionParameters&, CompletionHandler<void(WebExtensionDynamicScripts::Error)>&&);
    void scriptingRemoveCSS(const WebExtensionScriptInjectionParameters&, CompletionHandler<void(WebExtensionDynamicScripts::Error)>&&);
    void scriptingRegisterContentScripts(const Vector<WebExtensionRegisteredScriptParameters>& scripts, CompletionHandler<void(WebExtensionDynamicScripts::Error)>&&);
    void scriptingUpdateRegisteredScripts(const Vector<WebExtensionRegisteredScriptParameters>& scripts, CompletionHandler<void(WebExtensionDynamicScripts::Error)>&&);
    void scriptingGetRegisteredScripts(const Vector<String>&, CompletionHandler<void(Vector<WebExtensionRegisteredScriptParameters> scripts)>&&);
    void scriptingUnregisterContentScripts(const Vector<String>&, CompletionHandler<void(WebExtensionDynamicScripts::Error)>&&);
    bool createInjectedContentForScripts(const Vector<WebExtensionRegisteredScriptParameters>&, WebExtensionDynamicScripts::WebExtensionRegisteredScript::FirstTimeRegistration, InjectedContentVector&, NSString *callingAPIName, NSString **errorMessage);

    // Storage APIs
    void storageGet(WebPageProxyIdentifier, WebExtensionStorageType, const Vector<String>& keys, CompletionHandler<void(std::optional<String> dataJSON, ErrorString)>&&);
    void storageGetBytesInUse(WebPageProxyIdentifier, WebExtensionStorageType, const Vector<String>& keys, CompletionHandler<void(std::optional<size_t>, ErrorString)>&&);
    void storageSet(WebPageProxyIdentifier, WebExtensionStorageType, const String& dataJSON, CompletionHandler<void(ErrorString)>&&);
    void storageRemove(WebPageProxyIdentifier, WebExtensionStorageType, const Vector<String>& keys, CompletionHandler<void(ErrorString)>&&);
    void storageClear(WebPageProxyIdentifier, WebExtensionStorageType, CompletionHandler<void(ErrorString)>&&);
    void storageSetAccessLevel(WebPageProxyIdentifier, WebExtensionStorageType, WebExtensionStorageAccessLevel, CompletionHandler<void(ErrorString)>&&);
    void fireStorageChangedEventIfNeeded(NSDictionary *oldKeysAndValues, NSDictionary *newKeysAndValues, WebExtensionStorageType);

    // Tabs APIs
    void tabsCreate(WebPageProxyIdentifier, const WebExtensionTabParameters&, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&&);
    void tabsUpdate(WebExtensionTabIdentifier, const WebExtensionTabParameters&, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&&);
    void tabsDuplicate(WebExtensionTabIdentifier, const WebExtensionTabParameters&, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&&);
    void tabsGet(WebExtensionTabIdentifier, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&&);
    void tabsGetCurrent(WebPageProxyIdentifier, CompletionHandler<void(std::optional<WebExtensionTabParameters>, WebExtensionTab::Error)>&&);
    void tabsQuery(WebPageProxyIdentifier, const WebExtensionTabQueryParameters&, CompletionHandler<void(Vector<WebExtensionTabParameters>, WebExtensionTab::Error)>&&);
    void tabsReload(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, ReloadFromOrigin, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsGoBack(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsGoForward(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsDetectLanguage(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(std::optional<String>, WebExtensionTab::Error)>&&);
    void tabsCaptureVisibleTab(WebPageProxyIdentifier, std::optional<WebExtensionWindowIdentifier>, WebExtensionTab::ImageFormat, uint8_t imageQuality, CompletionHandler<void(std::optional<URL>, WebExtensionTab::Error)>&&);
    void tabsToggleReaderMode(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsSendMessage(WebExtensionTabIdentifier, const String& messageJSON, std::optional<WebExtensionFrameIdentifier>, const WebExtensionMessageSenderParameters&, CompletionHandler<void(std::optional<String> replyJSON, WebExtensionTab::Error)>&&);
    void tabsConnect(WebExtensionTabIdentifier, WebExtensionPortChannelIdentifier, String name, std::optional<WebExtensionFrameIdentifier>, const WebExtensionMessageSenderParameters&, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsGetZoom(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, CompletionHandler<void(std::optional<double>, WebExtensionTab::Error)>&&);
    void tabsSetZoom(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, double, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsRemove(Vector<WebExtensionTabIdentifier>, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsExecuteScript(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, const WebExtensionScriptInjectionParameters&, CompletionHandler<void(std::optional<Vector<WebExtensionScriptInjectionResultParameters>>, WebExtensionTab::Error)>&&);
    void tabsInsertCSS(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, const WebExtensionScriptInjectionParameters&, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void tabsRemoveCSS(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier>, const WebExtensionScriptInjectionParameters&, CompletionHandler<void(WebExtensionTab::Error)>&&);
    void fireTabsCreatedEventIfNeeded(const WebExtensionTabParameters&);
    void fireTabsUpdatedEventIfNeeded(const WebExtensionTabParameters&, const WebExtensionTabParameters& changedParameters);
    void fireTabsReplacedEventIfNeeded(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier);
    void fireTabsDetachedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, size_t oldIndex);
    void fireTabsMovedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, size_t oldIndex, size_t newIndex);
    void fireTabsAttachedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, size_t newIndex);
    void fireTabsActivatedEventIfNeeded(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier);
    void fireTabsHighlightedEventIfNeeded(Vector<WebExtensionTabIdentifier>, WebExtensionWindowIdentifier);
    void fireTabsRemovedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, WindowIsClosing);

    // Test APIs
    void testResult(bool result, String message, String sourceURL, unsigned lineNumber);
    void testEqual(bool result, String expected, String actual, String message, String sourceURL, unsigned lineNumber);
    void testMessage(String message, String sourceURL, unsigned lineNumber);
    void testYielded(String message, String sourceURL, unsigned lineNumber);
    void testFinished(bool result, String message, String sourceURL, unsigned lineNumber);

    // WebNavigation APIs
    void webNavigationGetFrame(WebExtensionTabIdentifier, WebExtensionFrameIdentifier, CompletionHandler<void(std::optional<WebExtensionFrameParameters>, std::optional<String> error)>&&);
    void webNavigationGetAllFrames(WebExtensionTabIdentifier, CompletionHandler<void(std::optional<Vector<WebExtensionFrameParameters>>, std::optional<String> error)>&&);
    void webNavigationTraverseFrameTreeForFrame(_WKFrameTreeNode *, _WKFrameTreeNode *parentFrame, WebExtensionTab*, Vector<WebExtensionFrameParameters> &);
    std::optional<WebExtensionFrameParameters> webNavigationFindFrameIdentifierInFrameTree(_WKFrameTreeNode *, _WKFrameTreeNode *parentFrame, WebExtensionTab*, WebExtensionFrameIdentifier);

    // Windows APIs
    void windowsCreate(const WebExtensionWindowParameters&, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsGet(WebPageProxyIdentifier, WebExtensionWindowIdentifier, OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsGetLastFocused(OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsGetAll(OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(Vector<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsUpdate(WebExtensionWindowIdentifier, WebExtensionWindowParameters, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsRemove(WebExtensionWindowIdentifier, CompletionHandler<void(WebExtensionWindow::Error)>&&);
    void fireWindowsEventIfNeeded(WebExtensionEventListenerType, std::optional<WebExtensionWindowParameters>);

    // webRequest support.
    bool hasPermissionToSendWebRequestEvent(WebExtensionTab*, const URL& resourceURL, const ResourceLoadInfo&);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    WebExtensionContextIdentifier m_identifier;

    String m_storageDirectory;

    RetainPtr<NSMutableDictionary> m_state;

    RefPtr<WebExtension> m_extension;
    WeakPtr<WebExtensionController> m_extensionController;

    URL m_baseURL;
    String m_uniqueIdentifier = WTF::UUID::createVersion4().toString();
    bool m_customUniqueIdentifier { false };

    bool m_inspectable { false };

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

    bool m_requestedOptionalAccessToAllHosts { false };
    bool m_hasAccessInPrivateBrowsing { false };
#ifdef NDEBUG
    bool m_testingMode { false };
#else
    bool m_testingMode { true };
#endif

    VoidCompletionHandlerVector m_actionsToPerformAfterBackgroundContentLoads;
    EventListenerTypeCountedSet m_backgroundContentEventListeners;
    EventListenerTypePageMap m_eventListenerPages;

    bool m_shouldFireStartupEvent { false };
    InstallReason m_installReason { InstallReason::None };
    String m_previousVersion;

    RetainPtr<NSDate> m_lastBackgroundContentLoadDate;

    RetainPtr<WKWebView> m_backgroundWebView;
    RetainPtr<_WKWebExtensionContextDelegate> m_delegate;

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
    PortQueuedMessageMap m_portQueuedMessages;
    NativePortMap m_nativePortMap;

    mutable WindowIdentifierMap m_windowMap;
    mutable WindowIdentifierVector m_windowOrderVector;
    mutable std::optional<WebExtensionWindowIdentifier> m_focusedWindowIdentifier;

    mutable TabIdentifierMap m_tabMap;
    PageTabIdentifierMap m_extensionPageTabMap;
    PopupPageActionMap m_popupPageActionMap;

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
void WebExtensionContext::sendToProcesses(const WebProcessProxySet& processes, const T& message)
{
    for (auto& process : processes)
        process->send(T(message), identifier());
}

template<typename T>
void WebExtensionContext::sendToProcessesForEvent(WebExtensionEventListenerType type, const T& message)
{
    sendToProcesses(processes(type, WebExtensionContentWorldType::Main), message);
}

template<typename T>
void WebExtensionContext::sendToProcessesForEvents(EventListenerTypeSet typeSet, const T& message)
{
    sendToProcesses(processes(typeSet, WebExtensionContentWorldType::Main), message);
}

template<typename T>
void WebExtensionContext::sendToContentScriptProcessesForEvent(WebExtensionEventListenerType type, const T& message)
{
    sendToProcesses(processes(type, WebExtensionContentWorldType::ContentScript), message);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
