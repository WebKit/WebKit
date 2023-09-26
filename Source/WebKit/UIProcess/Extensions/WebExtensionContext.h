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

#include "APIContentWorld.h"
#include "APIObject.h"
#include "APIUserScript.h"
#include "APIUserStyleSheet.h"
#include "MessageReceiver.h"
#include "WebExtension.h"
#include "WebExtensionAlarm.h"
#include "WebExtensionContextIdentifier.h"
#include "WebExtensionController.h"
#include "WebExtensionEventListenerType.h"
#include "WebExtensionFrameIdentifier.h"
#include "WebExtensionMatchPattern.h"
#include "WebExtensionMessagePort.h"
#include "WebExtensionPortChannelIdentifier.h"
#include "WebExtensionTab.h"
#include "WebExtensionTabIdentifier.h"
#include "WebExtensionWindow.h"
#include "WebExtensionWindowIdentifier.h"
#include "WebExtensionWindowParameters.h"
#include "WebPageProxyIdentifier.h"
#include "WebProcessProxy.h"
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
#include <wtf/WeakPtr.h>

OBJC_CLASS NSDate;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSMapTable;
OBJC_CLASS NSMutableDictionary;
OBJC_CLASS NSString;
OBJC_CLASS NSURL;
OBJC_CLASS NSUUID;
OBJC_CLASS WKNavigation;
OBJC_CLASS WKNavigationAction;
OBJC_CLASS WKWebView;
OBJC_CLASS WKWebViewConfiguration;
OBJC_CLASS _WKWebExtensionContext;
OBJC_CLASS _WKWebExtensionContextDelegate;
OBJC_PROTOCOL(_WKWebExtensionTab);
OBJC_PROTOCOL(_WKWebExtensionWindow);

namespace WebKit {

class WebExtension;
class WebUserContentControllerProxy;
struct WebExtensionContextParameters;

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
    using TabIdentifierMap = HashMap<WebExtensionTabIdentifier, Ref<WebExtensionTab>>;
    using TabMapValueIterator = TabIdentifierMap::ValuesConstIteratorRange;

    using WindowVector = Vector<Ref<WebExtensionWindow>>;
    using TabSet = HashSet<Ref<WebExtensionTab>>;

    using PopulateTabs = WebExtensionWindow::PopulateTabs;
    using WindowTypeFilter = WebExtensionWindow::TypeFilter;

    using WebProcessProxySet = HashSet<Ref<WebProcessProxy>>;

    using PortWorldPair = std::pair<WebExtensionContentWorldType, WebExtensionPortChannelIdentifier>;
    using PortCountedSet = HashCountedSet<PortWorldPair>;
    using PortQueuedMessageMap = HashMap<PortWorldPair, Vector<String>>;
    using NativePortMap = HashMap<WebExtensionPortChannelIdentifier, Ref<WebExtensionMessagePort>>;

    enum class EqualityOnly : bool { No, Yes };
    enum class WindowIsClosing : bool { No, Yes };
    enum class ReloadFromOrigin : bool { No, Yes };

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

    WebExtensionContextIdentifier identifier() const { return m_identifier; }
    WebExtensionContextParameters parameters() const;

    bool operator==(const WebExtensionContext& other) const { return (this == &other); }

    NSError *createError(Error, NSString *customLocalizedDescription = nil, NSError *underlyingError = nil);

    bool storageIsPersistent() const { return hasCustomUniqueIdentifier() && !m_storageDirectory.isEmpty(); }

    bool load(WebExtensionController&, String storageDirectory, NSError ** = nullptr);
    bool unload(NSError ** = nullptr);

    bool isLoaded() const { return !!m_extensionController; }

    WebExtension& extension() const { return *m_extension; }
    WebExtensionController* extensionController() const { return m_extensionController.get(); }

    const URL& baseURL() const { return m_baseURL; }
    void setBaseURL(URL&&);

    bool isURLForThisExtension(const URL&);

    bool hasCustomUniqueIdentifier() const { return m_customUniqueIdentifier; }

    const String& uniqueIdentifier() const { return m_uniqueIdentifier; }
    void setUniqueIdentifier(String&&);

    bool isInspectable() const { return m_inspectable; }
    void setInspectable(bool);

    const InjectedContentVector& injectedContents();
    bool hasInjectedContentForURL(NSURL *);

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

    void grantPermissions(PermissionsSet&&, WallTime expirationDate = WallTime::infinity());
    void denyPermissions(PermissionsSet&&, WallTime expirationDate = WallTime::infinity());

    void grantPermissionMatchPatterns(MatchPatternSet&&, WallTime expirationDate = WallTime::infinity());
    void denyPermissionMatchPatterns(MatchPatternSet&&, WallTime expirationDate = WallTime::infinity());

    void removeGrantedPermissions(PermissionsSet&);
    void removeGrantedPermissionMatchPatterns(MatchPatternSet&, EqualityOnly);

    void removeDeniedPermissions(PermissionsSet&);
    void removeDeniedPermissionMatchPatterns(MatchPatternSet&, EqualityOnly);

    PermissionsMap::KeysConstIteratorRange currentPermissions() { return grantedPermissions().keys(); }
    PermissionMatchPatternsMap::KeysConstIteratorRange currentPermissionMatchPatterns() { return grantedPermissionMatchPatterns().keys(); }

    bool hasAccessToAllURLs();
    bool hasAccessToAllHosts();

    bool hasPermission(const String& permission, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { });
    bool hasPermission(const URL&, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    bool hasPermissions(PermissionsSet, MatchPatternSet);

    PermissionState permissionState(const String& permission, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { });
    void setPermissionState(PermissionState, const String& permission, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(const URL&, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, const URL&, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(WebExtensionMatchPattern&, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, WebExtensionMatchPattern&, WallTime expirationDate = WallTime::infinity());

    void clearCachedPermissionStates();

    Ref<WebExtensionWindow> getOrCreateWindow(_WKWebExtensionWindow *);
    RefPtr<WebExtensionWindow> getWindow(WebExtensionWindowIdentifier, std::optional<WebPageProxyIdentifier> = std::nullopt);

    Ref<WebExtensionTab> getOrCreateTab(_WKWebExtensionTab *);
    RefPtr<WebExtensionTab> getTab(WebExtensionTabIdentifier);
    RefPtr<WebExtensionTab> getTab(WebPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> = std::nullopt);

    WindowVector openWindows() const;
    TabMapValueIterator openTabs() const { return m_tabMap.values(); }

    RefPtr<WebExtensionWindow> focusedWindow();
    RefPtr<WebExtensionWindow> frontmostWindow();

    void didOpenWindow(const WebExtensionWindow&);
    void didCloseWindow(const WebExtensionWindow&);
    void didFocusWindow(WebExtensionWindow*);

    void didOpenTab(const WebExtensionTab&);
    void didCloseTab(const WebExtensionTab&, WindowIsClosing = WindowIsClosing::No);
    void didActivateTab(const WebExtensionTab&, const WebExtensionTab* previousTab = nullptr);
    void didSelectOrDeselectTabs(const TabSet&);

    void didMoveTab(const WebExtensionTab&, size_t oldIndex, const WebExtensionWindow* oldWindow = nullptr);
    void didReplaceTab(const WebExtensionTab& oldTab, const WebExtensionTab& newTab);
    void didChangeTabProperties(const WebExtensionTab&, OptionSet<WebExtensionTab::ChangedProperties> = { });

    void userGesturePerformed(_WKWebExtensionTab *);
    bool hasActiveUserGesture(_WKWebExtensionTab *) const;
    void cancelUserGesture(_WKWebExtensionTab *);

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

    void wakeUpBackgroundContentIfNecessaryToFireEvents(EventListenerTypeSet, CompletionHandler<void()>&&);

    HashSet<Ref<WebProcessProxy>> processes(WebExtensionEventListenerType, WebExtensionContentWorldType) const;
    bool pageListensForEvent(const WebPageProxy&, WebExtensionEventListenerType, WebExtensionContentWorldType) const;

    template<typename T>
    void sendToProcesses(const WebProcessProxySet&, const T& message);

    template<typename T>
    void sendToProcessesForEvent(WebExtensionEventListenerType, const T& message);

    template<typename T>
    void sendToContentScriptProcessesForEvent(WebExtensionEventListenerType, const T& message);

#ifdef __OBJC__
    _WKWebExtensionContext *wrapper() const { return (_WKWebExtensionContext *)API::ObjectImpl<API::Object::Type::WebExtensionContext>::wrapper(); }
#endif

private:
    friend class WebExtensionMessagePort;

    explicit WebExtensionContext();

    String stateFilePath() const;
    NSDictionary *currentState() const;
    NSDictionary *readStateFromStorage();
    void writeStateToStorage() const;

    void postAsyncNotification(NSString *notificationName, PermissionsSet&);
    void postAsyncNotification(NSString *notificationName, MatchPatternSet&);

    void removePermissions(PermissionsMap&, PermissionsSet&, WallTime& nextExpirationDate, NSString *notificationName);
    void removePermissionMatchPatterns(PermissionMatchPatternsMap&, MatchPatternSet&, EqualityOnly, WallTime& nextExpirationDate, NSString *notificationName);

    PermissionsMap& removeExpired(PermissionsMap&, WallTime& nextExpirationDate, NSString *notificationName = nil);
    PermissionMatchPatternsMap& removeExpired(PermissionMatchPatternsMap&, WallTime& nextExpirationDate, NSString *notificationName = nil);

    WKWebViewConfiguration *webViewConfiguration();

    void populateWindowsAndTabs();

    void loadBackgroundWebViewDuringLoad();
    void loadBackgroundWebView();
    void unloadBackgroundWebView();
    void wakeUpBackgroundContentIfNecessary(CompletionHandler<void()>&&);
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

    // Test APIs
    void testResult(bool result, String message, String sourceURL, unsigned lineNumber);
    void testEqual(bool result, String expected, String actual, String message, String sourceURL, unsigned lineNumber);
    void testMessage(String message, String sourceURL, unsigned lineNumber);
    void testYielded(String message, String sourceURL, unsigned lineNumber);
    void testFinished(bool result, String message, String sourceURL, unsigned lineNumber);

    // Alarms APIs
    void alarmsCreate(const String& name, Seconds initialInterval, Seconds repeatInterval);
    void alarmsGet(const String& name, CompletionHandler<void(std::optional<WebExtensionAlarmParameters>)>&&);
    void alarmsClear(const String& name, CompletionHandler<void()>&&);
    void alarmsGetAll(CompletionHandler<void(Vector<WebExtensionAlarmParameters>&&)>&&);
    void alarmsClearAll(CompletionHandler<void()>&&);
    void fireAlarmsEventIfNeeded(const WebExtensionAlarm&);

    // Event APIs
    void addListener(WebPageProxyIdentifier, WebExtensionEventListenerType, WebExtensionContentWorldType);
    void removeListener(WebPageProxyIdentifier, WebExtensionEventListenerType, WebExtensionContentWorldType, size_t removedCount);

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
    void runtimeSendMessage(const String& extensionID, const String& messageJSON, const WebExtensionMessageSenderParameters&, CompletionHandler<void(std::optional<String> replyJSON, std::optional<String> error)>&&);
    void runtimeConnect(const String& extensionID, WebExtensionPortChannelIdentifier, const String& name, const WebExtensionMessageSenderParameters&, CompletionHandler<void(std::optional<String> error)>&&);
    void runtimeSendNativeMessage(const String& applicationID, const String& messageJSON, CompletionHandler<void(std::optional<String> replyJSON, std::optional<String> error)>&&);
    void runtimeConnectNative(const String& applicationID, WebExtensionPortChannelIdentifier, CompletionHandler<void(std::optional<String> error)>&&);

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
    void fireTabsCreatedEventIfNeeded(const WebExtensionTabParameters&);
    void fireTabsUpdatedEventIfNeeded(const WebExtensionTabParameters&, const WebExtensionTabParameters& changedParameters);
    void fireTabsReplacedEventIfNeeded(WebExtensionTabIdentifier replacedTabIdentifier, WebExtensionTabIdentifier newTabIdentifier);
    void fireTabsDetachedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier oldWindowIdentifier, size_t oldIndex);
    void fireTabsMovedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, size_t oldIndex, size_t newIndex);
    void fireTabsAttachedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier newWindowIdentifier, size_t newIndex);
    void fireTabsActivatedEventIfNeeded(WebExtensionTabIdentifier previousActiveTabIdentifier, WebExtensionTabIdentifier newActiveTabIdentifier, WebExtensionWindowIdentifier);
    void fireTabsHighlightedEventIfNeeded(Vector<WebExtensionTabIdentifier>, WebExtensionWindowIdentifier);
    void fireTabsRemovedEventIfNeeded(WebExtensionTabIdentifier, WebExtensionWindowIdentifier, WindowIsClosing);

    // Windows APIs
    void windowsCreate(const WebExtensionWindowParameters&, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsGet(WebPageProxyIdentifier, WebExtensionWindowIdentifier, OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsGetLastFocused(OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsGetAll(OptionSet<WindowTypeFilter>, PopulateTabs, CompletionHandler<void(Vector<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsUpdate(WebExtensionWindowIdentifier, WebExtensionWindowParameters, CompletionHandler<void(std::optional<WebExtensionWindowParameters>, WebExtensionWindow::Error)>&&);
    void windowsRemove(WebExtensionWindowIdentifier, CompletionHandler<void(WebExtensionWindow::Error)>&&);
    void fireWindowsEventIfNeeded(WebExtensionEventListenerType, std::optional<WebExtensionWindowParameters>);

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

    RetainPtr<NSMapTable> m_temporaryTabPermissionMatchPatterns;

    bool m_requestedOptionalAccessToAllHosts { false };
#ifdef NDEBUG
    bool m_testingMode { false };
#else
    bool m_testingMode { true };
#endif

    VoidCompletionHandlerVector m_actionsToPerformAfterBackgroundContentLoads;
    EventListenerTypeCountedSet m_backgroundContentEventListeners;
    EventListenerTypePageMap m_eventListenerPages;
    bool m_shouldFireStartupEvent { false };

    RetainPtr<NSDate> m_lastBackgroundContentLoadDate;

    RetainPtr<WKWebView> m_backgroundWebView;
    RetainPtr<_WKWebExtensionContextDelegate> m_delegate;

    HashMap<Ref<WebExtensionMatchPattern>, UserScriptVector> m_injectedScriptsPerPatternMap;
    HashMap<Ref<WebExtensionMatchPattern>, UserStyleSheetVector> m_injectedStyleSheetsPerPatternMap;

    HashMap<String, Ref<WebExtensionAlarm>> m_alarmMap;

    PortCountedSet m_ports;
    PortQueuedMessageMap m_portQueuedMessages;
    NativePortMap m_nativePortMap;

    WindowIdentifierMap m_windowMap;
    Vector<WebExtensionWindowIdentifier> m_openWindowIdentifiers;
    std::optional<WebExtensionWindowIdentifier> m_focusedWindowIdentifier;

    TabIdentifierMap m_tabMap;
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
void WebExtensionContext::sendToContentScriptProcessesForEvent(WebExtensionEventListenerType type, const T& message)
{
    sendToProcesses(processes(type, WebExtensionContentWorldType::ContentScript), message);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
