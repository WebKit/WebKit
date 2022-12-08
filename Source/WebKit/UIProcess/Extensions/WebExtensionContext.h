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
#include "WebExtensionContextIdentifier.h"
#include "WebExtensionController.h"
#include "WebExtensionEventListenerType.h"
#include "WebExtensionMatchPattern.h"
#include "WebPageProxyIdentifier.h"
#include <wtf/Forward.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/URLHash.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>

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

namespace WebKit {

class WebExtension;
class WebExtensionController;
class WebUserContentControllerProxy;
struct WebExtensionContextParameters;

class WebExtensionContext : public API::ObjectImpl<API::Object::Type::WebExtensionContext>, public IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(WebExtensionContext);

public:
    template<typename... Args>
    static Ref<WebExtension> create(Args&&... args)
    {
        return adoptRef(*new WebExtensionContext(std::forward<Args>(args)...));
    }

    static WebExtensionContext* get(WebExtensionContextIdentifier);

    explicit WebExtensionContext(Ref<WebExtension>&&);

    using PermissionsMap = HashMap<String, WallTime>;
    using PermissionMatchPatternsMap = HashMap<Ref<WebExtensionMatchPattern>, WallTime>;

    using UserScriptVector = Vector<Ref<API::UserScript>>;
    using UserStyleSheetVector = Vector<Ref<API::UserStyleSheet>>;

    using PermissionsSet = WebExtension::PermissionsSet;
    using MatchPatternSet = WebExtension::MatchPatternSet;
    using InjectedContentData = WebExtension::InjectedContentData;
    using InjectedContentVector = WebExtension::InjectedContentVector;

    using EventListenterTypeCountedSet = HashCountedSet<WebExtensionEventListenerType, WTF::IntHash<WebKit::WebExtensionEventListenerType>, WTF::StrongEnumHashTraits<WebKit::WebExtensionEventListenerType>>;

    enum class EqualityOnly : bool { No, Yes };

    enum class Error : uint8_t {
        Unknown = 1,
        AlreadyLoaded,
        NotLoaded,
        BaseURLTaken,
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
    bool operator!=(const WebExtensionContext& other) const { return !(this == &other); }

    NSError *createError(Error, NSString *customLocalizedDescription = nil, NSError *underlyingError = nil);

    bool storageIsPersistent() const { return hasCustomUniqueIdentifier(); }

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

    PermissionState permissionState(const String& permission, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { });
    void setPermissionState(PermissionState, const String& permission, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(const URL&, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, const URL&, WallTime expirationDate = WallTime::infinity());

    PermissionState permissionState(WebExtensionMatchPattern&, _WKWebExtensionTab * = nil, OptionSet<PermissionStateOptions> = { PermissionStateOptions::RequestedWithTabsPermission });
    void setPermissionState(PermissionState, WebExtensionMatchPattern&, WallTime expirationDate = WallTime::infinity());

    void clearCachedPermissionStates();

    void userGesturePerformed(_WKWebExtensionTab *);
    bool hasActiveUserGesture(_WKWebExtensionTab *) const;
    void cancelUserGesture(_WKWebExtensionTab *);

    bool inTestingMode() const { return m_testingMode; }
    void setTestingMode(bool);

    bool decidePolicyForNavigationAction(WKWebView *, WKNavigationAction *);
    void didFinishNavigation(WKWebView *, WKNavigation *);
    void didFailNavigation(WKWebView *, WKNavigation *, NSError *);
    void webViewWebContentProcessDidTerminate(WKWebView *);

    void addInjectedContent(WebUserContentControllerProxy&);
    void removeInjectedContent(WebUserContentControllerProxy&);

#ifdef __OBJC__
    _WKWebExtensionContext *wrapper() const { return (_WKWebExtensionContext *)API::ObjectImpl<API::Object::Type::WebExtensionContext>::wrapper(); }
#endif

private:
    explicit WebExtensionContext();

    String stateFilePath() const;
    NSDictionary *currentState() const;
    NSDictionary *readStateFromStorage();
    void writeStateToStorage() const;

    void postAsyncNotification(NSString *notificationName, PermissionsSet&);
    void postAsyncNotification(NSString *notificationName, MatchPatternSet&);

    void removePermissions(PermissionsMap&, PermissionsSet&, NSString *notificationName);
    void removePermissionMatchPatterns(PermissionMatchPatternsMap&, MatchPatternSet&, EqualityOnly, NSString *notificationName);

    PermissionsMap& removeExpired(PermissionsMap&, NSString *notificationName = nil);
    PermissionMatchPatternsMap& removeExpired(PermissionMatchPatternsMap&, NSString *notificationName = nil);

    WKWebViewConfiguration *webViewConfiguration();

    URL backgroundContentURL();

    void loadBackgroundWebViewDuringLoad();
    void loadBackgroundWebView();
    void unloadBackgroundWebView();

    uint64_t loadBackgroundPageListenersVersionNumberFromStorage();
    void loadBackgroundPageListenersFromStorage();
    void saveBackgroundPageListenersToStorage();

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

    // Event APIs
    void addListener(WebPageProxyIdentifier, WebExtensionEventListenerType);
    void removeListener(WebPageProxyIdentifier, WebExtensionEventListenerType);

    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    WebExtensionContextIdentifier m_identifier;

    String m_storageDirectory;

    RetainPtr<NSMutableDictionary> m_state;

    RefPtr<WebExtension> m_extension;
    WeakPtr<WebExtensionController> m_extensionController;

    URL m_baseURL;
    String m_uniqueIdentifier = UUID::createVersion4().toString();
    bool m_customUniqueIdentifier { false };

    RefPtr<API::ContentWorld> m_contentScriptWorld;

    PermissionsMap m_grantedPermissions;
    PermissionsMap m_deniedPermissions;

    PermissionMatchPatternsMap m_grantedPermissionMatchPatterns;
    PermissionMatchPatternsMap m_deniedPermissionMatchPatterns;

    ListHashSet<URL> m_cachedPermissionURLs;
    HashMap<URL, PermissionState> m_cachedPermissionStates;

    RetainPtr<NSMapTable> m_temporaryTabPermissionMatchPatterns;

    bool m_requestedOptionalAccessToAllHosts { false };
#ifdef NDEBUG
    bool m_testingMode { false };
#else
    bool m_testingMode { true };
#endif

    EventListenterTypeCountedSet m_backgroundPageListeners;
    bool m_shouldFireStartupEvent { false };

    RetainPtr<WKWebView> m_backgroundWebView;
    RetainPtr<_WKWebExtensionContextDelegate> m_delegate;

    HashMap<Ref<WebExtensionMatchPattern>, UserScriptVector> m_injectedScriptsPerPatternMap;
    HashMap<Ref<WebExtensionMatchPattern>, UserStyleSheetVector> m_injectedStyleSheetsPerPatternMap;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
