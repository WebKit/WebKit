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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIArray.h"
#import "APIContentRuleList.h"
#import "APIContentRuleListStore.h"
#import "APIData.h"
#import "APIPageConfiguration.h"
#import "CocoaHelpers.h"
#import "ContextMenuContextData.h"
#import "InjectUserScriptImmediately.h"
#import "Logging.h"
#import "PageLoadStateObserver.h"
#import "ResourceLoadInfo.h"
#import "WKNSArray.h"
#import "WKNSData.h"
#import "WKNSError.h"
#import "WKNavigationActionPrivate.h"
#import "WKNavigationDelegatePrivate.h"
#import "WKOpenPanelParametersPrivate.h"
#import "WKPreferencesPrivate.h"
#import "WKUIDelegatePrivate.h"
#import "WKWebExtensionContextInternal.h"
#import "WKWebExtensionControllerDelegatePrivate.h"
#import "WKWebExtensionControllerInternal.h"
#import "WKWebExtensionMatchPatternInternal.h"
#import "WKWebExtensionPermission.h"
#import "WKWebExtensionTab.h"
#import "WKWebExtensionWindow.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WKWebpagePreferencesPrivate.h"
#import "WKWebsiteDataStorePrivate.h"
#import "WKWindowFeaturesPrivate.h"
#import "WebCoreArgumentCoders.h"
#import "WebExtensionAction.h"
#import "WebExtensionConstants.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionDataType.h"
#import "WebExtensionDynamicScripts.h"
#import "WebExtensionMenuItemContextParameters.h"
#import "WebExtensionPermission.h"
#import "WebExtensionTab.h"
#import "WebExtensionURLSchemeHandler.h"
#import "WebExtensionWindow.h"
#import "WebPageProxy.h"
#import "WebPageProxyIdentifier.h"
#import "WebPreferences.h"
#import "WebScriptMessageHandler.h"
#import "WebUserContentControllerProxy.h"
#import "_WKWebExtensionDeclarativeNetRequestSQLiteStore.h"
#import "_WKWebExtensionDeclarativeNetRequestTranslator.h"
#import "_WKWebExtensionLocalization.h"
#import "_WKWebExtensionRegisteredScriptsSQLiteStore.h"
#import "_WKWebExtensionStorageSQLiteStore.h"
#import <UniformTypeIdentifiers/UTType.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/TextResourceDecoder.h>
#import <WebCore/UserScript.h>
#import <pal/spi/cocoa/NSKeyedUnarchiverSPI.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/EnumTraits.h>
#import <wtf/FileSystem.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/URLParser.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#import "WebInspectorUIExtensionControllerProxy.h"
#import "_WKInspectorInternal.h"
#endif

// This number was chosen arbitrarily based on testing with some popular extensions.
static constexpr size_t maximumCachedPermissionResults = 256;

static constexpr auto permissionRequestTimeout = 2_min;

static NSString * const backgroundContentEventListenersKey = @"BackgroundContentEventListeners";
static NSString * const backgroundContentEventListenersVersionKey = @"BackgroundContentEventListenersVersion";
static NSString * const lastSeenBaseURLStateKey = @"LastSeenBaseURL";
static NSString * const lastSeenBundleHashStateKey = @"LastSeenBundleHash";
static NSString * const lastSeenVersionStateKey = @"LastSeenVersion";
static NSString * const lastSeenDisplayNameStateKey = @"LastSeenDisplayName";
static NSString * const lastLoadedDeclarativeNetRequestHashStateKey = @"LastLoadedDeclarativeNetRequestHash";

static NSString * const sessionStorageAllowedInContentScriptsKey = @"SessionStorageAllowedInContentScripts";

// Update this value when any changes are made to the WebExtensionEventListenerType enum.
static constexpr NSInteger currentBackgroundContentListenerStateVersion = 3;

// Update this value when any changes are made to the rule translation logic in _WKWebExtensionDeclarativeNetRequestRule.
static constexpr NSInteger currentDeclarativeNetRequestRuleTranslatorVersion = 3;

@interface _WKWebExtensionContextDelegate : NSObject <WKNavigationDelegate, WKUIDelegate> {
    WeakPtr<WebKit::WebExtensionContext> _webExtensionContext;
}

- (instancetype)initWithWebExtensionContext:(WebKit::WebExtensionContext&)context;

@end

@implementation _WKWebExtensionContextDelegate

- (instancetype)initWithWebExtensionContext:(WebKit::WebExtensionContext&)context
{
    if (!(self = [super init]))
        return nil;

    _webExtensionContext = context;

    return self;
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    if (!_webExtensionContext)
        return;

    if (_webExtensionContext->decidePolicyForNavigationAction(webView, navigationAction)) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    decisionHandler(WKNavigationActionPolicyCancel);
}

- (void)_webView:(WKWebView *)webView navigationDidFinishDocumentLoad:(WKNavigation *)navigation
{
    if (!_webExtensionContext)
        return;

    _webExtensionContext->didFinishDocumentLoad(webView, navigation);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (!_webExtensionContext)
        return;

    _webExtensionContext->didFailNavigation(webView, navigation, error);
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (!_webExtensionContext)
        return;

    _webExtensionContext->webViewWebContentProcessDidTerminate(webView);
}

#if PLATFORM(MAC)
- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *URLs))completionHandler
{
    if (!_webExtensionContext) {
        completionHandler(nil);
        return;
    }

    _webExtensionContext->runOpenPanel(webView, parameters, completionHandler);
}
#endif // PLATFORM(MAC)

@end

namespace WebKit {

using namespace WebExtensionDynamicScripts;

WebExtensionContext::WebExtensionContext(Ref<WebExtension>&& extension)
    : WebExtensionContext()
{
    m_extension = extension.ptr();
    m_baseURL = URL { makeString("webkit-extension://"_s, uniqueIdentifier(), '/') };
    m_delegate = [[_WKWebExtensionContextDelegate alloc] initWithWebExtensionContext:*this];
    m_tabDelegateToIdentifierMap = [NSMapTable weakToStrongObjectsMapTable];
}

static WKWebExtensionContextError toAPI(WebExtensionContext::Error error)
{
    switch (error) {
    case WebExtensionContext::Error::Unknown:
        return WKWebExtensionContextErrorUnknown;
    case WebExtensionContext::Error::AlreadyLoaded:
        return WKWebExtensionContextErrorAlreadyLoaded;
    case WebExtensionContext::Error::NotLoaded:
        return WKWebExtensionContextErrorNotLoaded;
    case WebExtensionContext::Error::BaseURLAlreadyInUse:
        return WKWebExtensionContextErrorBaseURLAlreadyInUse;
    case WebExtensionContext::Error::NoBackgroundContent:
        return WKWebExtensionContextErrorNoBackgroundContent;
    case WebExtensionContext::Error::BackgroundContentFailedToLoad:
        return WKWebExtensionContextErrorBackgroundContentFailedToLoad;
    }
}

NSError *WebExtensionContext::createError(Error error, NSString *customLocalizedDescription, NSError *underlyingError)
{
    auto errorCode = toAPI(error);
    NSString *localizedDescription;

    switch (error) {
    case Error::Unknown:
        localizedDescription = WEB_UI_STRING_KEY("An unknown error has occurred.", "An unknown error has occurred. (WKWebExtensionContext)", "WKWebExtensionContextErrorUnknown description");
        break;

    case Error::AlreadyLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is already loaded.", "WKWebExtensionContextErrorAlreadyLoaded description");
        break;

    case Error::NotLoaded:
        localizedDescription = WEB_UI_STRING("Extension context is not loaded.", "WKWebExtensionContextErrorNotLoaded description");
        break;

    case Error::BaseURLAlreadyInUse:
        localizedDescription = WEB_UI_STRING("Another extension context is loaded with the same base URL.", "WKWebExtensionContextErrorBaseURLAlreadyInUse description");
        break;

    case Error::NoBackgroundContent:
        localizedDescription = WEB_UI_STRING("No background content is available to load.", "WKWebExtensionContextErrorNoBackgroundContent description");
        break;

    case Error::BackgroundContentFailedToLoad:
        localizedDescription = WEB_UI_STRING("The background content failed to load due to an error.", "WKWebExtensionContextErrorBackgroundContentFailedToLoad description");
        break;
    }

    if (customLocalizedDescription.length)
        localizedDescription = customLocalizedDescription;

    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey: localizedDescription };
    if (underlyingError)
        userInfo = @{ NSLocalizedDescriptionKey: localizedDescription, NSUnderlyingErrorKey: underlyingError };

    return [[NSError alloc] initWithDomain:WKWebExtensionContextErrorDomain code:errorCode userInfo:userInfo];
}

void WebExtensionContext::recordError(NSError *error)
{
    ASSERT(error);

    if (!m_errors)
        m_errors = [NSMutableArray array];

    RELEASE_LOG_ERROR(Extensions, "Error recorded: %{public}@", privacyPreservingDescription(error));

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if ([m_errors containsObject:error])
        return;

    [wrapper() willChangeValueForKey:@"errors"];
    [m_errors addObject:error];
    [wrapper() didChangeValueForKey:@"errors"];

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        [NSNotificationCenter.defaultCenter postNotificationName:WKWebExtensionContextErrorsDidUpdateNotification object:wrapper() userInfo:nil];
    }).get());
}

void WebExtensionContext::clearError(Error error)
{
    if (!m_errors.get().count)
        return;

    auto errorCode = toAPI(error);
    auto *indexes = [m_errors indexesOfObjectsPassingTest:^BOOL(NSError *error, NSUInteger, BOOL *) {
        return error.code == errorCode;
    }];

    if (!indexes.count)
        return;

    [wrapper() willChangeValueForKey:@"errors"];
    [m_errors removeObjectsAtIndexes:indexes];
    [wrapper() didChangeValueForKey:@"errors"];

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }]() {
        [NSNotificationCenter.defaultCenter postNotificationName:WKWebExtensionContextErrorsDidUpdateNotification object:wrapper() userInfo:nil];
    }).get());
}

NSArray *WebExtensionContext::errors()
{
    auto array = createNSArray(protectedExtension()->errors(), [](Ref<API::Error> error) {
        return ::WebKit::wrapper(error);
    });
    return [array.get() arrayByAddingObjectsFromArray:m_errors.get()];
}

bool WebExtensionContext::load(WebExtensionController& controller, String storageDirectory, NSError **outError)
{
    if (outError)
        *outError = nil;

    if (isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded");
        if (outError)
            *outError = createError(Error::AlreadyLoaded);
        return false;
    }

#if PLATFORM(IOS) || PLATFORM(VISION)
    RefPtr extension = m_extension;
    if (extension->backgroundContentIsPersistent()) {
        RELEASE_LOG_ERROR(Extensions, "Cannot load persistent background content on this platform");
        if (outError)
            *outError = ::WebKit::wrapper(extension->createError(WebExtension::Error::InvalidBackgroundPersistence)).get();
        return false;
    }
#endif

    m_storageDirectory = storageDirectory;
    m_extensionController = controller;
    m_contentScriptWorld = API::ContentWorld::sharedWorldWithName(makeString("WebExtension-"_s, m_uniqueIdentifier));

    readStateFromStorage();

    auto lastSeenBaseURL = URL { objectForKey<NSString>(m_state, lastSeenBaseURLStateKey) };
    [m_state setObject:(NSString *)m_baseURL.string() forKey:lastSeenBaseURLStateKey];

    if (NSString *displayName = protectedExtension()->displayName())
        [m_state setObject:displayName forKey:lastSeenDisplayNameStateKey];

    m_isSessionStorageAllowedInContentScripts = boolForKey(m_state.get(), sessionStorageAllowedInContentScriptsKey, false);

    determineInstallReasonDuringLoad();

    writeStateToStorage();

    populateWindowsAndTabs();

    moveLocalStorageIfNeeded(lastSeenBaseURL, [this, protectedThis = Ref { *this }] {
        // The extension could have been unloaded before this was called.
        if (!isLoaded())
            return;

        loadBackgroundWebViewDuringLoad();

#if ENABLE(INSPECTOR_EXTENSIONS)
        loadInspectorBackgroundPagesDuringLoad();
#endif

        loadRegisteredContentScripts();

        loadDeclarativeNetRequestRulesetStateFromStorage();
        loadDeclarativeNetRequestRules([](bool) { });

        addInjectedContent();
    });

    return true;
}

bool WebExtensionContext::unload(NSError **outError)
{
    if (outError)
        *outError = nil;

    if (!isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        if (outError)
            *outError = createError(Error::NotLoaded);
        return false;
    }

    writeStateToStorage();

    unloadBackgroundWebView();
    removeInjectedContent();

    invalidateStorage();
    unloadDeclarativeNetRequestState();

    m_actionsToPerformAfterBackgroundContentLoads.clear();
    m_backgroundContentEventListeners.clear();
    m_eventListenerPages.clear();
    m_installReason = InstallReason::None;
    m_previousVersion = nullString();
    m_safeToLoadBackgroundContent = false;
    m_backgroundContentLoadError = nil;

    m_registeredScriptsMap.clear();
    m_dynamicallyInjectedUserStyleSheets.clear();
    m_injectedScriptsPerPatternMap.clear();
    m_injectedStyleSheetsPerPatternMap.clear();

    m_extensionController = nullptr;
    m_contentScriptWorld = nullptr;

    m_tabMap.clear();
    m_extensionPageTabMap.clear();
    [m_tabDelegateToIdentifierMap removeAllObjects];

    m_windowMap.clear();
    m_windowOrderVector.clear();
    m_focusedWindowIdentifier = std::nullopt;

    m_actionWindowMap.clear();
    m_actionTabMap.clear();
    m_defaultAction = nullptr;
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    m_defaultSidebar = nullptr;
#endif
    m_popupPageActionMap.clear();

    m_ports.clear();
    m_pagePortMap.clear();
    m_portQueuedMessages.clear();
    m_nativePortMap.clear();

    m_alarmMap.clear();

    m_commands.clear();
    m_populatedCommands = false;

    m_menuItems.clear();
    m_mainMenuItems.clear();

#if ENABLE(INSPECTOR_EXTENSIONS)
    m_inspectorContextMap.clear();
#endif

    m_pendingPermissionRequests = 0;

    return true;
}

bool WebExtensionContext::reload(NSError **outError)
{
    if (outError)
        *outError = nil;

    if (!isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        if (outError)
            *outError = createError(Error::NotLoaded);
        return false;
    }

    Ref controller = *m_extensionController;
    if (!controller->unload(*this, outError))
        return false;

    if (!controller->load(*this, outError))
        return false;

    return true;
}

String WebExtensionContext::stateFilePath() const
{
    if (!storageIsPersistent())
        return nullString();
    return FileSystem::pathByAppendingComponent(storageDirectory(), plistFileName());
}

NSDictionary *WebExtensionContext::currentState() const
{
    return [m_state copy];
}

NSMutableDictionary *WebExtensionContext::readStateFromPath(const String& stateFilePath)
{
    NSFileCoordinator *fileCoordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];

    __block NSMutableDictionary *savedState;

    NSError *coordinatorError;
    [fileCoordinator coordinateReadingItemAtURL:[NSURL fileURLWithPath:stateFilePath] options:NSFileCoordinatorReadingWithoutChanges error:&coordinatorError byAccessor:^(NSURL *fileURL) {
        savedState = [NSMutableDictionary dictionaryWithContentsOfURL:fileURL] ?: [NSMutableDictionary dictionary];
    }];

    if (coordinatorError)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate reading extension state: %{public}@", privacyPreservingDescription(coordinatorError));

    return savedState;
}

bool WebExtensionContext::readLastBaseURLFromState(const String& filePath, URL& outLastBaseURL)
{
    auto *state = readStateFromPath(filePath);

    if (auto *baseURL = objectForKey<NSString>(state, lastSeenBaseURLStateKey))
        outLastBaseURL = URL { baseURL };

    return outLastBaseURL.isValid();
}

bool WebExtensionContext::readDisplayNameFromState(const String& filePath, String& outDisplayName)
{
    auto *state = readStateFromPath(filePath);

    if (auto *displayName = objectForKey<NSString>(state, lastSeenDisplayNameStateKey))
        outDisplayName = displayName;

    return !outDisplayName.isEmpty();
}

NSDictionary *WebExtensionContext::readStateFromStorage()
{
    if (!storageIsPersistent()) {
        if (!m_state)
            m_state = [NSMutableDictionary dictionary];
        return m_state.get();
    }

    auto *savedState = readStateFromPath(stateFilePath());

    m_state = savedState;

    return [savedState copy];
}

void WebExtensionContext::writeStateToStorage() const
{
    if (!storageIsPersistent())
        return;

    NSFileCoordinator *fileCoordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];

    NSError *coordinatorError;
    [fileCoordinator coordinateWritingItemAtURL:[NSURL fileURLWithPath:stateFilePath()] options:NSFileCoordinatorWritingForReplacing error:&coordinatorError byAccessor:^(NSURL *fileURL) {
        NSError *error;
        if (![currentState() writeToURL:fileURL error:&error])
            RELEASE_LOG_ERROR(Extensions, "Unable to save extension state: %{public}@", privacyPreservingDescription(error));
    }];

    if (coordinatorError)
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate writing extension state: %{public}@", privacyPreservingDescription(coordinatorError));
}

void WebExtensionContext::moveLocalStorageIfNeeded(const URL& previousBaseURL, CompletionHandler<void()>&& completionHandler)
{
    if (previousBaseURL == baseURL()) {
        completionHandler();
        return;
    }

    static NSSet<NSString *> *dataTypes = [NSSet setWithObjects:WKWebsiteDataTypeIndexedDBDatabases, WKWebsiteDataTypeLocalStorage, nil];
    [webViewConfiguration().websiteDataStore _renameOrigin:previousBaseURL to:baseURL() forDataOfTypes:dataTypes completionHandler:makeBlockPtr(WTFMove(completionHandler)).get()];
}

void WebExtensionContext::invalidateStorage()
{
    m_registeredContentScriptsStorage = nullptr;
    m_localStorageStore = nullptr;
    m_sessionStorageStore = nullptr;
    m_syncStorageStore = nullptr;
}

void WebExtensionContext::setBaseURL(URL&& url)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    if (!url.isValid())
        return;

    m_baseURL = URL { makeString(url.protocol(), "://"_s, url.host(), '/') };
}

bool WebExtensionContext::isURLForThisExtension(const URL& url) const
{
    return url.isValid() && protocolHostAndPortAreEqual(baseURL(), url);
}

bool WebExtensionContext::isURLForAnyExtension(const URL& url)
{
    return url.isValid() && WebExtensionMatchPattern::extensionSchemes().contains(url.protocol().toString());
}

void WebExtensionContext::setUniqueIdentifier(String&& uniqueIdentifier)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_customUniqueIdentifier = !uniqueIdentifier.isEmpty();

    if (uniqueIdentifier.isEmpty())
        uniqueIdentifier = WTF::UUID::createVersion4().toString();

    m_uniqueIdentifier = uniqueIdentifier;
}

_WKWebExtensionLocalization *WebExtensionContext::localization()
{
    if (!m_localization)
        m_localization = [[_WKWebExtensionLocalization alloc] initWithLocalizedDictionary:protectedExtension()->localization().localizationDictionary uniqueIdentifier:baseURL().host().toString()];
    return m_localization.get();
}

RefPtr<API::Data> WebExtensionContext::localizedResourceData(const RefPtr<API::Data>& resourceData, const String& mimeType)
{
    if (!equalLettersIgnoringASCIICase(mimeType, "text/css"_s) || !resourceData)
        return resourceData;

    RefPtr decoder = WebCore::TextResourceDecoder::create(mimeType, { }, true);
    auto stylesheetContents = decoder->decode(resourceData->span());

    auto localizedString = localizedResourceString(stylesheetContents, mimeType);
    if (localizedString == stylesheetContents)
        return resourceData;

    return API::Data::create(localizedString.utf8().span());
}

String WebExtensionContext::localizedResourceString(const String& resourceContents, const String& mimeType)
{
    if (!equalLettersIgnoringASCIICase(mimeType, "text/css"_s) || resourceContents.isEmpty() || !resourceContents.contains("__MSG_"_s))
        return resourceContents;

    auto* localization = this->localization();
    if (!localization)
        return resourceContents;

    return [localization localizedStringForString:resourceContents];
}

void WebExtensionContext::setInspectable(bool inspectable)
{
    m_inspectable = inspectable;

    m_backgroundWebView.get().inspectable = inspectable;

    for (auto entry : m_extensionPageTabMap) {
        Ref page = entry.key;
        page->cocoaView().get().inspectable = inspectable;
    }

    for (auto entry : m_popupPageActionMap) {
        Ref page = entry.key;
        page->cocoaView().get().inspectable = inspectable;
    }
}

void WebExtensionContext::setUnsupportedAPIs(HashSet<String>&& unsupported)
{
    ASSERT(!isLoaded());
    if (isLoaded())
        return;

    m_unsupportedAPIs = WTFMove(unsupported);
}

WebExtensionContext::InjectedContentVector WebExtensionContext::injectedContents() const
{
    InjectedContentVector result = protectedExtension()->staticInjectedContents();

    for (auto& entry : m_registeredScriptsMap)
        result.append(entry.value->injectedContent());

    return result;
}

bool WebExtensionContext::hasInjectedContentForURL(const URL& url)
{
    for (auto& injectedContent : injectedContents()) {
        // FIXME: <https://webkit.org/b/246492> Add support for exclude globs.
        bool isExcluded = false;
        for (auto& excludeMatchPattern : injectedContent.excludeMatchPatterns) {
            if (excludeMatchPattern->matchesURL(url)) {
                isExcluded = true;
                break;
            }
        }

        if (isExcluded)
            continue;

        // FIXME: <https://webkit.org/b/246492> Add support for include globs.
        for (auto& includeMatchPattern : injectedContent.includeMatchPatterns) {
            if (includeMatchPattern->matchesURL(url))
                return true;
        }
    }

    return false;
}

bool WebExtensionContext::hasInjectedContent()
{
    return !injectedContents().isEmpty();
}

URL WebExtensionContext::optionsPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasOptionsPage())
        return { };
    return { m_baseURL, extension->optionsPagePath() };
}

URL WebExtensionContext::overrideNewTabPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasOverrideNewTabPage())
        return { };
    return { m_baseURL, extension->overrideNewTabPagePath() };
}

void WebExtensionContext::setHasAccessToPrivateData(bool hasAccess)
{
    if (m_hasAccessToPrivateData == hasAccess)
        return;

    m_hasAccessToPrivateData = hasAccess;

    if (!isLoaded())
        return;

    if (m_hasAccessToPrivateData) {
        addDeclarativeNetRequestRulesToPrivateUserContentControllers();

        for (auto& controller : extensionController()->allPrivateUserContentControllers())
            addInjectedContent(controller);

#if ENABLE(INSPECTOR_EXTENSIONS)
        loadInspectorBackgroundPagesForPrivateBrowsing();
#endif
    } else {
        for (auto& controller : extensionController()->allPrivateUserContentControllers()) {
            removeInjectedContent(controller);
            controller.removeContentRuleList(uniqueIdentifier());
        }

#if ENABLE(INSPECTOR_EXTENSIONS)
        unloadInspectorBackgroundPagesForPrivateBrowsing();
#endif
    }
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::grantedPermissions()
{
    return removeExpired(m_grantedPermissions, m_nextGrantedPermissionsExpirationDate, WKWebExtensionContextGrantedPermissionsWereRemovedNotification);
}

void WebExtensionContext::setGrantedPermissions(PermissionsMap&& grantedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_grantedPermissions)
        removedPermissions.add(entry.key);

    m_nextGrantedPermissionsExpirationDate = WallTime::nan();
    m_grantedPermissions = removeExpired(grantedPermissions, m_nextGrantedPermissionsExpirationDate);

    PermissionsSet addedPermissions;
    for (auto& entry : m_grantedPermissions) {
        if (removedPermissions.contains(entry.key)) {
            removedPermissions.remove(entry.key);
            continue;
        }

        addedPermissions.add(entry.key);
        addedPermissions.add(entry.key);
    }

    if (addedPermissions.isEmpty() && removedPermissions.isEmpty())
        return;

    removeDeniedPermissions(addedPermissions);

    permissionsDidChange(WKWebExtensionContextGrantedPermissionsWereRemovedNotification, removedPermissions);
    permissionsDidChange(WKWebExtensionContextPermissionsWereGrantedNotification, addedPermissions);
}

const WebExtensionContext::PermissionsMap& WebExtensionContext::deniedPermissions()
{
    return removeExpired(m_deniedPermissions, m_nextDeniedPermissionsExpirationDate, WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

void WebExtensionContext::setDeniedPermissions(PermissionsMap&& deniedPermissions)
{
    PermissionsSet removedPermissions;
    for (auto& entry : m_deniedPermissions)
        removedPermissions.add(entry.key);

    m_nextDeniedPermissionsExpirationDate = WallTime::nan();
    m_deniedPermissions = removeExpired(deniedPermissions, m_nextDeniedPermissionsExpirationDate);

    PermissionsSet addedPermissions;
    for (auto& entry : m_deniedPermissions) {
        if (removedPermissions.contains(entry.key)) {
            removedPermissions.remove(entry.key);
            continue;
        }

        addedPermissions.add(entry.key);
    }

    if (addedPermissions.isEmpty() && removedPermissions.isEmpty())
        return;

    removeGrantedPermissions(addedPermissions);

    permissionsDidChange(WKWebExtensionContextDeniedPermissionsWereRemovedNotification, removedPermissions);
    permissionsDidChange(WKWebExtensionContextPermissionsWereDeniedNotification, addedPermissions);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::grantedPermissionMatchPatterns()
{
    return removeExpired(m_grantedPermissionMatchPatterns, m_nextGrantedPermissionMatchPatternsExpirationDate, WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification);
}

void WebExtensionContext::setGrantedPermissionMatchPatterns(PermissionMatchPatternsMap&& grantedPermissionMatchPatterns, EqualityOnly equalityOnly)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_grantedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_nextGrantedPermissionMatchPatternsExpirationDate = WallTime::nan();
    m_grantedPermissionMatchPatterns = removeExpired(grantedPermissionMatchPatterns, m_nextGrantedPermissionsExpirationDate);

    MatchPatternSet addedMatchPatterns;
    for (auto& entry : m_grantedPermissionMatchPatterns) {
        if (removedMatchPatterns.contains(entry.key)) {
            removedMatchPatterns.remove(entry.key);
            continue;
        }

        addedMatchPatterns.add(entry.key);
    }

    if (addedMatchPatterns.isEmpty() && removedMatchPatterns.isEmpty())
        return;

    removeDeniedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification, removedMatchPatterns);
    permissionsDidChange(WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification, addedMatchPatterns);
}

const WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::deniedPermissionMatchPatterns()
{
    return removeExpired(m_deniedPermissionMatchPatterns, m_nextDeniedPermissionMatchPatternsExpirationDate, WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification);
}

void WebExtensionContext::setDeniedPermissionMatchPatterns(PermissionMatchPatternsMap&& deniedPermissionMatchPatterns, EqualityOnly equalityOnly)
{
    MatchPatternSet removedMatchPatterns;
    for (auto& entry : m_deniedPermissionMatchPatterns)
        removedMatchPatterns.add(entry.key);

    m_nextDeniedPermissionMatchPatternsExpirationDate = WallTime::nan();
    m_deniedPermissionMatchPatterns = removeExpired(deniedPermissionMatchPatterns, m_nextDeniedPermissionMatchPatternsExpirationDate);

    MatchPatternSet addedMatchPatterns;
    for (auto& entry : m_deniedPermissionMatchPatterns) {
        if (removedMatchPatterns.contains(entry.key)) {
            removedMatchPatterns.remove(entry.key);
            continue;
        }

        addedMatchPatterns.add(entry.key);
    }

    if (addedMatchPatterns.isEmpty() && removedMatchPatterns.isEmpty())
        return;

    removeGrantedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification, removedMatchPatterns);
    permissionsDidChange(WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification, addedMatchPatterns);
}

void WebExtensionContext::permissionsDidChange(NSNotificationName notificationName, const PermissionsSet& permissions)
{
    if (permissions.isEmpty())
        return;

    if (isLoaded()) {
        extensionController()->sendToAllProcesses(Messages::WebExtensionContextProxy::UpdateGrantedPermissions(m_grantedPermissions), identifier());

        if (permissions.contains(WKWebExtensionPermissionClipboardWrite)) {
            bool granted = hasPermission(WKWebExtensionPermissionClipboardWrite);

            enumerateExtensionPages([&](auto& page, bool&) {
                page.preferences().setJavaScriptCanAccessClipboard(granted);
            });
        }

        if ([notificationName isEqualToString:WKWebExtensionContextPermissionsWereGrantedNotification])
            firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnAdded, permissions, { });
        else if ([notificationName isEqualToString:WKWebExtensionContextGrantedPermissionsWereRemovedNotification])
            firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnRemoved, permissions, { });
    }

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, notificationName = retainPtr(notificationName), permissions] {
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName.get() object:wrapper() userInfo:@{ WKWebExtensionContextNotificationUserInfoKeyPermissions: toAPI(permissions) }];
    }).get());
}

void WebExtensionContext::permissionsDidChange(NSNotificationName notificationName, const MatchPatternSet& matchPatterns)
{
    if (matchPatterns.isEmpty())
        return;

    clearCachedPermissionStates();

    if (isLoaded()) {
        updateCORSDisablingPatternsOnAllExtensionPages();

        if ([notificationName isEqualToString:WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification]) {
            addInjectedContent(injectedContents(), matchPatterns);
            firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnAdded, { }, matchPatterns);
        } else if ([notificationName isEqualToString:WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification]) {
            removeInjectedContent(matchPatterns);
            firePermissionsEventListenerIfNecessary(WebExtensionEventListenerType::PermissionsOnRemoved, { }, matchPatterns);
        } else
            updateInjectedContent();

        // Fire the tab updated event for any tabs that match the changed patterns, now that the extension has / does not have permission to see the URL and title.
        constexpr auto changedProperties = OptionSet { WebExtensionTab::ChangedProperties::URL, WebExtensionTab::ChangedProperties::Title };
        for (Ref tab : openTabs()) {
            for (auto& matchPattern : matchPatterns) {
                if (matchPattern->matchesURL(tab->url()))
                    didChangeTabProperties(tab, changedProperties);
            }
        }
    }

    dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, notificationName = retainPtr(notificationName), matchPatterns] {
        [NSNotificationCenter.defaultCenter postNotificationName:notificationName.get() object:wrapper() userInfo:@{ WKWebExtensionContextNotificationUserInfoKeyMatchPatterns: toAPI(matchPatterns) }];
    }).get());
}

void WebExtensionContext::grantPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    ASSERT(!expirationDate.isNaN());

    if (permissions.isEmpty())
        return;

    if (m_nextGrantedPermissionsExpirationDate > expirationDate)
        m_nextGrantedPermissionsExpirationDate = expirationDate;

    PermissionsSet addedPermissions;
    for (auto& permission : permissions) {
        if (m_grantedPermissions.add(permission, expirationDate))
            addedPermissions.addVoid(permission);
    }

    if (addedPermissions.isEmpty())
        return;

    removeDeniedPermissions(addedPermissions);

    permissionsDidChange(WKWebExtensionContextPermissionsWereGrantedNotification, addedPermissions);
}

void WebExtensionContext::denyPermissions(PermissionsSet&& permissions, WallTime expirationDate)
{
    ASSERT(!expirationDate.isNaN());

    if (permissions.isEmpty())
        return;

    if (m_nextDeniedPermissionsExpirationDate > expirationDate)
        m_nextDeniedPermissionsExpirationDate = expirationDate;

    PermissionsSet addedPermissions;
    for (auto& permission : permissions) {
        if (m_deniedPermissions.add(permission, expirationDate))
            addedPermissions.addVoid(permission);
    }

    if (addedPermissions.isEmpty())
        return;

    removeGrantedPermissions(addedPermissions);

    permissionsDidChange(WKWebExtensionContextPermissionsWereDeniedNotification, addedPermissions);
}

void WebExtensionContext::grantPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate, EqualityOnly equalityOnly)
{
    ASSERT(!expirationDate.isNaN());

    if (permissionMatchPatterns.isEmpty())
        return;

    if (m_nextGrantedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextGrantedPermissionMatchPatternsExpirationDate = expirationDate;

    MatchPatternSet addedMatchPatterns;
    for (auto& pattern : permissionMatchPatterns) {
        if (m_grantedPermissionMatchPatterns.add(pattern, expirationDate))
            addedMatchPatterns.addVoid(pattern);
    }

    if (addedMatchPatterns.isEmpty())
        return;

    removeDeniedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification, addedMatchPatterns);
}

void WebExtensionContext::denyPermissionMatchPatterns(MatchPatternSet&& permissionMatchPatterns, WallTime expirationDate, EqualityOnly equalityOnly)
{
    ASSERT(!expirationDate.isNaN());

    if (permissionMatchPatterns.isEmpty())
        return;

    if (m_nextDeniedPermissionMatchPatternsExpirationDate > expirationDate)
        m_nextDeniedPermissionMatchPatternsExpirationDate = expirationDate;

    MatchPatternSet addedMatchPatterns;
    for (auto& pattern : permissionMatchPatterns) {
        if (m_deniedPermissionMatchPatterns.add(pattern, expirationDate))
            addedMatchPatterns.addVoid(pattern);
    }

    if (addedMatchPatterns.isEmpty())
        return;

    removeGrantedPermissionMatchPatterns(addedMatchPatterns, equalityOnly);

    permissionsDidChange(WKWebExtensionContextPermissionMatchPatternsWereDeniedNotification, addedMatchPatterns);
}

bool WebExtensionContext::removeGrantedPermissions(PermissionsSet& permissionsToRemove)
{
    return removePermissions(m_grantedPermissions, permissionsToRemove, m_nextGrantedPermissionsExpirationDate, WKWebExtensionContextGrantedPermissionsWereRemovedNotification);
}

bool WebExtensionContext::removeGrantedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    // Clear activeTab permissions if the patterns match.
    for (Ref tab : openTabs()) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (!temporaryPattern)
            continue;

        for (auto& pattern : matchPatternsToRemove) {
            if (temporaryPattern->matchesPattern(pattern))
                tab->setTemporaryPermissionMatchPattern(nullptr);
        }
    }

    if (!removePermissionMatchPatterns(m_grantedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextGrantedPermissionMatchPatternsExpirationDate, WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification))
        return false;

    removeInjectedContent(matchPatternsToRemove);

    return true;
}

bool WebExtensionContext::removeDeniedPermissions(PermissionsSet& permissionsToRemove)
{
    return removePermissions(m_deniedPermissions, permissionsToRemove, m_nextDeniedPermissionsExpirationDate, WKWebExtensionContextDeniedPermissionsWereRemovedNotification);
}

bool WebExtensionContext::removeDeniedPermissionMatchPatterns(MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly)
{
    if (!removePermissionMatchPatterns(m_deniedPermissionMatchPatterns, matchPatternsToRemove, equalityOnly, m_nextDeniedPermissionMatchPatternsExpirationDate, WKWebExtensionContextDeniedPermissionMatchPatternsWereRemovedNotification))
        return false;

    updateInjectedContent();

    return true;
}

bool WebExtensionContext::removePermissions(PermissionsMap& permissionMap, PermissionsSet& permissionsToRemove, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    if (permissionsToRemove.isEmpty())
        return false;

    nextExpirationDate = WallTime::infinity();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (permissionsToRemove.contains(entry.key)) {
            removedPermissions.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedPermissions.isEmpty() || !notificationName)
        return false;

    permissionsDidChange(notificationName, removedPermissions);

    return true;
}

bool WebExtensionContext::removePermissionMatchPatterns(PermissionMatchPatternsMap& matchPatternMap, MatchPatternSet& matchPatternsToRemove, EqualityOnly equalityOnly, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    if (matchPatternsToRemove.isEmpty())
        return false;

    nextExpirationDate = WallTime::infinity();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (matchPatternsToRemove.contains(entry.key)) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (equalityOnly == EqualityOnly::Yes) {
            if (entry.value < nextExpirationDate)
                nextExpirationDate = entry.value;

            return false;
        }

        for (auto& patternToRemove : matchPatternsToRemove) {
            Ref pattern = entry.key;
            if (patternToRemove->matchesPattern(pattern, WebExtensionMatchPattern::Options::IgnorePaths)) {
                removedMatchPatterns.add(pattern);
                return true;
            }
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedMatchPatterns.isEmpty() || !notificationName)
        return false;

    permissionsDidChange(notificationName, removedMatchPatterns);

    return true;
}

WebExtensionContext::PermissionsMap& WebExtensionContext::removeExpired(PermissionsMap& permissionMap, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return permissionMap;

    nextExpirationDate = WallTime::infinity();

    PermissionsSet removedPermissions;
    permissionMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedPermissions.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedPermissions.isEmpty() || !notificationName)
        return permissionMap;

    permissionsDidChange(notificationName, removedPermissions);

    return permissionMap;
}

WebExtensionContext::PermissionMatchPatternsMap& WebExtensionContext::removeExpired(PermissionMatchPatternsMap& matchPatternMap, WallTime& nextExpirationDate, NSNotificationName notificationName)
{
    WallTime currentTime = WallTime::now();

    // If the next expiration date hasn't passed yet, there is nothing to remove.
    if (nextExpirationDate != WallTime::nan() && nextExpirationDate > currentTime)
        return matchPatternMap;

    nextExpirationDate = WallTime::infinity();

    MatchPatternSet removedMatchPatterns;
    matchPatternMap.removeIf([&](auto& entry) {
        if (entry.value <= currentTime) {
            removedMatchPatterns.add(entry.key);
            return true;
        }

        if (entry.value < nextExpirationDate)
            nextExpirationDate = entry.value;

        return false;
    });

    if (removedMatchPatterns.isEmpty() || !notificationName)
        return matchPatternMap;

    permissionsDidChange(notificationName, removedMatchPatterns);

    return matchPatternMap;
}

void WebExtensionContext::requestPermissionMatchPatterns(const MatchPatternSet& requestedMatchPatterns, RefPtr<WebExtensionTab> tab, CompletionHandler<void(MatchPatternSet&&, MatchPatternSet&&, WallTime expirationDate)>&& completionHandler, GrantOnCompletion grantOnCompletion, OptionSet<PermissionStateOptions> options)
{
    MatchPatternSet neededMatchPatterns;
    for (auto& pattern : requestedMatchPatterns) {
        if (needsPermission(pattern, tab.get(), options))
            neededMatchPatterns.addVoid(pattern);
    }

    if (!isLoaded() || neededMatchPatterns.isEmpty()) {
        if (completionHandler)
            completionHandler(WTFMove(neededMatchPatterns), { }, WallTime::infinity());
        return;
    }

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler:)]) {
        if (completionHandler)
            completionHandler(WTFMove(neededMatchPatterns), { }, WallTime::infinity());
        return;
    }

    auto internalCompletionHandler = [this, protectedThis = Ref { *this }, neededMatchPatterns, grantOnCompletion, completionHandler = WTFMove(completionHandler)](NSSet *allowedMatchPatterns, NSDate *expirationDate) mutable {
        --m_pendingPermissionRequests;

        THROW_UNLESS([allowedMatchPatterns isKindOfClass:NSSet.class], @"Object returned by webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler: is not a set");
        THROW_UNLESS(!expirationDate || [expirationDate isKindOfClass:NSDate.class], @"Object returned by webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler: is not a date");

        for (WKWebExtensionMatchPattern *pattern in allowedMatchPatterns) {
            THROW_UNLESS([pattern isKindOfClass:WKWebExtensionMatchPattern.class], @"Object returned in set by webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler: is not a WKWebExtensionMatchPattern");
            THROW_UNLESS(neededMatchPatterns.contains(pattern._webExtensionMatchPattern), @"Set returned by webExtensionController:promptForPermissionMatchPatterns:inTab:forExtensionContext:completionHandler: doesn't contain the requested match patterns");
        }

        auto matchPatterns = toPatterns(allowedMatchPatterns);
        auto expirationTime = toImpl(expirationDate ?: NSDate.distantFuture);

        if (grantOnCompletion == GrantOnCompletion::Yes && !matchPatterns.isEmpty())
            grantPermissionMatchPatterns(MatchPatternSet(matchPatterns), expirationTime);

        if (completionHandler)
            completionHandler(WTFMove(neededMatchPatterns), WTFMove(matchPatterns), expirationTime);
    };

    ++m_pendingPermissionRequests;

    Ref callbackAggregator = EagerCallbackAggregator<void(NSSet *, NSDate *)>::create(WTFMove(internalCompletionHandler), nil, nil);

    // Timeout the request after a delay, denying all the requested match patterns.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, permissionRequestTimeout.nanosecondsAs<int64_t>()), dispatch_get_main_queue(), makeBlockPtr([callbackAggregator] {
        callbackAggregator.get()(NSSet.set, nil);
    }).get());

    [delegate webExtensionController:extensionController()->wrapper() promptForPermissionMatchPatterns:toAPI(neededMatchPatterns) inTab:tab ? tab->delegate() : nil forExtensionContext:wrapper() completionHandler:makeBlockPtr([callbackAggregator](NSSet *allowedMatchPatterns, NSDate *expirationDate) {
        callbackAggregator.get()(allowedMatchPatterns, expirationDate);
    }).get()];
}

void WebExtensionContext::requestPermissionToAccessURLs(const URLVector& requestedURLs, RefPtr<WebExtensionTab> tab, CompletionHandler<void(URLSet&&, URLSet&&, WallTime expirationDate)>&& completionHandler, GrantOnCompletion grantOnCompletion, OptionSet<PermissionStateOptions> options)
{
    URLSet neededURLs;
    for (auto& url : requestedURLs) {
        // Only HTTP family URLs are really valid to request. This avoids requesting for
        // things like new tab pages, special tabs, other extensions, about:blank, etc.
        if (url.protocolIsInHTTPFamily() && needsPermission(url, tab.get(), options))
            neededURLs.addVoid(url);
    }

    if (!isLoaded() || neededURLs.isEmpty()) {
        if (completionHandler)
            completionHandler(WTFMove(neededURLs), { }, WallTime::infinity());
        return;
    }

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:promptForPermissionToAccessURLs:inTab:forExtensionContext:completionHandler:)]) {
        if (completionHandler)
            completionHandler(WTFMove(neededURLs), { }, WallTime::infinity());
        return;
    }

    auto internalCompletionHandler = [this, protectedThis = Ref { *this }, neededURLs, grantOnCompletion, completionHandler = WTFMove(completionHandler)](NSSet *allowedURLs, NSDate *expirationDate) mutable {
        --m_pendingPermissionRequests;

        THROW_UNLESS([allowedURLs isKindOfClass:NSSet.class], @"Object returned by webExtensionController:promptForPermissionToAccessURLs:inTab:forExtensionContext:completionHandler: is not a set");
        THROW_UNLESS(!expirationDate || [expirationDate isKindOfClass:NSDate.class], @"Object returned by webExtensionController:promptForPermissionToAccessURLs:inTab:forExtensionContext:completionHandler: is not a date");

        for (NSURL *url in allowedURLs) {
            THROW_UNLESS([url isKindOfClass:NSURL.class], @"Object returned in set by webExtensionController:promptForPermissionToAccessURLs:inTab:forExtensionContext:completionHandler: is not a URL");
            THROW_UNLESS(neededURLs.contains(url), @"Result returned by webExtensionController:promptForPermissionToAccessURLs:inTab:forExtensionContext:completionHandler: doesn't contain the requested URLs");
        }

        auto expirationTime = toImpl(expirationDate ?: NSDate.distantFuture);

        URLSet urls;
        MatchPatternSet matchPatterns;

        urls.reserveInitialCapacity(allowedURLs.count);
        matchPatterns.reserveInitialCapacity(allowedURLs.count);

        for (NSURL *url in allowedURLs) {
            RefPtr matchPattern = WebExtensionMatchPattern::getOrCreate(url);
            if (!matchPattern)
                continue;

            urls.addVoid(url);
            matchPatterns.addVoid(matchPattern.releaseNonNull());
        }

        if (grantOnCompletion == GrantOnCompletion::Yes && !matchPatterns.isEmpty())
            grantPermissionMatchPatterns(WTFMove(matchPatterns), expirationTime);

        if (completionHandler)
            completionHandler(WTFMove(neededURLs), WTFMove(urls), expirationTime);
    };

    ++m_pendingPermissionRequests;

    Ref callbackAggregator = EagerCallbackAggregator<void(NSSet *, NSDate *)>::create(WTFMove(internalCompletionHandler), nil, nil);

    // Timeout the request after a delay, denying all the requested URLs.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, permissionRequestTimeout.nanosecondsAs<int64_t>()), dispatch_get_main_queue(), makeBlockPtr([callbackAggregator] {
        callbackAggregator.get()(NSSet.set, nil);
    }).get());

    [delegate webExtensionController:extensionController()->wrapper() promptForPermissionToAccessURLs:toAPI(neededURLs) inTab:tab ? tab->delegate() : nil forExtensionContext:wrapper() completionHandler:makeBlockPtr([callbackAggregator](NSSet *allowedURLs, NSDate *expirationDate) {
        callbackAggregator.get()(allowedURLs, expirationDate);
    }).get()];
}

void WebExtensionContext::requestPermissions(const PermissionsSet& requestedPermissions, RefPtr<WebExtensionTab> tab, CompletionHandler<void(PermissionsSet&&, PermissionsSet&&, WallTime expirationDate)>&& completionHandler, GrantOnCompletion grantOnCompletion, OptionSet<PermissionStateOptions> options)
{
    PermissionsSet neededPermissions;
    for (auto& permission : requestedPermissions) {
        if (needsPermission(permission, tab.get(), options))
            neededPermissions.addVoid(permission);
    }

    if (!isLoaded() || neededPermissions.isEmpty()) {
        completionHandler(WTFMove(neededPermissions), { }, WallTime::infinity());
        return;
    }

    auto delegate = extensionController()->delegate();
    if (![delegate respondsToSelector:@selector(webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler:)]) {
        completionHandler(WTFMove(neededPermissions), { }, WallTime::infinity());
        return;
    }

    auto internalCompletionHandler = [this, protectedThis = Ref { *this }, neededPermissions, grantOnCompletion, completionHandler = WTFMove(completionHandler)](NSSet *allowedPermissions, NSDate *expirationDate) mutable {
        --m_pendingPermissionRequests;

        THROW_UNLESS([allowedPermissions isKindOfClass:NSSet.class], @"Object returned by webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler: is not a set");
        THROW_UNLESS(!expirationDate || [expirationDate isKindOfClass:NSDate.class], @"Object returned by webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler: is not a date");

        for (WKWebExtensionPermission permission in allowedPermissions) {
            THROW_UNLESS([permission isKindOfClass:NSString.class], @"Object returned in set by webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler: is not a WKWebExtensionPermission");
            THROW_UNLESS(neededPermissions.contains(permission), @"Result returned by webExtensionController:promptForPermissions:inTab:forExtensionContext:completionHandler: doesn't contain the requested permissions");
        }

        auto permissions = toImpl(allowedPermissions);
        auto expirationTime = toImpl(expirationDate ?: NSDate.distantFuture);

        if (grantOnCompletion == GrantOnCompletion::Yes && !permissions.isEmpty())
            grantPermissions(PermissionsSet(permissions), expirationTime);

        if (completionHandler)
            completionHandler(WTFMove(neededPermissions), WTFMove(permissions), expirationTime);
    };

    ++m_pendingPermissionRequests;

    Ref callbackAggregator = EagerCallbackAggregator<void(NSSet *, NSDate *)>::create(WTFMove(internalCompletionHandler), nil, nil);

    // Timeout the request after a delay, denying all the requested permissions.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, permissionRequestTimeout.nanosecondsAs<int64_t>()), dispatch_get_main_queue(), makeBlockPtr([callbackAggregator] {
        callbackAggregator.get()(NSSet.set, nil);
    }).get());

    [delegate webExtensionController:extensionController()->wrapper() promptForPermissions:toAPI(neededPermissions) inTab:tab ? tab->delegate() : nil forExtensionContext:wrapper() completionHandler:makeBlockPtr([callbackAggregator](NSSet *allowedPermissions, NSDate *expirationDate) {
        callbackAggregator.get()(allowedPermissions, expirationDate);
    }).get()];
}

bool WebExtensionContext::needsPermission(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(permission, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }
}

bool WebExtensionContext::needsPermission(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(url, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }
}

bool WebExtensionContext::needsPermission(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!options.contains(PermissionStateOptions::SkipRequestedPermissions));

    switch (permissionState(pattern, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return false;

    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermission(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(permission, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermission(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(url, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermission(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    options.add(PermissionStateOptions::SkipRequestedPermissions);

    switch (permissionState(pattern, tab, options)) {
    case PermissionState::Unknown:
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        return false;

    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        return true;
    }
}

bool WebExtensionContext::hasPermissions(PermissionsSet permissions, MatchPatternSet matchPatterns)
{
    for (auto& permission : permissions) {
        if (!m_grantedPermissions.contains(permission))
            return false;
    }

    for (auto& pattern : matchPatterns) {
        bool matchFound = false;
        for (auto& grantedPattern : currentPermissionMatchPatterns()) {
            if (grantedPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnorePaths })) {
                matchFound = true;
                break;
            }
        }

        if (!matchFound)
            return false;
    }

    return true;
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const String& permission, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    ASSERT(!permission.isEmpty());

    if (tab && permission == String(WKWebExtensionPermissionTabs)) {
        if (tab->extensionHasTemporaryPermission())
            return PermissionState::GrantedExplicitly;
    }

    if (!WebExtension::supportedPermissions().contains(permission))
        return PermissionState::Unknown;

    if (deniedPermissions().contains(permission))
        return PermissionState::DeniedExplicitly;

    if (grantedPermissions().contains(permission))
        return PermissionState::GrantedExplicitly;

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    RefPtr extension = m_extension;
    if (extension->hasRequestedPermission(permission))
        return PermissionState::RequestedExplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (extension->optionalPermissions().contains(permission))
            return PermissionState::RequestedImplicitly;
    }

    return PermissionState::Unknown;
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const URL& url, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (url.isEmpty())
        return PermissionState::Unknown;

    if (isURLForThisExtension(url))
        return PermissionState::GrantedImplicitly;

    if (!WebExtensionMatchPattern::validSchemes().contains(url.protocol().toStringWithoutCopying()))
        return PermissionState::Unknown;

    if (tab) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (temporaryPattern && temporaryPattern->matchesURL(url))
            return PermissionState::GrantedExplicitly;
    }

    bool skipRequestedPermissions = options.contains(PermissionStateOptions::SkipRequestedPermissions);

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // If the cache still has the URL, then it has not expired.
    if (m_cachedPermissionURLs.contains(url)) {
        PermissionState cachedState = m_cachedPermissionStates.get(url);

        // We only want to return an unknown cached state if the SkippingRequestedPermissions option isn't used.
        if (cachedState != PermissionState::Unknown || skipRequestedPermissions) {
            // Move the URL to the end, so it stays in the cache longer as a recent hit.
            m_cachedPermissionURLs.appendOrMoveToLast(url);

            if ((cachedState == PermissionState::RequestedExplicitly || cachedState == PermissionState::RequestedImplicitly) && skipRequestedPermissions)
                return PermissionState::Unknown;

            return cachedState;
        }
    }

    auto cacheResultAndReturn = ^PermissionState(PermissionState result) {
        m_cachedPermissionURLs.appendOrMoveToLast(url);
        m_cachedPermissionStates.set(url, result);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionURLs.size());

        if (m_cachedPermissionURLs.size() <= maximumCachedPermissionResults)
            return result;

        URL firstCachedURL = m_cachedPermissionURLs.takeFirst();
        m_cachedPermissionStates.remove(firstCachedURL);

        ASSERT(m_cachedPermissionURLs.size() == m_cachedPermissionURLs.size());

        return result;
    };

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.

    auto urlMatchesPatternIgnoringWildcardHostPatterns = ^(WebExtensionMatchPattern& pattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedExplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedExplicitly);
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.

    auto urlMatchesWildcardHostPatterns = ^(WebExtensionMatchPattern& pattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesURL(url);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::DeniedImplicitly);
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return cacheResultAndReturn(PermissionState::GrantedImplicitly);
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (skipRequestedPermissions)
        return cacheResultAndReturn(PermissionState::Unknown);

    auto requestedMatchPatterns = protectedExtension()->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedExplicitly);

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return cacheResultAndReturn(PermissionState::RequestedImplicitly);
    }

    if (hasPermission(WKWebExtensionPermissionWebNavigation, tab, options))
        return cacheResultAndReturn(PermissionState::RequestedImplicitly);

    if (hasPermission(WebExtensionPermission::declarativeNetRequestFeedback(), tab, options))
        return cacheResultAndReturn(PermissionState::RequestedImplicitly);

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(WKWebExtensionPermissionTabs, tab, options))
        return PermissionState::RequestedImplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (WebExtensionMatchPattern::patternsMatchURL(protectedExtension()->optionalPermissionMatchPatterns(), url))
            return cacheResultAndReturn(PermissionState::RequestedImplicitly);
    }

    return cacheResultAndReturn(PermissionState::Unknown);
}

WebExtensionContext::PermissionState WebExtensionContext::permissionState(const WebExtensionMatchPattern& pattern, WebExtensionTab* tab, OptionSet<PermissionStateOptions> options)
{
    if (!pattern.isValid())
        return PermissionState::Unknown;

    if (pattern.matchesURL(baseURL()))
        return PermissionState::GrantedImplicitly;

    if (!pattern.matchesAllURLs() && !WebExtensionMatchPattern::validSchemes().contains(pattern.scheme()))
        return PermissionState::Unknown;

    if (tab) {
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (temporaryPattern && temporaryPattern->matchesPattern(pattern))
            return PermissionState::GrantedExplicitly;
    }

    // Access the maps here to remove any expired entries, and only do it once for this call.
    auto& grantedPermissionMatchPatterns = this->grantedPermissionMatchPatterns();
    auto& deniedPermissionMatchPatterns = this->deniedPermissionMatchPatterns();

    // First, check for patterns that are specific to certain domains, ignoring wildcard host patterns that
    // match all hosts. The order is denied, then granted. This makes sure denied takes precedence over granted.

    auto urlMatchesPatternIgnoringWildcardHostPatterns = ^(WebExtensionMatchPattern& otherPattern) {
        if (pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedExplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedExplicitly;
    }

    // Next, check for patterns that are wildcard host patterns that match all hosts (<all_urls>, *://*/*, etc),
    // also checked in denied, then granted order. Doing these wildcard patterns separately allows for blanket
    // patterns to be set as default policies while allowing for specific domains to still be granted or denied.

    auto urlMatchesWildcardHostPatterns = ^(WebExtensionMatchPattern& otherPattern) {
        if (!pattern.matchesAllHosts())
            return false;
        return pattern.matchesPattern(otherPattern);
    };

    for (auto& deniedPermissionEntry : deniedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(deniedPermissionEntry.key))
            return PermissionState::DeniedImplicitly;
    }

    for (auto& grantedPermissionEntry : grantedPermissionMatchPatterns) {
        if (urlMatchesWildcardHostPatterns(grantedPermissionEntry.key))
            return PermissionState::GrantedImplicitly;
    }

    // Finally, check for requested patterns, allowing any pattern that matches. This is the default state
    // of the extension before any patterns are granted or denied, so it should always be last.

    if (options.contains(PermissionStateOptions::SkipRequestedPermissions))
        return PermissionState::Unknown;

    auto requestedMatchPatterns = protectedExtension()->allRequestedMatchPatterns();
    for (auto& requestedMatchPattern : requestedMatchPatterns) {
        if (urlMatchesPatternIgnoringWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedExplicitly;

        if (urlMatchesWildcardHostPatterns(requestedMatchPattern))
            return PermissionState::RequestedImplicitly;
    }

    if (options.contains(PermissionStateOptions::RequestedWithTabsPermission) && hasPermission(WKWebExtensionPermissionTabs, tab, options))
        return PermissionState::RequestedImplicitly;

    if (options.contains(PermissionStateOptions::IncludeOptionalPermissions)) {
        if (WebExtensionMatchPattern::patternsMatchPattern(protectedExtension()->optionalPermissionMatchPatterns(), pattern))
            return PermissionState::RequestedImplicitly;
    }

    return PermissionState::Unknown;
}

void WebExtensionContext::setPermissionState(PermissionState state, const String& permission, WallTime expirationDate)
{
    ASSERT(!permission.isEmpty());
    ASSERT(!expirationDate.isNaN());

    auto permissions = PermissionsSet { permission };

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissions(WTFMove(permissions), expirationDate);
        break;

    case PermissionState::Unknown: {
        removeGrantedPermissions(permissions);
        removeDeniedPermissions(permissions);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissions(WTFMove(permissions), expirationDate);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::setPermissionState(PermissionState state, const URL& url, WallTime expirationDate)
{
    ASSERT(!url.isEmpty());
    ASSERT(!expirationDate.isNaN());

    RefPtr pattern = WebExtensionMatchPattern::getOrCreate(url);
    if (!pattern)
        return;

    setPermissionState(state, *pattern, expirationDate);
}

void WebExtensionContext::setPermissionState(PermissionState state, const WebExtensionMatchPattern& pattern, WallTime expirationDate)
{
    ASSERT(pattern.isValid());
    ASSERT(!expirationDate.isNaN());

    auto patterns = MatchPatternSet { const_cast<WebExtensionMatchPattern&>(pattern) };
    auto equalityOnly = pattern.matchesAllHosts() ? EqualityOnly::Yes : EqualityOnly::No;

    switch (state) {
    case PermissionState::DeniedExplicitly:
        denyPermissionMatchPatterns(WTFMove(patterns), expirationDate, equalityOnly);
        break;

    case PermissionState::Unknown: {
        removeGrantedPermissionMatchPatterns(patterns, equalityOnly);
        removeDeniedPermissionMatchPatterns(patterns, equalityOnly);
        break;
    }

    case PermissionState::GrantedExplicitly:
        grantPermissionMatchPatterns(WTFMove(patterns), expirationDate, equalityOnly);
        break;

    case PermissionState::DeniedImplicitly:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
    case PermissionState::GrantedImplicitly:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebExtensionContext::clearCachedPermissionStates()
{
    m_cachedPermissionStates.clear();
    m_cachedPermissionURLs.clear();
}

bool WebExtensionContext::hasAccessToAllURLs()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllURLs())
            return true;
    }

    return false;
}

bool WebExtensionContext::hasAccessToAllHosts()
{
    for (auto& pattern : currentPermissionMatchPatterns()) {
        if (pattern->matchesAllHosts())
            return true;
    }

    return false;
}

void WebExtensionContext::removePage(WebPageProxy& page)
{
    disconnectPortsForPage(page);
}

Ref<WebExtensionWindow> WebExtensionContext::getOrCreateWindow(WKWebExtensionWindow *delegate) const
{
    ASSERT(delegate);

    for (Ref window : m_windowMap.values()) {
        if (window->delegate() == delegate)
            return window;
    }

    Ref window = adoptRef(*new WebExtensionWindow(*this, delegate));
    m_windowMap.set(window->identifier(), window);

    RELEASE_LOG_DEBUG(Extensions, "Window %{public}llu was created", window->identifier().toUInt64());

    return window;
}

RefPtr<WebExtensionWindow> WebExtensionContext::getWindow(WebExtensionWindowIdentifier identifier, std::optional<WebPageProxyIdentifier> webPageProxyIdentifier, IgnoreExtensionAccess ignoreExtensionAccess) const
{
    if (UNLIKELY(!isValid(identifier)))
        return nullptr;

    RefPtr<WebExtensionWindow> result;

    if (isCurrent(identifier)) {
        if (webPageProxyIdentifier) {
            if (auto tab = getCurrentTab(webPageProxyIdentifier.value(), IncludeExtensionViews::Yes, ignoreExtensionAccess))
                result = tab->window();
        }

        if (!result)
            result = frontmostWindow(ignoreExtensionAccess);
    } else
        result = m_windowMap.get(identifier);

    if (UNLIKELY(!result)) {
        if (isCurrent(identifier)) {
            if (webPageProxyIdentifier)
                RELEASE_LOG_ERROR(Extensions, "Current window for page %{public}llu was not found", webPageProxyIdentifier.value().toUInt64());
            else
                RELEASE_LOG_ERROR(Extensions, "Current window not found (no frontmost window)");
        } else
            RELEASE_LOG_ERROR(Extensions, "Window %{public}llu was not found", identifier.toUInt64());

        return nullptr;
    }

    if (UNLIKELY(!result->isValid())) {
        RELEASE_LOG_ERROR(Extensions, "Window %{public}llu has nil delegate; reference not removed via didCloseWindow: before release", result->identifier().toUInt64());
        forgetWindow(result->identifier());
        return nullptr;
    }

    if (ignoreExtensionAccess == IgnoreExtensionAccess::No && !result->extensionHasAccess())
        return nullptr;

    return result;
}

void WebExtensionContext::forgetWindow(WebExtensionWindowIdentifier identifier) const
{
    if (m_focusedWindowIdentifier == identifier)
        m_focusedWindowIdentifier = std::nullopt;

    m_windowOrderVector.removeAll(identifier);
    m_windowMap.remove(identifier);
}

Ref<WebExtensionTab> WebExtensionContext::getOrCreateTab(WKWebExtensionTab *delegate) const
{
    ASSERT(delegate);

    if (NSNumber *tabIdentifier = [m_tabDelegateToIdentifierMap objectForKey:delegate]) {
        // Pass IgnoreExtensionAccess::Yes here to always get the tab. Otherwise getTab() can return null if it is as private
        // tab and the extension does not have private access. This prevents us from falling through and making a new tab
        // object that wraps the same delegate but have a different identifier.
        if (RefPtr tab = getTab(WebExtensionTabIdentifier(tabIdentifier.unsignedLongLongValue), IgnoreExtensionAccess::Yes)) {
            Ref result = tab.releaseNonNull();
            reportWebViewConfigurationErrorIfNeeded(result);
            return result;
        }
    }

    Ref tab = adoptRef(*new WebExtensionTab(*this, delegate));
    reportWebViewConfigurationErrorIfNeeded(tab);

    auto tabIdentifier = tab->identifier();
    m_tabMap.set(tabIdentifier, tab);
    [m_tabDelegateToIdentifierMap setObject:@(tabIdentifier.toUInt64()) forKey:delegate];

    RELEASE_LOG_DEBUG(Extensions, "Tab %{public}llu was created", tab->identifier().toUInt64());

    return tab;
}

RefPtr<WebExtensionTab> WebExtensionContext::getTab(WebExtensionTabIdentifier identifier, IgnoreExtensionAccess ignoreExtensionAccess) const
{
    if (UNLIKELY(!isValid(identifier)))
        return nullptr;

    RefPtr result = m_tabMap.get(identifier);
    if (UNLIKELY(!result)) {
        RELEASE_LOG_ERROR(Extensions, "Tab %{public}llu was not found", identifier.toUInt64());
        return nullptr;
    }

    if (UNLIKELY(!result->isValid())) {
        RELEASE_LOG_ERROR(Extensions, "Tab %{public}llu has nil delegate; reference not removed via didCloseTab: before release", identifier.toUInt64());
        forgetTab(identifier);
        return nullptr;
    }

    if (ignoreExtensionAccess == IgnoreExtensionAccess::No && !result->extensionHasAccess())
        return nullptr;

    return result;
}

RefPtr<WebExtensionTab> WebExtensionContext::getTab(WebPageProxyIdentifier webPageProxyIdentifier, std::optional<WebExtensionTabIdentifier> identifier, IncludeExtensionViews includeExtensionViews, IgnoreExtensionAccess ignoreExtensionAccess) const
{
    if (identifier)
        return getTab(identifier.value(), ignoreExtensionAccess);

    return getCurrentTab(webPageProxyIdentifier, includeExtensionViews, ignoreExtensionAccess);
}

RefPtr<WebExtensionTab> WebExtensionContext::getCurrentTab(WebPageProxyIdentifier webPageProxyIdentifier, IncludeExtensionViews includeExtensionViews, IgnoreExtensionAccess ignoreExtensionAccess) const
{
    RefPtr<WebExtensionTab> result;

    if (isBackgroundPage(webPageProxyIdentifier)) {
        if (includeExtensionViews == IncludeExtensionViews::No)
            return nullptr;

        if (RefPtr window = frontmostWindow())
            result = window->activeTab();

        goto finish;
    }

    // Search actions for the page.
    for (auto entry : m_popupPageActionMap) {
        if (entry.key.identifier() != webPageProxyIdentifier)
            continue;

        if (includeExtensionViews == IncludeExtensionViews::No)
            return nullptr;

        RefPtr tab = entry.value->tab();
        RefPtr window = tab ? tab->window() : entry.value->window();
        if (!tab && window)
            tab = window->activeTab();

        result = tab;
        goto finish;
    }

    // Search extension tabs for the page.
    for (auto entry : m_extensionPageTabMap) {
        if (entry.key.identifier() != webPageProxyIdentifier)
            continue;

        result = m_tabMap.get(entry.value);
        goto finish;
    }

    // Search open tabs for the page.
    for (Ref tab : openTabs()) {
        if (WKWebView *webView = tab->webView()) {
            if (webView._page->identifier() != webPageProxyIdentifier)
                continue;

            result = tab.ptr();
            goto finish;
        }
    }

#if ENABLE(INSPECTOR_EXTENSIONS)
    // Search open inspector background pages.
    for (auto entry : m_inspectorContextMap) {
        auto *webView = entry.value.backgroundWebView.get();
        if (webView._page->identifier() == webPageProxyIdentifier) {
            if (includeExtensionViews == IncludeExtensionViews::No)
                return nullptr;

            result = m_tabMap.get(entry.value.tabIdentifier.value());
            goto finish;
        }
    }

    // Search open inspectors.
    for (auto [inspector, tab] : openInspectors()) {
        Ref protectedInspector = inspector;
        if (protectedInspector->protectedInspectorPage()->identifier() == webPageProxyIdentifier) {
            if (includeExtensionViews == IncludeExtensionViews::No)
                return nullptr;

            result = tab;
            goto finish;
        }
    }
#endif // ENABLE(INSPECTOR_EXTENSIONS)

finish:
    if (UNLIKELY(!result)) {
        RELEASE_LOG_DEBUG(Extensions, "Tab for page %{public}llu was not found", webPageProxyIdentifier.toUInt64());
        return nullptr;
    }

    if (UNLIKELY(!result->isValid())) {
        RELEASE_LOG_ERROR(Extensions, "Tab %{public}llu has nil delegate; reference not removed via didCloseTab: before release", result->identifier().toUInt64());
        forgetTab(result->identifier());
        return nullptr;
    }

    if (ignoreExtensionAccess == IgnoreExtensionAccess::No && !result->extensionHasAccess())
        return nullptr;

    return result;
}

void WebExtensionContext::forgetTab(WebExtensionTabIdentifier identifier) const
{
    RefPtr tab = m_tabMap.take(identifier);
    if (!tab)
        return;

    [m_tabDelegateToIdentifierMap removeObjectForKey:tab->delegate()];
}

bool WebExtensionContext::canOpenNewWindow() const
{
    ASSERT(isLoaded());

    return [extensionController()->delegate() respondsToSelector:@selector(webExtensionController:openNewWindowUsingConfiguration:forExtensionContext:completionHandler:)];
}

void WebExtensionContext::openNewWindow(const WebExtensionWindowParameters& parameters, CompletionHandler<void(RefPtr<WebExtensionWindow>)>&& completionHandler)
{
    ASSERT(isLoaded());

    windowsCreate(parameters, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](Expected<std::optional<WebExtensionWindowParameters>, WebExtensionError>&& result) mutable {
        if (!result || !result.value()) {
            completionHandler(nullptr);
            return;
        }

        completionHandler(getWindow(result.value()->identifier.value()));
    });
}

void WebExtensionContext::openNewTab(const WebExtensionTabParameters& parameters, CompletionHandler<void(RefPtr<WebExtensionTab>)>&& completionHandler)
{
    tabsCreate(std::nullopt, parameters, [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](Expected<std::optional<WebExtensionTabParameters>, WebExtensionError>&& result) mutable {
        if (!result || !result.value()) {
            completionHandler(nullptr);
            return;
        }

        completionHandler(getTab(result.value()->identifier.value()));
    });
}

void WebExtensionContext::populateWindowsAndTabs()
{
    ASSERT(isLoaded());

    auto delegate = m_extensionController->delegate();

    if ([delegate respondsToSelector:@selector(webExtensionController:openWindowsForExtensionContext:)]) {
        auto *openWindows = [delegate webExtensionController:extensionController()->wrapper() openWindowsForExtensionContext:wrapper()];
        THROW_UNLESS([openWindows isKindOfClass:NSArray.class], @"Object returned by webExtensionController:openWindowsForExtensionContext: is not an array");

        for (id windowDelegate in openWindows)
            didOpenWindow(getOrCreateWindow(windowDelegate), UpdateWindowOrder::No, SuppressEvents::Yes);
    }

    if ([delegate respondsToSelector:@selector(webExtensionController:focusedWindowForExtensionContext:)]) {
        id focusedWindow = [delegate webExtensionController:extensionController()->wrapper() focusedWindowForExtensionContext:wrapper()];
        didFocusWindow(focusedWindow ? getOrCreateWindow(focusedWindow).ptr() : nullptr, SuppressEvents::Yes);
    }
}

bool WebExtensionContext::isValidWindow(const WebExtensionWindow& window)
{
    return window.isValid() && window.extensionContext() == this && m_windowMap.get(window.identifier()) == &window;
}

bool WebExtensionContext::isValidTab(const WebExtensionTab& tab)
{
    return tab.isValid() && tab.extensionContext() == this && m_tabMap.get(tab.identifier()) == &tab;
}

WebExtensionContext::WindowVector WebExtensionContext::openWindows(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    return WTF::compactMap(m_windowOrderVector, [&](auto& identifier) -> std::optional<Ref<WebExtensionWindow>> {
        RefPtr window = m_windowMap.get(identifier);
        ASSERT(window && window->isOpen());

        if (ignoreExtensionAccess == IgnoreExtensionAccess::No && !window->extensionHasAccess())
            return std::nullopt;
        return *window;
    });
}

WebExtensionContext::TabVector WebExtensionContext::openTabs(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    return WTF::compactMap(m_tabMap, [&](auto& entry) -> std::optional<Ref<WebExtensionTab>> {
        if (!entry.value->isOpen())
            return std::nullopt;
        if (ignoreExtensionAccess == IgnoreExtensionAccess::No && !entry.value->extensionHasAccess())
            return std::nullopt;
        return entry.value;
    });
}

RefPtr<WebExtensionWindow> WebExtensionContext::focusedWindow(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    if (m_focusedWindowIdentifier)
        return getWindow(m_focusedWindowIdentifier.value(), std::nullopt, ignoreExtensionAccess);
    return nullptr;
}

RefPtr<WebExtensionWindow> WebExtensionContext::frontmostWindow(IgnoreExtensionAccess ignoreExtensionAccess) const
{
    // Return the first non-null window, skipping private windows if access is denied.
    for (auto& windowIdentifier : m_windowOrderVector) {
        if (RefPtr window = getWindow(windowIdentifier, std::nullopt, ignoreExtensionAccess))
            return window;
    }

    return nullptr;
}

void WebExtensionContext::didOpenWindow(WebExtensionWindow& window, UpdateWindowOrder updateWindowOrder, SuppressEvents suppressEvents)
{
    ASSERT(isValidWindow(window));

    // The window might already be open, don't log an error.
    if (window.isOpen())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Opened window %{public}llu", window.identifier().toUInt64());

    window.didOpen();

    ASSERT(!m_windowOrderVector.contains(window.identifier()));

    if (updateWindowOrder == UpdateWindowOrder::Yes) {
        m_focusedWindowIdentifier = window.identifier();
        m_windowOrderVector.insert(0, window.identifier());
    } else {
        if (m_windowOrderVector.isEmpty())
            m_focusedWindowIdentifier = window.identifier();
        m_windowOrderVector.append(window.identifier());
    }

    for (Ref tab : window.tabs())
        didOpenTab(tab);

    if (!isLoaded() || !window.extensionHasAccess() || suppressEvents == SuppressEvents::Yes)
        return;

    fireWindowsEventIfNeeded(WebExtensionEventListenerType::WindowsOnCreated, window.parameters());
}

void WebExtensionContext::didCloseWindow(WebExtensionWindow& window)
{
    ASSERT(isValidWindow(window));

    // The window might already be closed, don't log an error.
    if (!window.isOpen())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Closed window %{public}llu", window.identifier().toUInt64());

    Ref protectedWindow { window };

    window.didClose();
    forgetWindow(window.identifier());

    for (Ref tab : window.tabs())
        didCloseTab(tab, WindowIsClosing::Yes);

    if (!isLoaded() || !window.extensionHasAccess())
        return;

    fireWindowsEventIfNeeded(WebExtensionEventListenerType::WindowsOnRemoved, window.minimalParameters());
}

void WebExtensionContext::didFocusWindow(const WebExtensionWindow* window, SuppressEvents suppressEvents)
{
    ASSERT(!window || isValidWindow(*window));

    if (window && !window->isOpen())
        return;

    if (window)
        RELEASE_LOG_DEBUG(Extensions, "Focused window %{public}llu", window->identifier().toUInt64());
    else
        RELEASE_LOG_DEBUG(Extensions, "No window focused");

    m_focusedWindowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

    if (window) {
        ASSERT(m_windowOrderVector.contains(window->identifier()));
        m_windowOrderVector.removeAll(window->identifier());
        m_windowOrderVector.insert(0, window->identifier());
    }

    if (!isLoaded() || (window && !window->extensionHasAccess()) || suppressEvents == SuppressEvents::Yes)
        return;

    fireWindowsEventIfNeeded(WebExtensionEventListenerType::WindowsOnFocusChanged, window ? std::optional(window->minimalParameters()) : std::nullopt);
}

void WebExtensionContext::didOpenTab(WebExtensionTab& tab, SuppressEvents suppressEvents)
{
    ASSERT(isValidTab(tab));

    // The tab might already be open, don't log an error.
    if (tab.isOpen())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Opened tab %{public}llu", tab.identifier().toUInt64());

    tab.didOpen();

    if (!isLoaded() || !tab.extensionHasAccess() || suppressEvents == SuppressEvents::Yes)
        return;

    fireTabsCreatedEventIfNeeded(tab.parameters());
}

void WebExtensionContext::didCloseTab(WebExtensionTab& tab, WindowIsClosing windowIsClosing, SuppressEvents suppressEvents)
{
    ASSERT(isValidTab(tab));

    // The tab might already be closed, don't log an error.
    if (!tab.isOpen()) {
        forgetTab(tab.identifier());
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Closed tab %{public}llu %{public}s", tab.identifier().toUInt64(), windowIsClosing == WindowIsClosing::Yes ? "(window closing)" : "");

    Ref protectedTab { tab };

    tab.didClose();
    forgetTab(tab.identifier());

    if (!isLoaded() || !tab.extensionHasAccess() || suppressEvents == SuppressEvents::Yes)
        return;

    RefPtr window = tab.window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;

    fireTabsRemovedEventIfNeeded(tab.identifier(), windowIdentifier, windowIsClosing);
}

void WebExtensionContext::didActivateTab(const WebExtensionTab& tab, const WebExtensionTab* previousTab)
{
    ASSERT(isValidTab(tab));
    ASSERT(!previousTab || isValidTab(*previousTab));

    // The window and tab might still be opening, don't log an error.
    if (!tab.isOpen())
        return;

    didSelectOrDeselectTabs({ const_cast<WebExtensionTab&>(tab) });

    RELEASE_LOG_DEBUG(Extensions, "Activated tab %{public}llu", tab.identifier().toUInt64());

    if (!isLoaded() || !tab.extensionHasAccess())
        return;

    RefPtr window = tab.window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;
    auto previousTabIdentifier = previousTab ? previousTab->identifier() : WebExtensionTabConstants::NoneIdentifier;

    fireTabsActivatedEventIfNeeded(previousTabIdentifier, tab.identifier(), windowIdentifier);
}

void WebExtensionContext::didSelectOrDeselectTabs(const TabSet& tabs)
{
    HashMap<WebExtensionWindowIdentifier, Vector<WebExtensionTabIdentifier>> windowToTabs;

    for (Ref tab : tabs) {
        ASSERT(isValidTab(tab));

        // The window and tabs might still be opening, don't log an error.
        if (!tab->isOpen())
            continue;

        RefPtr window = tab->window();
        if (!window || !window->extensionHasAccess() || !window->isOpen())
            continue;

        windowToTabs.ensure(window->identifier(), [&] {
            RELEASE_LOG_DEBUG(Extensions, "Selected tabs changed for window %{public}llu", window->identifier().toUInt64());

            Vector<WebExtensionTabIdentifier> result;

            for (Ref tab : window->tabs()) {
                if (!tab->isSelected())
                    continue;

                RELEASE_LOG_DEBUG(Extensions, "Selected tab %{public}llu", tab->identifier().toUInt64());

                result.append(tab->identifier());
            }

            return result;
        });
    }

    if (!isLoaded() || windowToTabs.isEmpty())
        return;

    for (auto& entry : windowToTabs)
        fireTabsHighlightedEventIfNeeded(entry.value, entry.key);
}

void WebExtensionContext::didMoveTab(WebExtensionTab& tab, size_t oldIndex, const WebExtensionWindow* oldWindow)
{
    ASSERT(isValidTab(tab));
    ASSERT(!oldWindow || isValidWindow(*oldWindow));

    if (oldWindow && !tab.isOpen()) {
        RELEASE_LOG_ERROR(Extensions, "Moved tab %{public}llu to index %{public}zu from window %{public}llu, but tab is not open", tab.identifier().toUInt64(), oldIndex, oldWindow->identifier().toUInt64());
        return;
    }

    RefPtr newWindow = tab.window();
    size_t newIndex = tab.index();

    if (newWindow && oldWindow == newWindow)
        RELEASE_LOG_DEBUG(Extensions, "Moved tab %{public}llu from index %{public}zu to index %{public}zu (in same window)", tab.identifier().toUInt64(), oldIndex, newIndex);
    else if (oldWindow && newWindow)
        RELEASE_LOG_DEBUG(Extensions, "Moved tab %{public}llu to window %{public}llu at index %{public}zu", tab.identifier().toUInt64(), newWindow->identifier().toUInt64(), newIndex);
    else if (oldWindow)
        RELEASE_LOG_DEBUG(Extensions, "Moved tab %{public}llu out of window %{public}llu", tab.identifier().toUInt64(), oldWindow->identifier().toUInt64());
    else if (newWindow)
        RELEASE_LOG_DEBUG(Extensions, "Added tab %{public}llu to window %{public}llu at index %{public}zu", tab.identifier().toUInt64(), newWindow->identifier().toUInt64(), newIndex);

    if (!oldWindow)
        didOpenTab(tab);

    if (!isLoaded() || !tab.extensionHasAccess())
        return;

    if (newWindow && oldWindow == newWindow) {
        // Window did not change, only the index.
        if (newIndex != oldIndex)
            fireTabsMovedEventIfNeeded(tab.identifier(), newWindow->identifier(), oldIndex, newIndex);
    } else if (oldWindow && newWindow) {
        // Window changed to another.
        fireTabsDetachedEventIfNeeded(tab.identifier(), oldWindow->identifier(), oldIndex);
        fireTabsAttachedEventIfNeeded(tab.identifier(), newWindow->identifier(), newIndex);
    } else if (oldWindow) {
        // Window changed to null.
        fireTabsDetachedEventIfNeeded(tab.identifier(), oldWindow->identifier(), oldIndex);
        fireTabsAttachedEventIfNeeded(tab.identifier(), WebExtensionWindowConstants::NoneIdentifier, newIndex);
    } else if (newWindow) {
        // Window changed from null.
        fireTabsDetachedEventIfNeeded(tab.identifier(), WebExtensionWindowConstants::NoneIdentifier, oldIndex);
        fireTabsAttachedEventIfNeeded(tab.identifier(), newWindow->identifier(), newIndex);
    }
}

void WebExtensionContext::didReplaceTab(WebExtensionTab& oldTab, WebExtensionTab& newTab, SuppressEvents suppressEvents)
{
    ASSERT(isValidTab(oldTab));
    ASSERT(isValidTab(newTab));

    if (oldTab == newTab) {
        RELEASE_LOG_ERROR(Extensions, "Replaced tab %{public}llu with the same tab", newTab.identifier().toUInt64());
        return;
    }

    Ref protectedOldTab { oldTab };

    didOpenTab(newTab, suppressEvents);

    if (!oldTab.isOpen()) {
        RELEASE_LOG_ERROR(Extensions, "Replaced tab %{public}llu with tab %{public}llu, but old tab is not open", oldTab.identifier().toUInt64(), newTab.identifier().toUInt64());
        forgetTab(oldTab.identifier());
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Replaced tab %{public}llu with tab %{public}llu", oldTab.identifier().toUInt64(), newTab.identifier().toUInt64());

    if (isLoaded() && newTab.extensionHasAccess() && suppressEvents == SuppressEvents::No)
        fireTabsReplacedEventIfNeeded(oldTab.identifier(), newTab.identifier());

    didCloseTab(oldTab, WindowIsClosing::No, suppressEvents);
}

void WebExtensionContext::didChangeTabProperties(WebExtensionTab& tab, OptionSet<WebExtensionTab::ChangedProperties> properties)
{
    ASSERT(isValidTab(tab));

    // The tab might still be opening, don't log an error.
    if (!tab.isOpen())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Changed tab properties (0x%{public}X) for tab %{public}llu", properties.toRaw(), tab.identifier().toUInt64());

    if (!isLoaded() || !tab.extensionHasAccess())
        return;

    bool hasChangesPending = !tab.changedProperties().isEmpty();
    tab.addChangedProperties(properties);

    // If there are already changes pending, don't schedule the event to fire again.
    if (hasChangesPending)
        return;

    constexpr auto updatedEventDelay = 25_ms;

    // Fire the updated event after a small delay to coalesce relevant changes together.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, updatedEventDelay.nanosecondsAs<int64_t>()), dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, tabIdentifier = tab.identifier()] {
        if (!isLoaded())
            return;

        // Get the tab again, it might have closed since this was scheduled.
        RefPtr tab = getTab(tabIdentifier);
        if (!tab)
            return;

        RELEASE_LOG_DEBUG(Extensions, "Firing updated tab properties (0x%{public}X) for tab %{public}llu", tab->changedProperties().toRaw(), tab->identifier().toUInt64());
        fireTabsUpdatedEventIfNeeded(tab->parameters(), tab->changedParameters());
        tab->clearChangedProperties();
    }).get());
}

void WebExtensionContext::didStartProvisionalLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& targetURL, WallTime timestamp)
{
    RefPtr tab = getTab(pageID);

    // Dispatch webNavigation events.
    if (tab && hasPermission(WKWebExtensionPermissionWebNavigation, tab.get()) && hasPermission(targetURL, tab.get())) {
        constexpr auto eventType = WebExtensionEventListenerType::WebNavigationOnBeforeNavigate;
        wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }] {
            sendToProcessesForEvent(eventType, Messages::WebExtensionContextProxy::DispatchWebNavigationEvent(eventType, tab->identifier(), frameID, parentFrameID, targetURL, timestamp));
        });
    }
}

void WebExtensionContext::didCommitLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& frameURL, WallTime timestamp)
{
    RefPtr page = WebProcessProxy::webPage(pageID);
    if (!page)
        return;

    RefPtr tab = getTab(pageID);

    if (tab && isMainFrame(frameID)) {
        // Clear tab action customizations.
        if (RefPtr tabAction = m_actionTabMap.get(*tab))
            tabAction->clearCustomizations();

        // Clear activeTab permissions and user gesture if the site changed.
        auto temporaryPattern = tab->temporaryPermissionMatchPattern();
        if (temporaryPattern && !temporaryPattern->matchesURL(frameURL))
            clearUserGesture(*tab);

        // Clear injected styles tied to this specific page.
        // FIXME: <https://webkit.org/b/262491> There is currently no way to inject CSS in specific frames based on ID's.
        auto& userContentController = page.get()->userContentController();
        m_dynamicallyInjectedUserStyleSheets.removeAllMatching([&](auto& styleSheet) {
            auto styleSheetPageID = styleSheet->userStyleSheet().pageID();
            if (!styleSheetPageID || styleSheetPageID.value() != page->webPageIDInMainFrameProcess())
                return false;

            userContentController.removeUserStyleSheet(styleSheet);
            return true;
        });
    }

    // Dispatch webNavigation events.
    if (tab && hasPermission(WKWebExtensionPermissionWebNavigation, tab.get()) && hasPermission(frameURL, tab.get())) {
        constexpr auto committedEventType = WebExtensionEventListenerType::WebNavigationOnCommitted;
        constexpr auto contentEventType = WebExtensionEventListenerType::WebNavigationOnDOMContentLoaded;

        wakeUpBackgroundContentIfNecessaryToFireEvents({ committedEventType, contentEventType }, [=, this, protectedThis = Ref { *this }] {
            sendToProcessesForEvent(committedEventType, Messages::WebExtensionContextProxy::DispatchWebNavigationEvent(committedEventType, tab->identifier(), frameID, parentFrameID, frameURL, timestamp));
            sendToProcessesForEvent(contentEventType, Messages::WebExtensionContextProxy::DispatchWebNavigationEvent(contentEventType, tab->identifier(), frameID, parentFrameID, frameURL, timestamp));
        });
    }
}

void WebExtensionContext::didFinishLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& frameURL, WallTime timestamp)
{
    RefPtr tab = getTab(pageID);

    // Dispatch webNavigation events.
    if (tab && hasPermission(WKWebExtensionPermissionWebNavigation, tab.get()) && hasPermission(frameURL, tab.get())) {
        constexpr auto eventType = WebExtensionEventListenerType::WebNavigationOnCompleted;
        wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }] {
            sendToProcessesForEvent(eventType, Messages::WebExtensionContextProxy::DispatchWebNavigationEvent(eventType, tab->identifier(), frameID, parentFrameID, frameURL, timestamp));
        });
    }
}

void WebExtensionContext::didFailLoadForFrame(WebPageProxyIdentifier pageID, WebExtensionFrameIdentifier frameID, WebExtensionFrameIdentifier parentFrameID, const URL& frameURL, WallTime timestamp)
{
    RefPtr tab = getTab(pageID);

    // Dispatch webNavigation events.
    if (tab && hasPermission(WKWebExtensionPermissionWebNavigation, tab.get()) && hasPermission(frameURL, tab.get())) {
        constexpr auto eventType = WebExtensionEventListenerType::WebNavigationOnErrorOccurred;
        wakeUpBackgroundContentIfNecessaryToFireEvents({ eventType }, [=, this, protectedThis = Ref { *this }] {
            sendToProcessesForEvent(eventType, Messages::WebExtensionContextProxy::DispatchWebNavigationEvent(eventType, tab->identifier(), frameID, parentFrameID, frameURL, timestamp));
        });
    }
}

// MARK: webRequest

bool WebExtensionContext::hasPermissionToSendWebRequestEvent(WebExtensionTab* tab, const URL& resourceURL, const ResourceLoadInfo& loadInfo)
{
    if (!tab)
        return false;

    if (!hasPermission(WKWebExtensionPermissionWebRequest, tab))
        return false;

    if (!tab->extensionHasPermission())
        return false;

    if (resourceURL.isValid() && !hasPermission(resourceURL, tab))
        return false;

    const URL& resourceLoadURL = loadInfo.originalURL;
    if (resourceLoadURL.isValid() && !hasPermission(resourceLoadURL, tab))
        return false;

    return true;
}

void WebExtensionContext::resourceLoadDidSendRequest(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceRequest& request)
{
    RefPtr tab = getTab(pageID);
    if (!hasPermissionToSendWebRequestEvent(tab.get(), request.url(), loadInfo))
        return;

    RefPtr window = tab->window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;

    auto eventTypes = { WebExtensionEventListenerType::WebRequestOnBeforeRequest, WebExtensionEventListenerType::WebRequestOnBeforeSendHeaders, WebExtensionEventListenerType::WebRequestOnSendHeaders };
    wakeUpBackgroundContentIfNecessaryToFireEvents(eventTypes, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvents(eventTypes, Messages::WebExtensionContextProxy::ResourceLoadDidSendRequest(tab->identifier(), windowIdentifier, request, loadInfo));
    });
}

void WebExtensionContext::resourceLoadDidPerformHTTPRedirection(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request)
{
    RefPtr tab = getTab(pageID);
    if (!hasPermissionToSendWebRequestEvent(tab.get(), request.url(), loadInfo))
        return;

    RefPtr window = tab->window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;

    auto eventTypes = { WebExtensionEventListenerType::WebRequestOnHeadersReceived, WebExtensionEventListenerType::WebRequestOnBeforeRedirect };
    wakeUpBackgroundContentIfNecessaryToFireEvents(eventTypes, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvents(eventTypes, Messages::WebExtensionContextProxy::ResourceLoadDidPerformHTTPRedirection(tab->identifier(), windowIdentifier, response, loadInfo, request));
    });

    // After dispatching the redirect events, also dispatch the `didSendRequest` events for the redirection.
    resourceLoadDidSendRequest(pageID, loadInfo, request);
}

void WebExtensionContext::resourceLoadDidReceiveChallenge(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::AuthenticationChallenge& challenge)
{
    RefPtr tab = getTab(pageID);
    if (!hasPermissionToSendWebRequestEvent(tab.get(), URL { }, loadInfo))
        return;

    RefPtr window = tab->window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;

    auto eventTypes = { WebExtensionEventListenerType::WebRequestOnAuthRequired };
    wakeUpBackgroundContentIfNecessaryToFireEvents(eventTypes, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvents(eventTypes, Messages::WebExtensionContextProxy::ResourceLoadDidReceiveChallenge(tab->identifier(), windowIdentifier, challenge, loadInfo));
    });
}

void WebExtensionContext::resourceLoadDidReceiveResponse(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceResponse& response)
{
    RefPtr tab = getTab(pageID);
    if (!hasPermissionToSendWebRequestEvent(tab.get(), response.url(), loadInfo))
        return;

    RefPtr window = tab->window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;

    auto eventTypes = { WebExtensionEventListenerType::WebRequestOnHeadersReceived, WebExtensionEventListenerType::WebRequestOnResponseStarted };
    wakeUpBackgroundContentIfNecessaryToFireEvents(eventTypes, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvents(eventTypes, Messages::WebExtensionContextProxy::ResourceLoadDidReceiveResponse(tab->identifier(), windowIdentifier, response, loadInfo));
    });
}

void WebExtensionContext::resourceLoadDidCompleteWithError(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceResponse& response, const WebCore::ResourceError& error)
{
    RefPtr tab = getTab(pageID);

    // If a Fetch or XHR fails due to CORS, prompt the user for permission to the URL. This won’t help the failed request, but future requests might succeed if the user grants access.
    if (error.isAccessControl() && (loadInfo.type == ResourceLoadInfo::Type::Fetch || loadInfo.type == ResourceLoadInfo::Type::XMLHTTPRequest)) {
        RELEASE_LOG_ERROR(Extensions, "Requesting permission to access URL due to CORS failure: %{sensitive}s", loadInfo.originalURL.string().utf8().data());
        requestPermissionToAccessURLs({ loadInfo.originalURL }, tab, nullptr, GrantOnCompletion::Yes, { PermissionStateOptions::RequestedWithTabsPermission, PermissionStateOptions::IncludeOptionalPermissions });
    }

    if (!hasPermissionToSendWebRequestEvent(tab.get(), response.url(), loadInfo))
        return;

    RefPtr window = tab->window();
    auto windowIdentifier = window ? window->identifier() : WebExtensionWindowConstants::NoneIdentifier;

    auto eventTypes = { WebExtensionEventListenerType::WebRequestOnErrorOccurred, WebExtensionEventListenerType::WebRequestOnCompleted };
    wakeUpBackgroundContentIfNecessaryToFireEvents(eventTypes, [=, this, protectedThis = Ref { *this }] {
        sendToProcessesForEvents(eventTypes, Messages::WebExtensionContextProxy::ResourceLoadDidCompleteWithError(tab->identifier(), windowIdentifier, response, error, loadInfo));
    });
}

WebExtensionAction& WebExtensionContext::defaultAction()
{
    if (!m_defaultAction)
        m_defaultAction = WebExtensionAction::create(*this);

    return *m_defaultAction;
}

Ref<WebExtensionAction> WebExtensionContext::getAction(WebExtensionWindow* window)
{
    if (!window)
        return defaultAction();

    if (RefPtr windowAction = m_actionWindowMap.get(*window))
        return *windowAction;

    return defaultAction();
}

Ref<WebExtensionAction> WebExtensionContext::getAction(WebExtensionTab* tab)
{
    if (!tab)
        return defaultAction();

    if (RefPtr tabAction = m_actionTabMap.get(*tab))
        return *tabAction;

    return getAction(tab->window().get());
}

Ref<WebExtensionAction> WebExtensionContext::getOrCreateAction(WebExtensionWindow* window)
{
    if (!window)
        return defaultAction();

    return m_actionWindowMap.ensure(*window, [&] {
        return WebExtensionAction::create(*this, *window);
    }).iterator->value;
}

Ref<WebExtensionAction> WebExtensionContext::getOrCreateAction(WebExtensionTab* tab)
{
    if (!tab)
        return defaultAction();

    return m_actionTabMap.ensure(*tab, [&] {
        return WebExtensionAction::create(*this, *tab);
    }).iterator->value;
}

void WebExtensionContext::performAction(WebExtensionTab* tab, UserTriggered userTriggered)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    if (tab && userTriggered == UserTriggered::Yes)
        userGesturePerformed(*tab);

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    std::optional<Ref<WebExtensionSidebar>> sidebar;
    if (m_actionClickBehavior == WebExtensionActionClickBehavior::OpenSidebar && (sidebar = getOrCreateSidebar(*tab)) && canProgrammaticallyOpenSidebar() && canProgrammaticallyCloseSidebar() && sidebar.value()->opensSidebar()) {
        if (!sidebar.value()->isOpen())
            openSidebar(sidebar.value());
        else
            closeSidebar(sidebar.value());
        return;
    }
#endif

    auto action = getOrCreateAction(tab);
    if (action->presentsPopup()) {
        action->presentPopupWhenReady();
        return;
    }

    fireActionClickedEventIfNeeded(tab);
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
WebExtensionSidebar& WebExtensionContext::defaultSidebar()
{
    if (!m_defaultSidebar)
        m_defaultSidebar = WebExtensionSidebar::create(*this, WebExtensionSidebar::IsDefault::Yes);

    return *m_defaultSidebar;
}

std::optional<Ref<WebExtensionSidebar>> WebExtensionContext::getSidebar(WebExtensionWindow const& window)
{
    if (RefPtr windowSidebar = m_sidebarWindowMap.get(window))
        return *windowSidebar;

    return std::nullopt;
}

std::optional<Ref<WebExtensionSidebar>> WebExtensionContext::getSidebar(WebExtensionTab const& tab)
{
    if (RefPtr tabSidebar = m_sidebarTabMap.get(tab))
        return *tabSidebar;

    return std::nullopt;
}

std::optional<Ref<WebExtensionSidebar>> WebExtensionContext::getOrCreateSidebar(WebExtensionWindow& window)
{
    if (!protectedExtension()->hasAnySidebar())
        return std::nullopt;

    return m_sidebarWindowMap.ensure(window, [&] {
        return WebExtensionSidebar::create(*this, window);
    }).iterator->value;
}

std::optional<Ref<WebExtensionSidebar>> WebExtensionContext::getOrCreateSidebar(WebExtensionTab& tab)
{
    if (!protectedExtension()->hasAnySidebar())
        return std::nullopt;

    return m_sidebarTabMap.ensure(tab, [&] {
        return WebExtensionSidebar::create(*this, tab);
    }).iterator->value;
}

RefPtr<WebExtensionSidebar> WebExtensionContext::getOrCreateSidebar(RefPtr<WebExtensionTab> tab)
{
    if (!protectedExtension()->hasAnySidebar())
        return nil;
    if (!tab)
        return &defaultSidebar();

    return getOrCreateSidebar(*tab.get())
        .and_then([](auto const& sidebar) { return std::optional(RefPtr<WebExtensionSidebar>(&sidebar.get())); })
        .value_or(nil);
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

const WebExtensionContext::CommandsVector& WebExtensionContext::commands()
{
    if (m_populatedCommands)
        return m_commands;

    m_commands = WTF::map(protectedExtension()->commands(), [&](auto& data) {
        return WebExtensionCommand::create(*this, data);
    });

    m_populatedCommands = true;

    return m_commands;
}

WebExtensionCommand* WebExtensionContext::command(const String& commandIdentifier)
{
    if (commandIdentifier.isEmpty())
        return nullptr;

    for (auto& command : commands()) {
        if (command->identifier() == commandIdentifier)
            return command.ptr();
    }

    return nullptr;
}

void WebExtensionContext::performCommand(WebExtensionCommand& command, UserTriggered userTriggered)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    ASSERT(command.extensionContext() == this);
    if (command.extensionContext() != this)
        return;

    auto currentWindow = frontmostWindow();
    auto activeTab = currentWindow ? currentWindow->activeTab() : nullptr;

    if (command.isActionCommand()) {
        performAction(activeTab.get(), userTriggered);
        return;
    }

    if (activeTab && userTriggered == UserTriggered::Yes)
        userGesturePerformed(*activeTab);

    fireCommandEventIfNeeded(command, activeTab.get());
}

#if TARGET_OS_IPHONE
WebExtensionCommand* WebExtensionContext::commandMatchingKeyCommand(UIKeyCommand *keyCommand)
{
    ASSERT(keyCommand);
    for (auto& command : commands()) {
        if (command->matchesKeyCommand(keyCommand))
            return command.ptr();
    }

    return nullptr;
}

bool WebExtensionContext::performCommand(UIKeyCommand *keyCommand)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return false;

    if (RefPtr result = commandMatchingKeyCommand(keyCommand)) {
        performCommand(*result, UserTriggered::Yes);
        return true;
    }

    return false;
}
#endif

#if USE(APPKIT)
WebExtensionCommand* WebExtensionContext::command(NSEvent *event)
{
    ASSERT(event);

    if (event.type != NSEventTypeKeyDown || event.isARepeat)
        return nullptr;

    for (auto& command : commands()) {
        if (command->matchesEvent(event))
            return command.ptr();
    }

    return nullptr;
}

bool WebExtensionContext::performCommand(NSEvent *event)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return false;

    if (event.type != NSEventTypeKeyDown || event.isARepeat)
        return false;

    if (RefPtr result = command(event)) {
        performCommand(*result, UserTriggered::Yes);
        return true;
    }

    return false;
}
#endif // USE(APPKIT)

NSArray *WebExtensionContext::platformMenuItems(const WebExtensionTab& tab) const
{
    WebExtensionMenuItemContextParameters contextParameters;
    contextParameters.types = WebExtensionMenuItemContextType::Tab;
    contextParameters.tabIdentifier = tab.identifier();
    contextParameters.frameURL = tab.url();

    if (auto *menuItem = singleMenuItemOrExtensionItemWithSubmenu(contextParameters))
        return @[ menuItem ];
    return @[ ];
}

WebExtensionMenuItem* WebExtensionContext::menuItem(const String& identifier) const
{
    if (identifier.isEmpty())
        return nullptr;
    return m_menuItems.get(identifier);
}

void WebExtensionContext::performMenuItem(WebExtensionMenuItem& menuItem, const WebExtensionMenuItemContextParameters& contextParameters, UserTriggered userTriggered)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    ASSERT(menuItem.extensionContext() == this);
    if (menuItem.extensionContext() != this)
        return;

    if (contextParameters.tabIdentifier) {
        RefPtr activeTab = getTab(contextParameters.tabIdentifier.value());
        if (activeTab && userTriggered == UserTriggered::Yes)
            userGesturePerformed(*activeTab);
    }

    if (RefPtr command = menuItem.command()) {
        performCommand(*command);
        return;
    }

    bool wasChecked = menuItem.toggleCheckedIfNeeded(contextParameters);
    fireMenusClickedEventIfNeeded(menuItem, wasChecked, contextParameters);
}

CocoaMenuItem *WebExtensionContext::singleMenuItemOrExtensionItemWithSubmenu(const WebExtensionMenuItemContextParameters& contextParameters) const
{
#if USE(APPKIT)
    auto *menuItems = WebExtensionMenuItem::matchingPlatformMenuItems(mainMenuItems(), contextParameters);
    if (!menuItems.count)
        return nil;

    if (menuItems.count == 1) {
        // Don't allow images for the top-level items, it isn't typical on macOS for menus.
        dynamic_objc_cast<NSMenuItem>(menuItems.firstObject).image = nil;

        return menuItems.firstObject;
    }

    auto *extensionItem = [[_WKWebExtensionMenuItem alloc] initWithTitle:protectedExtension()->displayShortName() handler:^(id) { }];
    auto *extensionSubmenu = [[NSMenu alloc] init];
    extensionSubmenu.itemArray = menuItems;
    extensionItem.submenu = extensionSubmenu;

    return extensionItem;
#else
    auto *menuItems = WebExtensionMenuItem::matchingPlatformMenuItems(mainMenuItems(), contextParameters);
    if (!menuItems.count)
        return nil;

    if (menuItems.count == 1)
        return menuItems.firstObject;

    return [UIMenu menuWithTitle:protectedExtension()->displayShortName() children:menuItems];
#endif
}

#if PLATFORM(MAC)
void WebExtensionContext::addItemsToContextMenu(WebPageProxy& page, const ContextMenuContextData& contextData, NSMenu *menu)
{
    WebExtensionMenuItemContextParameters contextParameters;

    ASSERT(contextData.webHitTestResultData());
    auto& hitTestData = contextData.webHitTestResultData().value();

    if (!hitTestData.frameInfo)
        return;

    auto& frameInfo = hitTestData.frameInfo.value();
    contextParameters.frameIdentifier = toWebExtensionFrameIdentifier(frameInfo);
    contextParameters.frameURL = frameInfo.request.url();

    RefPtr tab = getTab(page.identifier());
    if (tab)
        contextParameters.tabIdentifier = tab->identifier();

    // Don't show context menu items unless the extension has permission, or can be granted permission
    // with an activeTab user gesture if the user interacts with one of the menu items.
    if (!hasPermission(frameInfo.request.url(), tab.get()) && (!tab || !frameInfo.isMainFrame || !hasPermission(WKWebExtensionPermissionActiveTab)))
        return;

    if (!hitTestData.absoluteImageURL.isEmpty()) {
        contextParameters.types.add(WebExtensionMenuItemContextType::Image);
        contextParameters.sourceURL = URL { hitTestData.absoluteImageURL };
    }

    if (!hitTestData.absoluteMediaURL.isEmpty() && hitTestData.elementType != WebHitTestResultData::ElementType::None) {
        contextParameters.sourceURL = URL { hitTestData.absoluteMediaURL };

        switch (hitTestData.elementType) {
        case WebHitTestResultData::ElementType::None:
            ASSERT_NOT_REACHED();
            break;

        case WebHitTestResultData::ElementType::Audio:
            contextParameters.types.add(WebExtensionMenuItemContextType::Audio);
            break;

        case WebHitTestResultData::ElementType::Video:
            contextParameters.types.add(WebExtensionMenuItemContextType::Video);
            break;
        }
    }

    if (hitTestData.isContentEditable) {
        contextParameters.types.add(WebExtensionMenuItemContextType::Editable);
        contextParameters.editable = true;
    }

    if (hitTestData.isSelected && !contextData.selectedText().isEmpty()) {
        contextParameters.types.add(WebExtensionMenuItemContextType::Selection);
        contextParameters.selectionString = contextData.selectedText();
    }

    if (!hitTestData.absoluteLinkURL.isEmpty()) {
        // Links are selected when showing the context menu, so remove the Selection type since Link is more specific.
        // This matches how built-in context menus work, e.g. hiding Lookup and Translate when on a link.
        contextParameters.types.remove(WebExtensionMenuItemContextType::Selection);

        contextParameters.types.add(WebExtensionMenuItemContextType::Link);
        contextParameters.linkURL = URL { hitTestData.absoluteLinkURL };
        contextParameters.linkText = hitTestData.linkLabel;
    }

    // The Page and Frame contexts only apply if there are no other contexts.
    if (contextParameters.types.isEmpty())
        contextParameters.types.add(frameInfo.isMainFrame ? WebExtensionMenuItemContextType::Page : WebExtensionMenuItemContextType::Frame);

    if (auto *menuItem = singleMenuItemOrExtensionItemWithSubmenu(contextParameters))
        [menu addItem:menuItem];
}
#endif

void WebExtensionContext::userGesturePerformed(WebExtensionTab& tab)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    tab.setActiveUserGesture(true);

    // Nothing else to do if the extension does not have the activeTab permission.
    if (!hasPermission(WKWebExtensionPermissionActiveTab))
        return;

    if (!tab.shouldGrantPermissionsOnUserGesture())
        return;

    auto currentURL = tab.url();
    if (currentURL.isEmpty())
        return;

    switch (permissionState(currentURL, &tab)) {
    case PermissionState::DeniedImplicitly:
    case PermissionState::DeniedExplicitly:
    case PermissionState::GrantedImplicitly:
    case PermissionState::GrantedExplicitly:
        // The extension already has permission, or permission was denied, so there is nothing to do.
        return;

    case PermissionState::Unknown:
    case PermissionState::RequestedImplicitly:
    case PermissionState::RequestedExplicitly:
        // The temporary permission should be granted.
        break;
    }

    // A pattern should not exist, since it should be cleared in clearUserGesture
    // on any navigation between different hosts.
    ASSERT(!tab.temporaryPermissionMatchPattern());

    // Grant the tab a temporary permission to access to a pattern matching the current URL.
    RefPtr pattern = WebExtensionMatchPattern::getOrCreate(currentURL);
    tab.setTemporaryPermissionMatchPattern(pattern.copyRef());

    // FIXME: <https://webkit.org/b/279287> permissionsDidChange should include a tab parameter for this use-case
    if (pattern)
        permissionsDidChange(WKWebExtensionContextPermissionMatchPatternsWereGrantedNotification, MatchPatternSet { *pattern });

    // Fire the updated event now that the extension has permission to see the URL and title.
    didChangeTabProperties(tab, { WebExtensionTab::ChangedProperties::URL, WebExtensionTab::ChangedProperties::Title });
}

bool WebExtensionContext::hasActiveUserGesture(WebExtensionTab& tab) const
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return false;

    return tab.hasActiveUserGesture();
}

void WebExtensionContext::clearUserGesture(WebExtensionTab& tab)
{
    ASSERT(isLoaded());
    if (!isLoaded())
        return;

    RefPtr oldTemporaryPermissionMatchPattern = tab.temporaryPermissionMatchPattern();

    tab.setActiveUserGesture(false);
    tab.setTemporaryPermissionMatchPattern(nullptr);

    // FIXME: <https://webkit.org/b/279287> permissionsDidChange should include a tab parameter for this use-case
    if (oldTemporaryPermissionMatchPattern)
        permissionsDidChange(WKWebExtensionContextGrantedPermissionMatchPatternsWereRemovedNotification, MatchPatternSet { *oldTemporaryPermissionMatchPattern });
}

std::optional<WebCore::PageIdentifier> WebExtensionContext::backgroundPageIdentifier() const
{
    if (!m_backgroundWebView || protectedExtension()->backgroundContentIsServiceWorker())
        return std::nullopt;

    return m_backgroundWebView.get()._page->webPageIDInMainFrameProcess();
}

#if ENABLE(INSPECTOR_EXTENSIONS)
Vector<WebExtensionContext::PageIdentifierTuple> WebExtensionContext::inspectorBackgroundPageIdentifiers() const
{
    Vector<PageIdentifierTuple> result;

    for (auto entry : m_inspectorContextMap) {
        RefPtr tab = getTab(entry.value.tabIdentifier.value());
        RefPtr window = tab ? tab->window() : nullptr;

        auto tabIdentifier = tab ? std::optional(tab->identifier()) : std::nullopt;
        auto windowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

        auto *webView = entry.value.backgroundWebView.get();
        result.append({ webView._page->webPageIDInMainFrameProcess(), tabIdentifier, windowIdentifier });
    }

    return result;
}

Vector<WebExtensionContext::PageIdentifierTuple> WebExtensionContext::inspectorPageIdentifiers() const
{
    Vector<PageIdentifierTuple> result;

    for (auto [inspector, tab] : loadedInspectors()) {
        RefPtr window = tab ? tab->window() : nullptr;

        auto tabIdentifier = tab ? std::optional(tab->identifier()) : std::nullopt;
        auto windowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

        Ref protectedInspector = inspector;
        result.append({ protectedInspector->protectedInspectorPage()->webPageIDInMainFrameProcess(), tabIdentifier, windowIdentifier });
    }

    return result;
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

Vector<WebExtensionContext::PageIdentifierTuple> WebExtensionContext::popupPageIdentifiers() const
{
    Vector<PageIdentifierTuple> result;

    for (auto entry : m_popupPageActionMap) {
        RefPtr tab = entry.value->tab();
        RefPtr window = tab ? tab->window() : entry.value->window();

        auto tabIdentifier = tab ? std::optional(tab->identifier()) : std::nullopt;
        auto windowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

        result.append({ entry.key.webPageIDInMainFrameProcess(), tabIdentifier, windowIdentifier });
    }

    return result;
}

Vector<WebExtensionContext::PageIdentifierTuple> WebExtensionContext::tabPageIdentifiers() const
{
    Vector<PageIdentifierTuple> result;

    for (auto entry : m_extensionPageTabMap) {
        RefPtr tab = getTab(entry.value);
        if (!tab)
            continue;

        RefPtr window = tab->window();
        auto windowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

        result.append({ entry.key.webPageIDInMainFrameProcess(), tab->identifier(), windowIdentifier });
    }

    return result;
}

void WebExtensionContext::addPopupPage(WebPageProxy& page, WebExtensionAction& action)
{
    m_popupPageActionMap.set(page, action);

    RefPtr tab = action.tab();
    RefPtr window = tab ? tab->window() : action.window();

    auto tabIdentifier = tab ? std::optional(tab->identifier()) : std::nullopt;
    auto windowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

    Ref protectedPage = page;
    protectedPage->protectedLegacyMainFrameProcess()->send(Messages::WebExtensionContextProxy::AddPopupPageIdentifier(protectedPage->webPageIDInMainFrameProcess(), tabIdentifier, windowIdentifier), identifier());
}

void WebExtensionContext::addExtensionTabPage(WebPageProxy& page, WebExtensionTab& tab)
{
    m_extensionPageTabMap.set(page, tab.identifier());

    RefPtr window = tab.window();
    auto windowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

    page.protectedLegacyMainFrameProcess()->send(Messages::WebExtensionContextProxy::AddTabPageIdentifier(page.webPageIDInMainFrameProcess(), tab.identifier(), windowIdentifier), identifier());
}

void WebExtensionContext::enumerateExtensionPages(Function<void(WebPageProxy&, bool&)>&& action)
{
    if (!isLoaded())
        return;

    bool stop = false;
    for (Ref page : extensionController()->allPages()) {
        auto* webView = page->cocoaView().get();
        if (isURLForThisExtension(webView._requiredWebExtensionBaseURL)) {
            action(page, stop);
            if (stop)
                return;
        }
    }
}

WKWebView *WebExtensionContext::relatedWebView()
{
    ASSERT(isLoaded());

    if (m_backgroundWebView)
        return m_backgroundWebView.get();

    WKWebView *extensionWebView;
    enumerateExtensionPages([&](auto& page, bool& stop) {
        auto *webView = page.cocoaView().get();

#if ENABLE(INSPECTOR_EXTENSIONS)
        // Inspector pages use a different process pool, and should not be related to other extension web views.
        if (isInspectorBackgroundPage(webView))
            return;
#endif

        extensionWebView = webView;
        stop = true;
    });

    return extensionWebView;
}

NSString *WebExtensionContext::processDisplayName()
{
ALLOW_NONLITERAL_FORMAT_BEGIN
    return [NSString localizedStringWithFormat:WEB_UI_STRING("%@ Web Extension", "Extension's process name that appears in Activity Monitor where the parameter is the name of the extension"), static_cast<NSString *>(protectedExtension()->displayShortName())];
ALLOW_NONLITERAL_FORMAT_END
}

NSArray *WebExtensionContext::corsDisablingPatterns()
{
    NSMutableSet<NSString *> *patterns = [NSMutableSet set];

    auto grantedMatchPatterns = grantedPermissionMatchPatterns();
    for (auto& entry : grantedMatchPatterns) {
        Ref pattern = entry.key;
        [patterns addObjectsFromArray:createNSArray(pattern->expandedStrings()).get()];
    }

    return [patterns allObjects];
}

void WebExtensionContext::updateCORSDisablingPatternsOnAllExtensionPages()
{
    auto *patterns = corsDisablingPatterns();
    enumerateExtensionPages([&](auto& page, bool& stop) {
        auto *webView = page.cocoaView().get();
        webView._corsDisablingPatterns = patterns;
    });
}

WKWebViewConfiguration *WebExtensionContext::webViewConfiguration(WebViewPurpose purpose)
{
    if (!isLoaded())
        return nil;

    bool isManifestVersion3 = protectedExtension()->supportsManifestVersion(3);

    WKWebViewConfiguration *configuration = [extensionController()->protectedConfiguration()->webViewConfiguration() copy];
    configuration._contentSecurityPolicyModeForExtension = isManifestVersion3 ? _WKContentSecurityPolicyModeForExtensionManifestV3 : _WKContentSecurityPolicyModeForExtensionManifestV2;
    configuration._corsDisablingPatterns = corsDisablingPatterns();
    configuration._crossOriginAccessControlCheckEnabled = NO;
    configuration._processDisplayName = processDisplayName();
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    configuration._relatedWebView = relatedWebView();
    ALLOW_DEPRECATED_DECLARATIONS_END
    configuration._requiredWebExtensionBaseURL = baseURL();
    configuration._shouldRelaxThirdPartyCookieBlocking = YES;

    // By default extension URLs are masked, for extension pages we can relax this.
    configuration._maskedURLSchemes = [NSSet set];

    configuration.defaultWebpagePreferences._autoplayPolicy = _WKWebsiteAutoplayPolicyAllow;

    if (purpose == WebViewPurpose::Tab) {
        configuration.webExtensionController = extensionController()->wrapper();
        configuration._weakWebExtensionController = nil;
    } else {
        // Use the weak property to avoid a reference cycle while an extension web view is owned by the context.
        configuration._weakWebExtensionController = extensionController()->wrapper();
        configuration.webExtensionController = nil;
    }

    auto *preferences = configuration.preferences;
    preferences._javaScriptCanAccessClipboard = hasPermission(WKWebExtensionPermissionClipboardWrite);

    if (purpose == WebViewPurpose::Background || purpose == WebViewPurpose::Inspector) {
        // FIXME: <https://webkit.org/b/263286> Consider allowing the background page to throttle or be suspended.
        preferences._hiddenPageDOMTimerThrottlingEnabled = NO;
        preferences._pageVisibilityBasedProcessSuppressionEnabled = NO;
        preferences.inactiveSchedulingPolicy = WKInactiveSchedulingPolicyNone;
    }

    return configuration;
}

WebsiteDataStore* WebExtensionContext::websiteDataStore(std::optional<PAL::SessionID> sessionID) const
{
    RefPtr result = extensionController()->websiteDataStore(sessionID);
    if (result && !result->isPersistent() && !hasAccessToPrivateData())
        return nullptr;
    return result.get();
}

void WebExtensionContext::cookiesDidChange(API::HTTPCookieStore&)
{
    // FIXME: <https://webkit.org/b/267514> Add support for changeInfo.

    fireCookiesChangedEventIfNeeded();
}

URL WebExtensionContext::backgroundContentURL()
{
    RefPtr extension = m_extension;
    if (!extension->hasBackgroundContent())
        return { };
    return { m_baseURL, extension->backgroundContentPath() };
}

void WebExtensionContext::loadBackgroundContent(CompletionHandler<void(NSError *)>&& completionHandler)
{
    if (!protectedExtension()->hasBackgroundContent()) {
        if (completionHandler)
            completionHandler(createError(Error::NoBackgroundContent));
        return;
    }

    wakeUpBackgroundContentIfNecessary([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        if (completionHandler)
            completionHandler(backgroundContentLoadError());
    });
}

void WebExtensionContext::loadBackgroundWebViewDuringLoad()
{
    ASSERT(isLoaded());

    RefPtr extension = m_extension;
    if (!extension->hasBackgroundContent())
        return;

    m_safeToLoadBackgroundContent = true;

    if (!extension->backgroundContentIsPersistent()) {
        loadBackgroundPageListenersFromStorage();

        bool hasEventsToFire = m_shouldFireStartupEvent || m_installReason != InstallReason::None;
        if (m_backgroundContentEventListeners.isEmpty() || hasEventsToFire)
            loadBackgroundWebView();
    } else
        loadBackgroundWebView();
}

bool WebExtensionContext::isBackgroundPage(WebPageProxyIdentifier pageProxyIdentifier) const
{
    return m_backgroundWebView && m_backgroundWebView.get()._page->identifier() == pageProxyIdentifier;
}

bool WebExtensionContext::backgroundContentIsLoaded() const
{
    return m_backgroundWebView && m_backgroundContentIsLoaded && m_actionsToPerformAfterBackgroundContentLoads.isEmpty();
}

void WebExtensionContext::loadBackgroundWebViewIfNeeded()
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasBackgroundContent() || m_backgroundWebView || !safeToLoadBackgroundContent())
        return;

    loadBackgroundWebView();
}

void WebExtensionContext::loadBackgroundWebView()
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasBackgroundContent())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Loading background content");

    ASSERT(safeToLoadBackgroundContent());

    ASSERT(!m_backgroundContentIsLoaded);
    m_backgroundContentIsLoaded = false;

    ASSERT(!m_backgroundWebView);
    m_backgroundWebView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:webViewConfiguration(WebViewPurpose::Background)];
    m_backgroundWebView.get().UIDelegate = m_delegate.get();
    m_backgroundWebView.get().navigationDelegate = m_delegate.get();
    m_backgroundWebView.get().inspectable = m_inspectable;

    auto delegate = m_extensionController->delegate();
    if ([delegate respondsToSelector:@selector(_webExtensionController:didCreateBackgroundWebView:forExtensionContext:)])
        [delegate _webExtensionController:m_extensionController->wrapper() didCreateBackgroundWebView:m_backgroundWebView.get() forExtensionContext:wrapper()];

    m_backgroundWebView.get()._remoteInspectionNameOverride = backgroundWebViewInspectionName();
    clearError(Error::BackgroundContentFailedToLoad);
    m_backgroundContentLoadError = nil;

    Ref backgroundPage = *m_backgroundWebView.get()._page;
    Ref backgroundProcess = backgroundPage->protectedLegacyMainFrameProcess();

    // Use foreground activity to keep background content responsive to events.
    m_backgroundWebViewActivity = backgroundProcess->protectedThrottler()->foregroundActivity("Web Extension background content"_s);

    if (!protectedExtension()->backgroundContentIsServiceWorker()) {
        backgroundPage->protectedLegacyMainFrameProcess()->send(Messages::WebExtensionContextProxy::SetBackgroundPageIdentifier(backgroundPage->webPageIDInMainFrameProcess()), identifier());

        [m_backgroundWebView loadRequest:[NSURLRequest requestWithURL:backgroundContentURL()]];
        return;
    }

    [m_backgroundWebView _loadServiceWorker:backgroundContentURL() usingModules:protectedExtension()->backgroundContentUsesModules() completionHandler:makeBlockPtr([this, protectedThis = Ref { *this }](BOOL success) {
        if (!success) {
            m_backgroundContentLoadError = createError(Error::BackgroundContentFailedToLoad);
            recordError(backgroundContentLoadError());
            return;
        }

        performTasksAfterBackgroundContentLoads();
    }).get()];
}

void WebExtensionContext::unloadBackgroundWebView()
{
    if (!m_backgroundWebView)
        return;

    m_backgroundContentIsLoaded = false;
    m_unloadBackgroundWebViewTimer = nullptr;
    m_backgroundWebViewActivity = nullptr;

    [m_backgroundWebView _close];
    m_backgroundWebView = nil;
}

NSString *WebExtensionContext::backgroundWebViewInspectionName()
{
    if (!m_backgroundWebViewInspectionName.isEmpty())
        return m_backgroundWebViewInspectionName;

    if (protectedExtension()->backgroundContentIsServiceWorker())
        m_backgroundWebViewInspectionName = WEB_UI_FORMAT_CFSTRING("%@ — Extension Service Worker", "Label for an inspectable Web Extension service worker", protectedExtension()->displayShortName().createCFString().get());
    else
        m_backgroundWebViewInspectionName = WEB_UI_FORMAT_CFSTRING("%@ — Extension Background Page", "Label for an inspectable Web Extension background page", protectedExtension()->displayShortName().createCFString().get());

    return m_backgroundWebViewInspectionName;
}

void WebExtensionContext::setBackgroundWebViewInspectionName(const String& name)
{
    m_backgroundWebViewInspectionName = name;
    m_backgroundWebView.get()._remoteInspectionNameOverride = name;
}

static inline bool isNotRunningInTestRunner()
{
    return applicationBundleIdentifier() != "com.apple.WebKit.TestWebKitAPI"_s;
}

void WebExtensionContext::scheduleBackgroundContentToUnload()
{
    if (!m_backgroundWebView || protectedExtension()->backgroundContentIsPersistent())
        return;

#ifdef NDEBUG
    static const auto testRunnerDelayBeforeUnloading = 3_s;
#else
    static const auto testRunnerDelayBeforeUnloading = 6_s;
#endif

    static const auto delayBeforeUnloading = isNotRunningInTestRunner() ? 30_s : testRunnerDelayBeforeUnloading;

    RELEASE_LOG_DEBUG(Extensions, "Scheduling background content to unload in %{public}.0f seconds", delayBeforeUnloading.seconds());

    if (!m_unloadBackgroundWebViewTimer)
        m_unloadBackgroundWebViewTimer = makeUnique<RunLoop::Timer>(RunLoop::protectedCurrent(), this, &WebExtensionContext::unloadBackgroundContentIfPossible);
    m_unloadBackgroundWebViewTimer->startOneShot(delayBeforeUnloading);
}

void WebExtensionContext::unloadBackgroundContentIfPossible()
{
    if (!m_backgroundWebView || protectedExtension()->backgroundContentIsPersistent())
        return;

    static const auto delayForInactivePorts = isNotRunningInTestRunner() ? 2_min : 6_s;

    if (m_pendingPermissionRequests) {
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because it has pending permission requests");
        scheduleBackgroundContentToUnload();
        return;
    }

    Ref backgroundPage = *m_backgroundWebView.get()._page;
    if (pageHasOpenPorts(backgroundPage) && MonotonicTime::now() - m_lastBackgroundPortActivityTime < delayForInactivePorts) {
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because it has open, active ports");
        scheduleBackgroundContentToUnload();
        return;
    }

    if (m_backgroundWebView.get()._isBeingInspected) {
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because it is being inspected");
        scheduleBackgroundContentToUnload();
        return;
    }

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (!m_inspectorContextMap.isEmptyIgnoringNullReferences()) {
        scheduleBackgroundContentToUnload();
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because an inspector background page is open");
        return;
    }
#endif

    RELEASE_LOG_DEBUG(Extensions, "Unloading non-persistent background content");

    unloadBackgroundWebView();
}

void WebExtensionContext::determineInstallReasonDuringLoad()
{
    ASSERT(isLoaded());

    RefPtr extension = m_extension;
    String currentVersion = extension->version();
    m_previousVersion = objectForKey<NSString>(m_state, lastSeenVersionStateKey);
    m_state.get()[lastSeenVersionStateKey] = currentVersion;

    bool extensionVersionDidChange = !m_previousVersion.isEmpty() && m_previousVersion != currentVersion;

    auto *lastSeenBundleHash = objectForKey<NSData>(m_state, lastSeenBundleHashStateKey);
    auto *currentBundleHash = extension->bundleHash();
    m_state.get()[lastSeenBundleHashStateKey] = currentBundleHash;

    bool extensionDidChange = lastSeenBundleHash && currentBundleHash && ![lastSeenBundleHash isEqualToData:currentBundleHash];

    m_shouldFireStartupEvent = extensionController()->isFreshlyCreated();

    if (extensionDidChange || extensionVersionDidChange) {
        // Clear background event listeners on extension update.
        [m_state removeObjectForKey:backgroundContentEventListenersKey];
        [m_state removeObjectForKey:backgroundContentEventListenersVersionKey];

        // Clear other state that is not persistent between extension updates.
        clearDeclarativeNetRequestRulesetState();
        clearRegisteredContentScripts();

        RELEASE_LOG_DEBUG(Extensions, "Queued installed event with extension update reason");
        m_installReason = InstallReason::ExtensionUpdate;
    } else if (!m_shouldFireStartupEvent) {
        RELEASE_LOG_DEBUG(Extensions, "Queued installed event with extension install reason");
        m_installReason = InstallReason::ExtensionInstall;
    } else
        m_installReason = InstallReason::None;
}

void WebExtensionContext::loadBackgroundPageListenersFromStorage()
{
    if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
        return;

    m_backgroundContentEventListeners.clear();

    auto backgroundContentListenersVersionNumber = objectForKey<NSNumber>(m_state, backgroundContentEventListenersVersionKey).unsignedLongValue;
    if (backgroundContentListenersVersionNumber != currentBackgroundContentListenerStateVersion) {
        RELEASE_LOG_DEBUG(Extensions, "Background listener version mismatch %{public}zu != %{public}zu", backgroundContentListenersVersionNumber, currentBackgroundContentListenerStateVersion);

        [m_state removeObjectForKey:backgroundContentEventListenersKey];
        [m_state removeObjectForKey:backgroundContentEventListenersVersionKey];

        writeStateToStorage();
        return;
    }

    auto *listenersData = objectForKey<NSData>(m_state, backgroundContentEventListenersKey);
    NSCountedSet *savedListeners = [NSKeyedUnarchiver _strictlyUnarchivedObjectOfClasses:[NSSet setWithObjects:NSCountedSet.class, NSNumber.class, nil] fromData:listenersData error:nil];

    for (NSNumber *entry in savedListeners)
        m_backgroundContentEventListeners.add(static_cast<WebExtensionEventListenerType>(entry.unsignedIntValue), [savedListeners countForObject:entry]);
}

void WebExtensionContext::saveBackgroundPageListenersToStorage()
{
    if (!storageIsPersistent() || protectedExtension()->backgroundContentIsPersistent())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Saving %{public}u background content event listeners to storage", m_backgroundContentEventListeners.size());

    auto *listeners = [NSCountedSet set];
    for (auto& entry : m_backgroundContentEventListeners)
        [listeners addObject:@(static_cast<unsigned>(entry.key))];

    auto *newBackgroundPageListenersAsData = [NSKeyedArchiver archivedDataWithRootObject:listeners requiringSecureCoding:YES error:nil];
    auto *savedBackgroundPageListenersAsData = objectForKey<NSData>(m_state, backgroundContentEventListenersKey);
    [m_state setObject:newBackgroundPageListenersAsData forKey:backgroundContentEventListenersKey];

    auto *savedListenerVersionNumber = objectForKey<NSNumber>(m_state, backgroundContentEventListenersVersionKey);
    [m_state setObject:@(currentBackgroundContentListenerStateVersion) forKey:backgroundContentEventListenersVersionKey];

    bool hasListenerStateChanged = ![newBackgroundPageListenersAsData isEqualToData:savedBackgroundPageListenersAsData];
    bool hasVersionNumberChanged = savedListenerVersionNumber.unsignedLongValue != currentBackgroundContentListenerStateVersion;
    if (hasListenerStateChanged || hasVersionNumberChanged)
        writeStateToStorage();
}

void WebExtensionContext::performTasksAfterBackgroundContentLoads()
{
    if (!isLoaded())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Background content loaded");

    if (m_shouldFireStartupEvent) {
        fireRuntimeStartupEventIfNeeded();
        m_shouldFireStartupEvent = false;
    }

    if (m_installReason != InstallReason::None) {
        fireRuntimeInstalledEventIfNeeded();

        m_installReason = InstallReason::None;
        m_previousVersion = nullString();
    }

    RELEASE_LOG_DEBUG(Extensions, "Performing %{public}zu task(s) after background content loaded", m_actionsToPerformAfterBackgroundContentLoads.size());

    for (auto& action : m_actionsToPerformAfterBackgroundContentLoads)
        action();

    m_backgroundContentIsLoaded = true;
    m_actionsToPerformAfterBackgroundContentLoads.clear();

    saveBackgroundPageListenersToStorage();
    scheduleBackgroundContentToUnload();
}

void WebExtensionContext::wakeUpBackgroundContentIfNecessary(CompletionHandler<void()>&& completionHandler)
{
    if (!protectedExtension()->hasBackgroundContent()) {
        completionHandler();
        return;
    }

    scheduleBackgroundContentToUnload();

    if (backgroundContentIsLoaded()) {
        completionHandler();
        return;
    }

    RELEASE_LOG_DEBUG(Extensions, "Scheduled task for after background content loads");

    m_actionsToPerformAfterBackgroundContentLoads.append(WTFMove(completionHandler));

    loadBackgroundWebViewIfNeeded();
}

void WebExtensionContext::wakeUpBackgroundContentIfNecessaryToFireEvents(EventListenerTypeSet&& types, CompletionHandler<void()>&& completionHandler)
{
    RefPtr extension = m_extension;
    if (!extension->hasBackgroundContent()) {
        completionHandler();
        return;
    }

    if (!extension->backgroundContentIsPersistent()) {
        bool backgroundContentListensToAtLeastOneEvent = false;
        for (auto& type : types) {
            if (m_backgroundContentEventListeners.contains(type)) {
                backgroundContentListensToAtLeastOneEvent = true;
                break;
            }
        }

        // Don't load the background page if it isn't expecting these events.
        if (!backgroundContentListensToAtLeastOneEvent) {
            completionHandler();
            return;
        }
    }

    wakeUpBackgroundContentIfNecessary(WTFMove(completionHandler));
}

#ifndef NDEBUG
// This is only defined in debug builds since it has a performance impact with little benefit to release builds.
void WebExtensionContext::reportWebViewConfigurationErrorIfNeeded(const WebExtensionTab& tab) const
{
    if (!isLoaded())
        return;

    // Access the method(s) below to trigger time-of-use logging with this stack trace
    // so it is easy to catch errors where they are actionable by the app.

    tab.webView();
}
#endif

bool WebExtensionContext::decidePolicyForNavigationAction(WKWebView *webView, WKNavigationAction *navigationAction)
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    ASSERT(webView == m_backgroundWebView || isInspectorBackgroundPage(webView));
#else
    ASSERT(webView == m_backgroundWebView);
#endif

    NSURL *url = navigationAction.request.URL;
    if (!navigationAction.targetFrame.isMainFrame || isURLForThisExtension(url))
        return true;

    return false;
}

void WebExtensionContext::didFinishDocumentLoad(WKWebView *webView, WKNavigation *)
{
    if (webView != m_backgroundWebView)
        return;

    // The service worker will notify the load via a completion handler instead.
    if (protectedExtension()->backgroundContentIsServiceWorker())
        return;

    performTasksAfterBackgroundContentLoads();
}

void WebExtensionContext::didFailNavigation(WKWebView *webView, WKNavigation *, NSError *error)
{
    if (webView != m_backgroundWebView)
        return;

    m_backgroundContentLoadError = createError(Error::BackgroundContentFailedToLoad, nil, error);
    recordError(backgroundContentLoadError());

    unloadBackgroundWebView();
}

void WebExtensionContext::webViewWebContentProcessDidTerminate(WKWebView *webView)
{
    if (webView == m_backgroundWebView) {
        unloadBackgroundWebView();

        if (protectedExtension()->backgroundContentIsPersistent())
            loadBackgroundWebView();

        return;
    }

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (isInspectorBackgroundPage(webView)) {
        [webView loadRequest:[NSURLRequest requestWithURL:inspectorBackgroundPageURL()]];
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

#if PLATFORM(MAC)
void WebExtensionContext::runOpenPanel(WKWebView *, WKOpenPanelParameters *parameters, void (^completionHandler)(NSArray *URLs))
{
    auto *panel = [NSOpenPanel openPanel];
    panel.allowsMultipleSelection = parameters.allowsMultipleSelection;
    panel.canChooseDirectories = parameters.allowsDirectories;
    panel.canChooseFiles = YES;

    panel.allowedContentTypes = WebKit::mapObjects((NSArray *)parameters._allowedFileExtensions, ^(id, NSString *fileExtension) {
        return [UTType typeWithFilenameExtension:fileExtension];
    });

    [panel beginWithCompletionHandler:^(NSModalResponse result) {
        completionHandler(result == NSModalResponseOK ? panel.URLs : nil);
    }];
}
#endif // PLATFORM(MAC)

#if ENABLE(INSPECTOR_EXTENSIONS)
URL WebExtensionContext::inspectorBackgroundPageURL() const
{
    RefPtr extension = m_extension;
    if (!extension->hasInspectorBackgroundPage())
        return { };
    return { m_baseURL, extension->inspectorBackgroundPagePath() };
}

WebExtensionContext::InspectorTabVector WebExtensionContext::openInspectors(Function<bool(WebExtensionTab&, WebInspectorUIProxy&)>&& predicate) const
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasInspectorBackgroundPage())
        return { };

    InspectorTabVector result;

    for (Ref tab : openTabs()) {
        if (WKWebView *webView = tab->webView()) {
            auto *webInspector = webView._inspector;
            if (!webInspector)
                continue;

            Ref inspector = *webInspector->_inspector;
            if (inspector->isVisible() && (!predicate || predicate(tab, inspector)))
                result.append({ inspector, tab.ptr() });
        }
    }

    return result;
}

WebExtensionContext::InspectorTabVector WebExtensionContext::loadedInspectors() const
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasInspectorBackgroundPage())
        return { };

    InspectorTabVector result;

    for (auto entry : m_inspectorContextMap)
        result.append({ entry.key, getTab(entry.value.tabIdentifier.value()) });

    return result;
}

RefPtr<API::InspectorExtension> WebExtensionContext::inspectorExtension(WebPageProxyIdentifier webPageProxyIdentifier) const
{
    ASSERT(isLoaded());
    ASSERT(protectedExtension()->hasInspectorBackgroundPage());

    RefPtr<WebInspectorUIProxy> foundInspector;

    for (auto entry : m_inspectorContextMap) {
        auto *webView = entry.value.backgroundWebView.get();
        if (webView._page->identifier() == webPageProxyIdentifier)
            return entry.value.extension;
    }

    for (auto [inspector, tab] : openInspectors()) {
        Ref protectedInspector = inspector;
        if (protectedInspector->protectedInspectorPage()->identifier() == webPageProxyIdentifier) {
            const auto& inspectorContext = m_inspectorContextMap.get(inspector);
            return inspectorContext.extension;
        }
    }

    return nullptr;
}

RefPtr<WebInspectorUIProxy> WebExtensionContext::inspector(const API::InspectorExtension& inspectorExtension) const
{
    ASSERT(isLoaded());
    ASSERT(protectedExtension()->hasInspectorBackgroundPage());

    for (auto entry : m_inspectorContextMap) {
        if (entry.value.extension == &inspectorExtension)
            return &entry.key;
    }

    return nullptr;
}

HashSet<Ref<WebProcessProxy>> WebExtensionContext::processes(const API::InspectorExtension& inspectorExtension) const
{
    ASSERT(isLoaded());
    ASSERT(protectedExtension()->hasInspectorBackgroundPage());

    HashSet<Ref<WebProcessProxy>> result;

    RefPtr inspectorProxy = inspector(inspectorExtension);
    if (!inspectorProxy)
        return result;

    ASSERT(m_inspectorContextMap.contains(*inspectorProxy));

    const auto& inspectorContext = m_inspectorContextMap.get(*inspectorProxy);
    if (auto *backgroundWebView = inspectorContext.backgroundWebView.get())
        result.add(backgroundWebView._page->legacyMainFrameProcess());

    return result;
}

bool WebExtensionContext::isInspectorBackgroundPage(WKWebView *webView) const
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasInspectorBackgroundPage())
        return false;

    for (auto entry : m_inspectorContextMap) {
        if (webView == entry.value.backgroundWebView)
            return true;
    }

    return false;
}

bool WebExtensionContext::isDevToolsMessageAllowed()
{
    return isLoaded() && protectedExtension()->hasInspectorBackgroundPage();
}

void WebExtensionContext::loadInspectorBackgroundPagesDuringLoad()
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasInspectorBackgroundPage())
        return;

    for (auto [inspector, tab] : openInspectors())
        loadInspectorBackgroundPage(Ref { inspector }, *tab);
}

void WebExtensionContext::unloadInspectorBackgroundPages()
{
    if (!protectedExtension()->hasInspectorBackgroundPage())
        return;

    for (auto [inspector, tab] : loadedInspectors())
        unloadInspectorBackgroundPage(inspector);
}

void WebExtensionContext::loadInspectorBackgroundPagesForPrivateBrowsing()
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasInspectorBackgroundPage())
        return;

    auto predicate = [](WebExtensionTab& tab, WebInspectorUIProxy&) -> bool {
        return tab.isPrivate();
    };

    for (auto [inspector, tab] : openInspectors(WTFMove(predicate)))
        loadInspectorBackgroundPage(inspector, *tab);
}

void WebExtensionContext::unloadInspectorBackgroundPagesForPrivateBrowsing()
{
    if (!protectedExtension()->hasInspectorBackgroundPage())
        return;

    for (auto [inspector, tab] : loadedInspectors()) {
        if (!tab || !tab->isPrivate())
            continue;

        unloadInspectorBackgroundPage(inspector);
    }
}

void WebExtensionContext::loadInspectorBackgroundPage(WebInspectorUIProxy& inspector, WebExtensionTab& tab)
{
    ASSERT(isLoaded());
    ASSERT(protectedExtension()->hasInspectorBackgroundPage());

    ASSERT(!m_inspectorContextMap.contains(inspector));
    if (m_inspectorContextMap.contains(inspector))
        return;

    class InspectorExtensionClient : public API::InspectorExtensionClient {
        IGNORE_CLANG_WARNINGS_BEGIN("unused-local-typedef")
        WTF_MAKE_TZONE_ALLOCATED_INLINE(InspectorExtensionClient);
        IGNORE_CLANG_WARNINGS_END
    public:
        explicit InspectorExtensionClient(API::InspectorExtension& inspectorExtension, WebExtensionContext& extensionContext)
            : m_inspectorExtension(&inspectorExtension)
            , m_extensionContext(extensionContext)
        {
        }

    private:
        void didShowExtensionTab(const Inspector::ExtensionTabID& identifier, WebCore::FrameIdentifier frameIdentifier) override
        {
            if (RefPtr extensionContext = m_extensionContext.get())
                extensionContext->didShowInspectorExtensionPanel(Ref { *m_inspectorExtension }, identifier, frameIdentifier);
        }

        void didHideExtensionTab(const Inspector::ExtensionTabID& identifier) override
        {
            if (RefPtr extensionContext = m_extensionContext.get())
                extensionContext->didHideInspectorExtensionPanel(Ref { *m_inspectorExtension }, identifier);
        }

        void inspectedPageDidNavigate(const WTF::URL& url) override
        {
            if (RefPtr extensionContext = m_extensionContext.get())
                extensionContext->inspectedPageDidNavigate(Ref { *m_inspectorExtension }, url);
        }

        void effectiveAppearanceDidChange(Inspector::ExtensionAppearance appearance) override
        {
            if (RefPtr extensionContext = m_extensionContext.get())
                extensionContext->inspectorEffectiveAppearanceDidChange(Ref { *m_inspectorExtension }, appearance);
        }

        NakedPtr<API::InspectorExtension> m_inspectorExtension;
        WeakPtr<WebExtensionContext> m_extensionContext;
    };

    inspector.protectedExtensionController()->registerExtension(uniqueIdentifier(), uniqueIdentifier(), protectedExtension()->displayName(), [this, protectedThis = Ref { *this }, inspector = Ref { inspector }, tab = Ref { tab }](Expected<RefPtr<API::InspectorExtension>, Inspector::ExtensionError> result) {
        if (!result) {
            RELEASE_LOG_ERROR(Extensions, "Failed to register Inspector extension (error %{public}hhu)", enumToUnderlyingType(result.error()));
            return;
        }

        auto *inspectorWebView = inspector->protectedInspectorPage()->cocoaView().get();
        auto *inspectorWebViewConfiguration = inspectorWebView.configuration;

        auto *configuration = webViewConfiguration(WebViewPurpose::Inspector);

        // The devtools_page needs to load in the Inspector's process instead of the extension's web process.
        // Force this by relating the web view to the Inspector's web view and sharing the same process pool and data store.
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        configuration._relatedWebView = inspectorWebView;
        ALLOW_DEPRECATED_DECLARATIONS_END
        configuration._processDisplayName = inspectorWebViewConfiguration._processDisplayName;
        configuration.processPool = inspectorWebViewConfiguration.processPool;
        configuration.websiteDataStore = inspectorWebViewConfiguration.websiteDataStore;

        auto *inspectorBackgroundWebView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration];
        inspectorBackgroundWebView.UIDelegate = m_delegate.get();
        inspectorBackgroundWebView.navigationDelegate = m_delegate.get();
        inspectorBackgroundWebView.inspectable = m_inspectable;

        // In order for new web view to use the same process as _relatedWebView we need to force it here. Otherwise a process swap
        // will happen because the Inspector URL scheme and Web Extension scheme don't match.
        inspectorBackgroundWebView._page->setAlwaysUseRelatedPageProcess();

        Ref inspectorExtension = result.value().releaseNonNull();
        inspectorExtension->setClient(makeUniqueRef<InspectorExtensionClient>(inspectorExtension, *this));

        Ref process = inspectorBackgroundWebView._page->legacyMainFrameProcess();

        // Use foreground activity to keep background content responsive to events.
        Ref inspectorBackgroundWebViewActivity = process->protectedThrottler()->foregroundActivity("Web Extension Inspector background content"_s);

        InspectorContext inspectorContext {
            tab->identifier(),
            inspectorExtension.ptr(),
            inspectorBackgroundWebView,
            inspectorBackgroundWebViewActivity.ptr()
        };

        m_inspectorContextMap.set(inspector.get(), WTFMove(inspectorContext));

        RefPtr window = tab->window();
        auto windowIdentifier = window ? std::optional(window->identifier()) : std::nullopt;

        auto appearance = inspector->protectedInspectorPage()->useDarkAppearance() ? Inspector::ExtensionAppearance::Dark : Inspector::ExtensionAppearance::Light;

        ASSERT(inspectorWebView._page->legacyMainFrameProcess() == process);
        process->send(Messages::WebExtensionContextProxy::AddInspectorPageIdentifier(inspectorWebView._page->webPageIDInMainFrameProcess(), tab->identifier(), windowIdentifier), identifier());
        process->send(Messages::WebExtensionContextProxy::AddInspectorBackgroundPageIdentifier(inspectorBackgroundWebView._page->webPageIDInMainFrameProcess(), tab->identifier(), windowIdentifier), identifier());
        process->send(Messages::WebExtensionContextProxy::DispatchDevToolsPanelsThemeChangedEvent(appearance), identifier());

        [inspectorBackgroundWebView loadRequest:[NSURLRequest requestWithURL:inspectorBackgroundPageURL()]];
    });
}

void WebExtensionContext::unloadInspectorBackgroundPage(WebInspectorUIProxy& inspector)
{
    ASSERT(m_inspectorContextMap.contains(inspector));

    auto inspectorContext = m_inspectorContextMap.take(inspector);
    [inspectorContext.backgroundWebView _close];

    inspector.protectedExtensionController()->unregisterExtension(uniqueIdentifier(), [](Expected<void, Inspector::ExtensionError> result) {
        if (!result)
            RELEASE_LOG_ERROR(Extensions, "Failed to unregister Inspector extension (error %{public}hhu)", enumToUnderlyingType(result.error()));
    });
}

void WebExtensionContext::inspectorWillOpen(WebInspectorUIProxy& inspector, WebPageProxy& inspectedPage)
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasInspectorBackgroundPage())
        return;

    RefPtr tab = getTab(inspectedPage.identifier());
    if (!tab)
        return;

    loadInspectorBackgroundPage(inspector, *tab);
}

void WebExtensionContext::inspectorWillClose(WebInspectorUIProxy& inspector, WebPageProxy& inspectedPage)
{
    ASSERT(isLoaded());

    if (!protectedExtension()->hasInspectorBackgroundPage())
        return;

    unloadInspectorBackgroundPage(inspector);
}

void WebExtensionContext::didShowInspectorExtensionPanel(API::InspectorExtension& inspectorExtension, const Inspector::ExtensionTabID& identifier, WebCore::FrameIdentifier frameIdentifier) const
{
    sendToProcesses(processes(inspectorExtension), Messages::WebExtensionContextProxy::DispatchDevToolsExtensionPanelShownEvent(identifier, frameIdentifier));
}

void WebExtensionContext::didHideInspectorExtensionPanel(API::InspectorExtension& inspectorExtension, const Inspector::ExtensionTabID& identifier) const
{
    sendToProcesses(processes(inspectorExtension), Messages::WebExtensionContextProxy::DispatchDevToolsExtensionPanelHiddenEvent(identifier));
}

void WebExtensionContext::inspectedPageDidNavigate(API::InspectorExtension& inspectorExtension, const URL& url)
{
    if (!hasPermission(url))
        return;

    sendToProcesses(processes(inspectorExtension), Messages::WebExtensionContextProxy::DispatchDevToolsNetworkNavigatedEvent(url));
}

void WebExtensionContext::inspectorEffectiveAppearanceDidChange(API::InspectorExtension& inspectorExtension, Inspector::ExtensionAppearance appearance)
{
    sendToProcesses(processes(inspectorExtension), Messages::WebExtensionContextProxy::DispatchDevToolsPanelsThemeChangedEvent(appearance));
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents)
{
    if (!isLoaded())
        return;

    // Only add content for one "all hosts" pattern if the extension has the permission.
    // This avoids duplicate injected content if individual hosts are granted in addition to "all hosts".
    if (hasAccessToAllHosts()) {
        addInjectedContent(injectedContents, WebExtensionMatchPattern::allHostsAndSchemesMatchPattern());
        return;
    }

    MatchPatternSet grantedMatchPatterns;
    for (auto& pattern : currentPermissionMatchPatterns())
        grantedMatchPatterns.add(pattern);

    addInjectedContent(injectedContents, grantedMatchPatterns);
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents, const MatchPatternSet& grantedMatchPatterns)
{
    if (!isLoaded())
        return;

    if (hasAccessToAllHosts()) {
        // If this is not currently granting "all hosts", then we can return early. This means
        // the "all hosts" pattern injected content was added already, and no content needs added.
        // Continuing here would add multiple copies of injected content, one for "all hosts" and
        // another for individually granted hosts.
        if (!WebExtensionMatchPattern::patternsMatchAllHosts(grantedMatchPatterns))
            return;

        // Since we are granting "all hosts" we want to remove any previously added content since
        // "all hosts" will cover any hosts previously added, and we don't want duplicate scripts.
        MatchPatternSet patternsToRemove;
        for (auto& entry : m_injectedScriptsPerPatternMap)
            patternsToRemove.add(entry.key);

        for (auto& entry : m_injectedStyleSheetsPerPatternMap)
            patternsToRemove.add(entry.key);

        for (auto& pattern : patternsToRemove)
            removeInjectedContent(pattern);
    }

    for (auto& pattern : grantedMatchPatterns)
        addInjectedContent(injectedContents, pattern);
}

static WebCore::UserScriptInjectionTime toImpl(WebExtension::InjectionTime injectionTime)
{
    switch (injectionTime) {
    case WebExtension::InjectionTime::DocumentStart:
        return WebCore::UserScriptInjectionTime::DocumentStart;
    case WebExtension::InjectionTime::DocumentIdle:
        // FIXME: <rdar://problem/57613315> Implement idle injection time. For now, the end injection time is fine.
    case WebExtension::InjectionTime::DocumentEnd:
        return WebCore::UserScriptInjectionTime::DocumentEnd;
    }
}

API::ContentWorld& WebExtensionContext::toContentWorld(WebExtensionContentWorldType contentWorldType) const
{
    ASSERT(isLoaded());

    switch (contentWorldType) {
    case WebExtensionContentWorldType::Main:
    case WebExtensionContentWorldType::WebPage:
#if ENABLE(INSPECTOR_EXTENSIONS)
    case WebExtensionContentWorldType::Inspector:
#endif
        return API::ContentWorld::pageContentWorld();
    case WebExtensionContentWorldType::ContentScript:
        return *m_contentScriptWorld;
    case WebExtensionContentWorldType::Native:
        ASSERT_NOT_REACHED();
        return API::ContentWorld::pageContentWorld();
    }
}

void WebExtensionContext::addInjectedContent(const InjectedContentVector& injectedContents, WebExtensionMatchPattern& pattern)
{
    if (!isLoaded())
        return;

    auto scriptAddResult = m_injectedScriptsPerPatternMap.ensure(pattern, [&] {
        return UserScriptVector { };
    });

    auto styleSheetAddResult = m_injectedStyleSheetsPerPatternMap.ensure(pattern, [&] {
        return UserStyleSheetVector { };
    });

    auto& originInjectedScripts = scriptAddResult.iterator->value;
    auto& originInjectedStyleSheets = styleSheetAddResult.iterator->value;

    NSMutableSet<NSString *> *baseExcludeMatchPatternsSet = [NSMutableSet set];

    auto& deniedMatchPatterns = deniedPermissionMatchPatterns();
    for (auto& deniedEntry : deniedMatchPatterns) {
        // Granted host patterns always win over revoked host patterns. Skip any revoked "all hosts" patterns.
        // This supports the case where "all hosts" is revoked and a handful of specific hosts are granted.
        Ref deniedMatchPattern = deniedEntry.key;
        if (deniedMatchPattern->matchesAllHosts())
            continue;

        // Only revoked patterns that match the granted pattern need to be included. This limits
        // the size of the exclude match patterns list to speed up processing.
        if (!pattern.matchesPattern(deniedMatchPattern, { WebExtensionMatchPattern::Options::IgnorePaths, WebExtensionMatchPattern::Options::MatchBidirectionally }))
            continue;

        [baseExcludeMatchPatternsSet addObjectsFromArray:createNSArray(deniedMatchPattern->expandedStrings()).get()];
    }

    auto& userContentControllers = this->userContentControllers();

    for (auto& injectedContentData : injectedContents) {
        NSMutableSet<NSString *> *includeMatchPatternsSet = [NSMutableSet set];

        for (auto& includeMatchPattern : injectedContentData.includeMatchPatterns) {
            // Paths are not matched here since all we need to match at this point is scheme and host.
            // The path matching will happen in WebKit when deciding to inject content into a frame.

            // When the include pattern matches all hosts, we can generate a restricted patten here and skip
            // the more expensive calls to matchesPattern() below since we know they will match.
            if (includeMatchPattern->matchesAllHosts()) {
                auto restrictedPattern = WebExtensionMatchPattern::getOrCreate(includeMatchPattern->scheme(), pattern.host(), includeMatchPattern->path());
                if (!restrictedPattern)
                    continue;

                [includeMatchPatternsSet addObjectsFromArray:createNSArray(restrictedPattern->expandedStrings()).get()];
                continue;
            }

            // When deciding if injected content patterns match, we need to check bidirectionally.
            // This allows an extension that requests *.wikipedia.org, to still inject content when
            // it is granted more specific access to *.en.wikipedia.org.
            if (!includeMatchPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnorePaths, WebExtensionMatchPattern::Options::MatchBidirectionally }))
                continue;

            // Pick the most restrictive match pattern by comparing unidirectionally to the granted origin pattern.
            // If the include pattern still matches the granted origin pattern, it is not restrictive enough.
            // In that case we need to use the include pattern scheme and path, but with the granted pattern host.
            RefPtr restrictedPattern = includeMatchPattern.ptr();
            if (includeMatchPattern->matchesPattern(pattern, { WebExtensionMatchPattern::Options::IgnoreSchemes, WebExtensionMatchPattern::Options::IgnorePaths }))
                restrictedPattern = WebExtensionMatchPattern::getOrCreate(includeMatchPattern->scheme(), pattern.host(), includeMatchPattern->path());
            if (!restrictedPattern)
                continue;

            [includeMatchPatternsSet addObjectsFromArray:createNSArray(restrictedPattern->expandedStrings()).get()];
        }

        if (!includeMatchPatternsSet.count)
            continue;

        // FIXME: <rdar://problem/57613243> Support injecting into about:blank, honoring self.contentMatchesAboutBlank. Appending @"about:blank" to the includeMatchPatterns does not work currently.
        NSArray<NSString *> *includeMatchPatterns = includeMatchPatternsSet.allObjects;

        auto *excludeMatchPatternsSet = [NSMutableSet setWithArray:createNSArray(injectedContentData.expandedExcludeMatchPatternStrings()).get()];
        [excludeMatchPatternsSet unionSet:baseExcludeMatchPatternsSet];

        NSArray<NSString *> *excludeMatchPatterns = excludeMatchPatternsSet.allObjects;

        auto injectedFrames = injectedContentData.injectsIntoAllFrames ? WebCore::UserContentInjectedFrames::InjectInAllFrames : WebCore::UserContentInjectedFrames::InjectInTopFrameOnly;
        auto injectionTime = toImpl(injectedContentData.injectionTime);
        auto waitForNotification = WebCore::WaitForNotificationBeforeInjecting::No;
        Ref executionWorld = toContentWorld(injectedContentData.contentWorldType);
        auto styleLevel = injectedContentData.styleLevel;

        auto scriptID = injectedContentData.identifier;
        bool isRegisteredScript = !scriptID.isEmpty();

        RefPtr extension = m_extension;

        for (NSString *scriptPath : injectedContentData.scriptPaths) {
            RefPtr<API::Error> error;
            auto scriptString = extension->resourceStringForPath(scriptPath, error, WebExtension::CacheResult::Yes);
            if (!scriptString) {
                recordError(::WebKit::wrapper(error));
                continue;
            }

            Ref userScript = API::UserScript::create(WebCore::UserScript { WTFMove(scriptString), URL { m_baseURL, scriptPath }, makeVector<String>(includeMatchPatterns), makeVector<String>(excludeMatchPatterns), injectionTime, injectedFrames, waitForNotification }, executionWorld);
            originInjectedScripts.append(userScript);

            for (auto& userContentController : userContentControllers)
                userContentController.addUserScript(userScript, InjectUserScriptImmediately::Yes);

            if (isRegisteredScript) {
                RefPtr registeredScript = m_registeredScriptsMap.get(scriptID);
                ASSERT(registeredScript);
                if (!registeredScript)
                    continue;

                registeredScript->addUserScript(scriptID, userScript);
            }
        }

        for (NSString *styleSheetPath : injectedContentData.styleSheetPaths) {
            RefPtr<API::Error> error;
            auto styleSheetString = extension->resourceStringForPath(styleSheetPath, error, WebExtension::CacheResult::Yes);
            if (!styleSheetString) {
                recordError(::WebKit::wrapper(error));
                continue;
            }

            styleSheetString = localizedResourceString(styleSheetString, "text/css"_s);

            Ref userStyleSheet = API::UserStyleSheet::create(WebCore::UserStyleSheet { WTFMove(styleSheetString), URL { m_baseURL, styleSheetPath }, makeVector<String>(includeMatchPatterns), makeVector<String>(excludeMatchPatterns), injectedFrames, styleLevel, std::nullopt }, executionWorld);
            originInjectedStyleSheets.append(userStyleSheet);

            for (auto& userContentController : userContentControllers)
                userContentController.addUserStyleSheet(userStyleSheet);

            if (isRegisteredScript) {
                RefPtr registeredScript = m_registeredScriptsMap.get(scriptID);
                ASSERT(registeredScript);
                if (!registeredScript)
                    continue;

                registeredScript->addUserStyleSheet(scriptID, userStyleSheet);
            }
        }
    }
}

void WebExtensionContext::addInjectedContent(WebUserContentControllerProxy& userContentController)
{
    for (auto& entry : m_injectedScriptsPerPatternMap) {
        for (auto& userScript : entry.value)
            userContentController.addUserScript(userScript, InjectUserScriptImmediately::Yes);
    }

    for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
        for (auto& userStyleSheet : entry.value)
            userContentController.addUserStyleSheet(userStyleSheet);
    }
}

void WebExtensionContext::removeInjectedContent()
{
    if (!isLoaded())
        return;

    // Use all user content controllers in case the extension was briefly allowed in private browsing
    // and content was injected into any of those content controllers.
    for (auto& userContentController : extensionController()->allUserContentControllers()) {
        for (auto& entry : m_injectedScriptsPerPatternMap) {
            for (auto& userScript : entry.value)
                userContentController.removeUserScript(userScript);
        }

        for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
            for (auto& userStyleSheet : entry.value)
                userContentController.removeUserStyleSheet(userStyleSheet);
        }
    }

    m_injectedScriptsPerPatternMap.clear();
    m_injectedStyleSheetsPerPatternMap.clear();
}

void WebExtensionContext::removeInjectedContent(const MatchPatternSet& removedMatchPatterns)
{
    if (!isLoaded())
        return;

    for (auto& removedPattern : removedMatchPatterns)
        removeInjectedContent(removedPattern);

    // If "all hosts" was removed, then we need to add back any individual granted hosts,
    // now that the catch all pattern has been removed.
    if (WebExtensionMatchPattern::patternsMatchAllHosts(removedMatchPatterns))
        addInjectedContent();
}

void WebExtensionContext::removeInjectedContent(WebExtensionMatchPattern& pattern)
{
    if (!isLoaded())
        return;

    auto originInjectedScripts = m_injectedScriptsPerPatternMap.take(pattern);
    auto originInjectedStyleSheets = m_injectedStyleSheetsPerPatternMap.take(pattern);

    if (originInjectedScripts.isEmpty() && originInjectedStyleSheets.isEmpty())
        return;

    // Use all user content controllers in case the extension was briefly allowed in private browsing
    // and content was injected into any of those content controllers.
    for (auto& userContentController : extensionController()->allUserContentControllers()) {
        for (auto& userScript : originInjectedScripts)
            userContentController.removeUserScript(userScript);

        for (auto& userStyleSheet : originInjectedStyleSheets)
            userContentController.removeUserStyleSheet(userStyleSheet);
    }
}

void WebExtensionContext::removeInjectedContent(WebUserContentControllerProxy& userContentController)
{
    for (auto& entry : m_injectedScriptsPerPatternMap) {
        for (auto& userScript : entry.value)
            userContentController.removeUserScript(userScript);
    }

    for (auto& entry : m_injectedStyleSheetsPerPatternMap) {
        for (auto& userStyleSheet : entry.value)
            userContentController.removeUserStyleSheet(userStyleSheet);
    }
}

void WebExtensionContext::unloadDeclarativeNetRequestState()
{
    removeDeclarativeNetRequestRules();

    m_sessionRulesIDs.clear();
    m_dynamicRulesIDs.clear();
    m_matchedRules.clear();
    m_enabledStaticRulesetIDs.clear();

    m_declarativeNetRequestDynamicRulesStore = nullptr;
    m_declarativeNetRequestSessionRulesStore = nullptr;
}

String WebExtensionContext::declarativeNetRequestContentRuleListFilePath()
{
    if (!m_declarativeNetRequestContentRuleListFilePath.isEmpty())
        return m_declarativeNetRequestContentRuleListFilePath;

    auto directoryPath = storageIsPersistent() ? storageDirectory() : String(FileSystem::createTemporaryDirectory(@"DeclarativeNetRequest"));
    m_declarativeNetRequestContentRuleListFilePath = FileSystem::pathByAppendingComponent(directoryPath, "DeclarativeNetRequestContentRuleList.data"_s);

    return m_declarativeNetRequestContentRuleListFilePath;
}

void WebExtensionContext::removeDeclarativeNetRequestRules()
{
    if (!isLoaded())
        return;

    // Use all user content controllers in case the extension was briefly allowed in private browsing
    // and content was injected into any of those content controllers.
    for (auto& userContentController : extensionController()->allUserContentControllers())
        userContentController.removeContentRuleList(uniqueIdentifier());
}

void WebExtensionContext::addDeclarativeNetRequestRulesToPrivateUserContentControllers()
{
    API::ContentRuleListStore::defaultStoreSingleton().lookupContentRuleListFile(declarativeNetRequestContentRuleListFilePath(), uniqueIdentifier().isolatedCopy(), [this, protectedThis = Ref { *this }](RefPtr<API::ContentRuleList> ruleList, std::error_code) {
        if (!ruleList)
            return;

        // The extension could have been unloaded before this was called.
        if (!isLoaded())
            return;

        for (auto& controller : extensionController()->allPrivateUserContentControllers())
            controller.addContentRuleList(*ruleList, m_baseURL);
    });
}

bool WebExtensionContext::hasContentModificationRules()
{
    return declarativeNetRequestEnabledRulesetCount() || !m_sessionRulesIDs.isEmpty() || !m_dynamicRulesIDs.isEmpty();
}

static NSString *computeStringHashForContentBlockerRules(NSString *rules)
{
    SHA1 sha1;
    sha1.addUTF8Bytes(rules);

    SHA1::Digest digest;
    sha1.computeHash(digest);

    auto hashAsCString = SHA1::hexDigest(digest);
    auto hashAsString = String::fromUTF8(hashAsCString.span());
    return [hashAsString stringByAppendingString:[NSString stringWithFormat:@"-%zu", currentDeclarativeNetRequestRuleTranslatorVersion]];
}

void WebExtensionContext::compileDeclarativeNetRequestRules(NSArray *rulesData, CompletionHandler<void(bool)>&& completionHandler)
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), makeBlockPtr([this, protectedThis = Ref { *this }, rulesData = RetainPtr { rulesData }, completionHandler = WTFMove(completionHandler)]() mutable {
        NSArray<NSString *> *jsonDeserializationErrorStrings;
        auto *allJSONObjects = [_WKWebExtensionDeclarativeNetRequestTranslator jsonObjectsFromData:rulesData.get() errorStrings:&jsonDeserializationErrorStrings];

        NSArray<NSString *> *parsingErrorStrings;
        auto *allConvertedRules = [_WKWebExtensionDeclarativeNetRequestTranslator translateRules:allJSONObjects errorStrings:&parsingErrorStrings];

        auto *webKitRules = encodeJSONString(allConvertedRules, JSONOptions::FragmentsAllowed);
        if (!webKitRules) {
            dispatch_async(dispatch_get_main_queue(), makeBlockPtr([completionHandler = WTFMove(completionHandler)]() mutable {
                completionHandler(false);
            }).get());
            return;
        }

        auto *previouslyLoadedHash = objectForKey<NSString>(m_state, lastLoadedDeclarativeNetRequestHashStateKey);
        auto *hashOfWebKitRules = computeStringHashForContentBlockerRules(webKitRules);

        dispatch_async(dispatch_get_main_queue(), makeBlockPtr([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), previouslyLoadedHash = String { previouslyLoadedHash }, hashOfWebKitRules = String { hashOfWebKitRules }, webKitRules = String { webKitRules }]() mutable {
            API::ContentRuleListStore::defaultStoreSingleton().lookupContentRuleListFile(declarativeNetRequestContentRuleListFilePath(), uniqueIdentifier().isolatedCopy(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), previouslyLoadedHash, hashOfWebKitRules, webKitRules](RefPtr<API::ContentRuleList> foundRuleList, std::error_code) mutable {
                // The extension could have been unloaded before this was called.
                if (!isLoaded()) {
                    completionHandler(false);
                    return;
                }

                if (foundRuleList) {
                    if ([previouslyLoadedHash isEqualToString:hashOfWebKitRules]) {
                        for (auto& userContentController : userContentControllers())
                            userContentController.addContentRuleList(*foundRuleList, m_baseURL);

                        completionHandler(true);
                        return;
                    }
                }

                API::ContentRuleListStore::defaultStoreSingleton().compileContentRuleListFile(declarativeNetRequestContentRuleListFilePath(), uniqueIdentifier().isolatedCopy(), String(webKitRules), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), hashOfWebKitRules](RefPtr<API::ContentRuleList> ruleList, std::error_code error) mutable {
                    if (error) {
                        RELEASE_LOG_ERROR(Extensions, "Error compiling declarativeNetRequest rules: %{public}s", error.message().c_str());
                        completionHandler(false);
                        return;
                    }

                    // The extension could have been unloaded before this was called.
                    if (!isLoaded()) {
                        completionHandler(false);
                        return;
                    }

                    [m_state setObject:hashOfWebKitRules forKey:lastLoadedDeclarativeNetRequestHashStateKey];
                    writeStateToStorage();

                    for (auto& userContentController : userContentControllers())
                        userContentController.addContentRuleList(*ruleList, m_baseURL);

                    completionHandler(true);
                });
            });
        }).get());
    }).get());
}

void WebExtensionContext::loadDeclarativeNetRequestRules(CompletionHandler<void(bool)>&& completionHandler)
{
    if (!hasPermission(WKWebExtensionPermissionDeclarativeNetRequest) && !hasPermission(WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess)) {
        completionHandler(false);
        return;
    }

    auto *allJSONData = [NSMutableArray array];

    auto applyDeclarativeNetRequestRules = [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler), allJSONData = RetainPtr { allJSONData }] () mutable {
        if (!allJSONData.get().count) {
            removeDeclarativeNetRequestRules();
            API::ContentRuleListStore::defaultStoreSingleton().removeContentRuleListFile(declarativeNetRequestContentRuleListFilePath(), [completionHandler = WTFMove(completionHandler)](std::error_code error) mutable {
                completionHandler(error ? false : true);
            });
            return;
        }

        compileDeclarativeNetRequestRules(allJSONData.get(), WTFMove(completionHandler));
    };

    auto addStaticRulesets = [this, protectedThis = Ref { *this }, applyDeclarativeNetRequestRules = WTFMove(applyDeclarativeNetRequestRules), allJSONData = RetainPtr { allJSONData }] () mutable {
        RefPtr extension = m_extension;
        for (auto& ruleset : extension->declarativeNetRequestRulesets()) {
            if (!m_enabledStaticRulesetIDs.contains(ruleset.rulesetID))
                continue;

            RefPtr<API::Error> error;
            RefPtr jsonData = extension->resourceDataForPath(ruleset.jsonPath, error);
            if (!jsonData || error) {
                recordError(::WebKit::wrapper(*error));
                continue;
            }

            [allJSONData addObject:jsonData->wrapper()];
        }

        applyDeclarativeNetRequestRules();
    };

    auto addDynamicAndStaticRules = [this, protectedThis = Ref { *this }, addStaticRulesets = WTFMove(addStaticRulesets), allJSONData = RetainPtr { allJSONData }] () mutable {
        [declarativeNetRequestDynamicRulesStore() getRulesWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, addStaticRulesets = WTFMove(addStaticRulesets), allJSONData = RetainPtr { allJSONData }](NSArray *rules, NSString *errorMessage) mutable {
            if (!rules.count) {
                m_dynamicRulesIDs.clear();
                addStaticRulesets();
                return;
            }

            NSError *serializationError;
            NSData *dynamicRulesAsData = encodeJSONData(rules, JSONOptions::FragmentsAllowed, &serializationError);
            if (serializationError)
                RELEASE_LOG_ERROR(Extensions, "Unable to serialize dynamic declarativeNetRequest rules for extension with identifier %{private}@ with error: %{public}@", (NSString *)uniqueIdentifier(), privacyPreservingDescription(serializationError));
            else
                [allJSONData addObject:dynamicRulesAsData];

            HashSet<double> dynamicRuleIDs;
            for (NSDictionary<NSString *, id> *rule in rules)
                dynamicRuleIDs.add(objectForKey<NSNumber>(rule, @"id").doubleValue);

            m_dynamicRulesIDs = WTFMove(dynamicRuleIDs);

            addStaticRulesets();
        }).get()];
    };

    [declarativeNetRequestSessionRulesStore() getRulesWithCompletionHandler:makeBlockPtr([this, protectedThis = Ref { *this }, addDynamicAndStaticRules = WTFMove(addDynamicAndStaticRules), allJSONData = RetainPtr { allJSONData }](NSArray *rules, NSString *errorMessage) mutable {
        if (!rules.count) {
            m_sessionRulesIDs.clear();
            addDynamicAndStaticRules();
            return;
        }

        NSError *serializationError;
        NSData *sessionRulesAsData = encodeJSONData(rules, JSONOptions::FragmentsAllowed, &serializationError);
        if (serializationError)
            RELEASE_LOG_ERROR(Extensions, "Unable to serialize session declarativeNetRequest rules for extension with identifier %{private}@ with error: %{public}@", (NSString *)uniqueIdentifier(), privacyPreservingDescription(serializationError));
        else
            [allJSONData addObject:sessionRulesAsData];

        HashSet<double> sessionRuleIDs;
        for (NSDictionary<NSString *, id> *rule in rules)
            sessionRuleIDs.add(objectForKey<NSNumber>(rule, @"id").doubleValue);

        m_sessionRulesIDs = WTFMove(sessionRuleIDs);

        addDynamicAndStaticRules();
    }).get()];
}

bool WebExtensionContext::handleContentRuleListNotificationForTab(WebExtensionTab& tab, const URL& url, WebCore::ContentRuleListResults::Result)
{
    incrementActionCountForTab(tab, 1);

    if (!hasPermission(WKWebExtensionPermissionDeclarativeNetRequestFeedback) && !(hasPermission(WKWebExtensionPermissionDeclarativeNetRequest) && hasPermission(url, &tab)))
        return false;

    m_matchedRules.append({
        url,
        WallTime::now(),
        tab.identifier()
    });

    return true;
}

bool WebExtensionContext::purgeMatchedRulesFromBefore(const WallTime& startTime)
{
    if (m_matchedRules.isEmpty())
        return false;

    DeclarativeNetRequestMatchedRuleVector filteredMatchedRules;
    for (auto& matchedRule : m_matchedRules) {
        if (matchedRule.timeStamp >= startTime)
            filteredMatchedRules.append(matchedRule);
    }

    m_matchedRules = WTFMove(filteredMatchedRules);
    return !m_matchedRules.isEmpty();
}

_WKWebExtensionRegisteredScriptsSQLiteStore *WebExtensionContext::registeredContentScriptsStore()
{
    if (!m_registeredContentScriptsStorage)
        m_registeredContentScriptsStorage = [[_WKWebExtensionRegisteredScriptsSQLiteStore alloc] initWithUniqueIdentifier:m_uniqueIdentifier directory:storageDirectory() usesInMemoryDatabase:!storageIsPersistent()];
    return m_registeredContentScriptsStorage.get();
}

void WebExtensionContext::setSessionStorageAllowedInContentScripts(bool allowed)
{
    m_isSessionStorageAllowedInContentScripts = allowed;

    [m_state setObject:@(allowed) forKey:sessionStorageAllowedInContentScriptsKey];

    writeStateToStorage();

    if (!isLoaded())
        return;

    extensionController()->sendToAllProcesses(Messages::WebExtensionContextProxy::SetStorageAccessLevel(allowed), identifier());
}

size_t WebExtensionContext::quotaForStorageType(WebExtensionDataType storageType)
{
    switch (storageType) {
    case WebExtensionDataType::Local:
        return hasPermission(WKWebExtensionPermissionUnlimitedStorage) ? webExtensionUnlimitedStorageQuotaBytes : webExtensionStorageAreaLocalQuotaBytes;
    case WebExtensionDataType::Session:
        return webExtensionStorageAreaSessionQuotaBytes;
    case WebExtensionDataType::Sync:
        return webExtensionStorageAreaSyncQuotaBytes;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

_WKWebExtensionStorageSQLiteStore *WebExtensionContext::localStorageStore()
{
    if (!m_localStorageStore)
        m_localStorageStore = [[_WKWebExtensionStorageSQLiteStore alloc] initWithUniqueIdentifier:m_uniqueIdentifier storageType:WebExtensionDataType::Local directory:storageDirectory() usesInMemoryDatabase:!storageIsPersistent()];
    return m_localStorageStore.get();
}

_WKWebExtensionStorageSQLiteStore *WebExtensionContext::sessionStorageStore()
{
    if (!m_sessionStorageStore)
        m_sessionStorageStore = [[_WKWebExtensionStorageSQLiteStore alloc] initWithUniqueIdentifier:m_uniqueIdentifier storageType:WebExtensionDataType::Session directory:storageDirectory() usesInMemoryDatabase:true];
    return m_sessionStorageStore.get();
}

_WKWebExtensionStorageSQLiteStore *WebExtensionContext::syncStorageStore()
{
    if (!m_syncStorageStore)
        m_syncStorageStore = [[_WKWebExtensionStorageSQLiteStore alloc] initWithUniqueIdentifier:m_uniqueIdentifier storageType:WebExtensionDataType::Sync directory:storageDirectory() usesInMemoryDatabase:!storageIsPersistent()];
    return m_syncStorageStore.get();
}

_WKWebExtensionStorageSQLiteStore *WebExtensionContext::storageForType(WebExtensionDataType storageType)
{
    switch (storageType) {
    case WebExtensionDataType::Local:
        return localStorageStore();
    case WebExtensionDataType::Session:
        return sessionStorageStore();
    case WebExtensionDataType::Sync:
        return syncStorageStore();
    }

    ASSERT_NOT_REACHED();
    return nil;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
