/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebExtensionContext.h"

#include "WebExtensionContextProxyMessages.h"
#include "WebExtensionPermission.h"
#include "WebKitNavigationActionPrivate.h"
#include "WebKitSettingsPrivate.h"
#include "WebKitWebExtensionContextPrivate.h"
#include "WebKitWebExtensionPrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include "WebsiteDataFetchOption.h"
#include <glib.h>
#include <wtf/glib/Application.h>

#if ENABLE(WK_WEB_EXTENSIONS)

#if ENABLE(2022_GLIB_API)

static constexpr auto groupNameStateKey = "ExtensionState"_s;
static constexpr auto backgroundContentEventListenersKey = "BackgroundContentEventListeners"_s;
static constexpr auto backgroundContentEventListenersVersionKey = "BackgroundContentEventListenersVersion"_s;
static constexpr auto lastSeenBaseURLStateKey = "LastSeenBaseURL"_s;
static constexpr auto lastSeenVersionStateKey = "LastSeenVersion"_s;
static constexpr auto lastSeenDisplayNameStateKey = "LastSeenDisplayName"_s;

static constexpr auto storageAccessLevelsKey = "StorageAccessLevels"_s;

// Update this value when any changes are made to the WebExtensionEventListenerType enum.
static constexpr auto currentBackgroundContentListenerStateVersion = 5;

static gboolean onDecidePolicy(WebKitWebView *webView, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, WebKit::WebExtensionContext* context)
{
    ASSERT(context);

    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
        return FALSE;

    if (context->decidePolicyForNavigationAction(webView, WEBKIT_NAVIGATION_POLICY_DECISION(decision)))
        webkit_policy_decision_use(decision);
    else
        webkit_policy_decision_ignore(decision);

    return TRUE;
}

static void onDidFinishDocumentLoad(WebKitWebView *webView, WebKitLoadEvent loadEvent, WebKit::WebExtensionContext* context)
{
    ASSERT(context);

    // FIXME: Match Cocoa's didFinishDocumentLoad which doesn't wait on all subresources
    switch (loadEvent) {
    case WEBKIT_LOAD_FINISHED:
        context->didFinishDocumentLoad(webView);
    default:
        return;
    }
}

static gboolean onDidFailNavigation(WebKitWebView *webView, WebKitLoadEvent loadEvent, gchar* failingURI, GError* error, WebKit::WebExtensionContext* context)
{
    ASSERT(context);
    ASSERT(error);

    context->didFailNavigation(webView, API::Error::create({ String::fromUTF8(g_quark_to_string(error->domain)), error->code, URL { String::fromUTF8(failingURI) }, String::fromUTF8(error->message) }));

    return TRUE;
}

static void onWebProcessTerminated(WebKitWebView *webView, WebKitWebProcessTerminationReason reason, WebKit::WebExtensionContext* context)
{
    ASSERT(context);

    context->webViewWebContentProcessDidTerminate(webView);
}

namespace WebKit {

WebExtensionContext::WebExtensionContext(WebKitWebExtensionContext* contextObject)
    : WebExtensionContext()
{
    m_extension = webkitWebExtensionToImpl(webkit_web_extension_context_get_web_extension(contextObject)).get();
    m_baseURL = URL { makeString("webkit-extension://"_s, uniqueIdentifier(), '/') };
    m_delegate.reset(contextObject);
}

WebExtensionContext::~WebExtensionContext()
{
    unloadBackgroundWebView();
}

void WebExtensionContext::recordError(Ref<API::Error> error)
{
    RELEASE_LOG_ERROR(Extensions, "Error recorded: %s", error->localizedDescription().utf8());

    // Only the first occurrence of each error is recorded in the array. This prevents duplicate errors,
    // such as repeated "resource not found" errors, from being included multiple times.
    if (m_errors.containsIf([&](auto& existingError) { return existingError->localizedDescription() == error->localizedDescription(); }))
        return;

    m_errors.append(error);
}

void WebExtensionContext::clearError(Error error)
{
    if (!m_errors.size())
        return;

    auto errorCode = toAPIError(error);
    m_errors.removeAllMatching([&](auto& error) {
        return error->errorCode() == errorCode;
    });
}

std::expected<bool, RefPtr<API::Error>> WebExtensionContext::load(WebExtensionController& controller, String storageDirectory)
{
    if (isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded");
        return makeUnexpected(createError(Error::AlreadyLoaded));
    }

    m_storageDirectory = storageDirectory;
    m_extensionController = controller;
    m_contentScriptWorld = API::ContentWorld::sharedWorldWithName(makeString("WebExtension-"_s, m_uniqueIdentifier));

    readStateFromStorage();

    GUniquePtr<char> baseURI(g_key_file_get_string(m_state.get(), groupNameStateKey, lastSeenBaseURLStateKey, nullptr));
    auto lastSeenBaseURL = URL { String::fromUTF8(baseURI.get()) };
    g_key_file_set_string(m_state.get(), groupNameStateKey, lastSeenBaseURLStateKey, m_baseURL.string().utf8().legacyCStringPointer());

    if (auto displayName = protect(m_extension)->displayName(); !displayName.isEmpty())
        g_key_file_set_string(m_state.get(), groupNameStateKey, lastSeenDisplayNameStateKey, displayName.utf8().legacyCStringPointer());

    loadStorageAccessLevelsFromStorage();

    determineInstallReasonDuringLoad();

    writeStateToStorage();

    moveLocalStorageIfNeeded(lastSeenBaseURL, [this, protectedThis = Ref { *this }] {
        // The extension could have been unloaded before this was called.
        if (!isLoaded())
            return;

        removeStaleExtensionWebsiteData();

        m_safeToInjectContent = true;

        loadBackgroundWebViewDuringLoad();

#if ENABLE(INSPECTOR_EXTENSIONS)
        loadInspectorBackgroundPagesDuringLoad();
#endif

        // Notify the WebProcess that the extension loaded before we inject content scripts.
        // This will ensure that the content world is set up correctly (e.g. configured with the `browser` namespace).
        if (RefPtr controller = extensionController())
            controller->dispatchDidLoad(*this);

        addInjectedContent();
    });

    g_object_notify(G_OBJECT(m_wrapper.get()), "loaded");

    return true;
}

std::expected<bool, RefPtr<API::Error>> WebExtensionContext::unload()
{
    if (!isLoaded()) {
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        return makeUnexpected(createError(Error::NotLoaded));
    }

    writeStateToStorage();

    unloadBackgroundWebView();
#if ENABLE(WK_WEB_EXTENSIONS_OFFSCREEN)
    unloadOffscreenWebView();
#endif

    removeInjectedContent();

    invalidateStorage();
    unloadDeclarativeNetRequestState();

    m_privilegedIdentifier = std::nullopt;

    m_actionsToPerformAfterBackgroundContentLoads.clear();
    m_backgroundContentEventListeners.clear();
    m_backgroundContentHasLoadedOnce = false;
    m_eventListenerFrames.clear();
    m_installReason = InstallReason::None;
    m_previousVersion = nullString();
    m_safeToLoadBackgroundContent = false;
    m_backgroundContentLoadError = nullptr;

    m_registeredScriptsMap.clear();
    m_dynamicallyInjectedUserStyleSheets.clear();
    m_injectedScriptsPerPatternMap.clear();
    m_injectedStyleSheetsPerPatternMap.clear();
    m_safeToInjectContent = false;

    m_extensionController = nullptr;
    m_contentScriptWorld = nullptr;

    m_tabMap.clear();
    m_extensionPageTabMap.clear();

    m_windowMap.clear();
    m_windowOrderVector.clear();
    m_focusedWindowIdentifier = std::nullopt;

    m_actionWindowMap.clear();
    m_actionTabMap.clear();
    m_defaultAction = nullptr;
#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
    m_sidebarWindowMap.clear();
    m_sidebarTabMap.clear();
    m_sidebarPageMap.clear();
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

    g_object_notify(G_OBJECT(m_wrapper.get()), "loaded");

    return true;
}

GRefPtr<GKeyFile> WebExtensionContext::currentState() const
{
    return m_state;
}

GRefPtr<GKeyFile> WebExtensionContext::readStateFromPath(const String& stateFilePath)
{
    GRefPtr<GKeyFile> stateFile(adoptGRef(g_key_file_new()));
    GUniqueOutPtr<GError> error;

    g_key_file_load_from_file(stateFile.get(), stateFilePath.utf8().legacyCStringPointer(), G_KEY_FILE_NONE, &error.outPtr());
    if (error && !g_error_matches(error.get(), g_file_error_quark(), G_FILE_ERROR_NOENT))
        RELEASE_LOG_ERROR(Extensions, "Failed to coordinate reading extension state: %" PUBLIC_LOG_STRING, error->message);

    return stateFile;
}

bool WebExtensionContext::readLastBaseURLFromState(const String& filePath, URL& outLastBaseURL)
{
    GRefPtr<GKeyFile> state(readStateFromPath(filePath));

    GUniquePtr<char> baseURL(g_key_file_get_string(state.get(), groupNameStateKey, lastSeenBaseURLStateKey, nullptr));
    if (baseURL)
        outLastBaseURL = URL { String::fromUTF8(baseURL.get()) };

    return outLastBaseURL.isValid();
}

bool WebExtensionContext::readDisplayNameFromState(const String& filePath, String& outDisplayName)
{
    GRefPtr<GKeyFile> state(readStateFromPath(filePath));

    GUniquePtr<char> displayName(g_key_file_get_string(state.get(), groupNameStateKey, lastSeenDisplayNameStateKey, nullptr));
    if (displayName)
        outDisplayName = String::fromUTF8(displayName.get());

    return !outDisplayName.isEmpty();
}

GRefPtr<GKeyFile> WebExtensionContext::readStateFromStorage()
{
    if (!storageIsPersistent()) {
        if (!m_state) {
            GRefPtr<GKeyFile> stateFile(adoptGRef(g_key_file_new()));
            m_state = stateFile;
        }
        return m_state;
    }

    auto savedState = readStateFromPath(stateFilePath());
    m_state = savedState;
    return savedState;
}

void WebExtensionContext::writeStateToStorage() const
{
    if (!storageIsPersistent())
        return;

    GUniqueOutPtr<GError> error;

    if (!currentState())
        return;

    if (!g_key_file_save_to_file(currentState().get(), stateFilePath().utf8().legacyCStringPointer(), &error.outPtr()))
        RELEASE_LOG_ERROR(Extensions, "Unable to save extension state: %" PUBLIC_LOG_STRING, error->message);
}

void WebExtensionContext::enumerateExtensionPages(NOESCAPE const Function<void(WebPageProxy&, bool&)>& action)
{
    if (!isLoaded())
        return;

    bool stop = false;
    for (Ref page : extensionController()->allPages()) {
        WebKitWebView* webView = page->platformView();
        if (auto* context = webkitWebViewGetWebExtensionContext(webView)) {
            if (isURLForThisExtension(webkitWebExtensionContextToImpl(context)->baseURL())) {
                action(page, stop);
                if (stop)
                    return;
            }
        }
    }
}

void WebExtensionContext::moveLocalStorageIfNeeded(const URL& previousBaseURL, CompletionHandler<void()>&& completionHandler)
{
    if (previousBaseURL == baseURL()) {
        completionHandler();
        return;
    }

    if (!m_backgroundWebView) {
        completionHandler();
        return;
    }

    RefPtr dataStore = extensionController()->websiteDataStore();

    auto oldOrigin = WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(previousBaseURL);
    auto newOrigin = WebCore::SecurityOriginData::fromURLWithoutStrictOpaqueness(baseURL());
    dataStore->renameOriginInWebsiteData(WTF::move(oldOrigin), WTF::move(newOrigin), { WebsiteDataType::IndexedDBDatabases, WebsiteDataType::LocalStorage }, [completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler();
    });
}

static OptionSet<WebsiteDataType> allWebsiteDataTypes()
{
    return toWebsiteDataTypes(WEBKIT_WEBSITE_DATA_ALL);
}

void WebExtensionContext::removeStaleExtensionWebsiteData()
{
    if (!storageIsPersistent())
        return;

    RefPtr controller = extensionController();
    if (!controller || !controller->markDidRemoveStaleExtensionWebsiteData())
        return;

    auto sentinelPath = FileSystem::pathByAppendingComponent(controller->configuration().storageDirectory(), "StaleExtensionOriginsCleared"_s);
    if (FileSystem::fileExists(sentinelPath))
        return;

    RefPtr dataStore = extensionController()->websiteDataStore();

    auto dataTypes = allWebsiteDataTypes();
    dataStore->fetchData(dataTypes, { WebsiteDataFetchOption::IncludeAllOrigins }, [protectedThis = Ref { *this }, dataTypes, dataStore, sentinelPath](Vector<WebsiteDataRecord> records) {
        RefPtr controller = protectedThis->extensionController();
        if (!controller)
            return;

        auto activeExtensionURLs = controller->activeExtensionURLs();

        Vector<WebsiteDataRecord> staleRecords;
        for (auto& record : records) {
            WebsiteDataRecord staleRecord;
            for (auto& origin : record.origins) {
                if (WebExtensionMatchPattern::isWebExtensionURL(origin.toURL()) && !activeExtensionURLs.contains(origin.toURL().protocolHostAndPort().convertToASCIILowercase()))
                    staleRecord.origins.add(origin);
            }
            if (!staleRecord.origins.isEmpty()) {
                staleRecord.types = record.types;
                staleRecords.append(WTF::move(staleRecord));
            }
        }

        if (staleRecords.isEmpty()) {
            FileSystem::overwriteEntireFile(sentinelPath, { });
            return;
        }

        dataStore->removeData(dataTypes, staleRecords, [sentinelPath] {
            FileSystem::overwriteEntireFile(sentinelPath, { });
        });
    });
}

void WebExtensionContext::loadStorageAccessLevelsFromStorage()
{
    bool hasSavedLevels = g_key_file_has_group(m_state.get(), storageAccessLevelsKey);

    if (hasSavedLevels) {
        for (auto dataType : allWebExtensionDataTypes()) {
            GUniquePtr<char> accessLevelString(g_key_file_get_string(m_state.get(), storageAccessLevelsKey, toAPIString(dataType).utf8().legacyCStringPointer(), nullptr));
            if (auto accessLevel = toWebExtensionStorageAccessLevel(String::fromUTF8(accessLevelString.get())))
                m_storageAccessLevels.set(dataType, *accessLevel);
        }

        return;
    }
}

WebKitWebView* WebExtensionContext::relatedWebView()
{
    ASSERT(isLoaded());

    if (m_backgroundWebView)
        return m_backgroundWebView.get();

    GWeakPtr<WebKitWebView> extensionWebView;
    enumerateExtensionPages([&](auto& page, bool& stop) {
        GRefPtr<WebKitWebView> webView(adoptGRef(page.platformView()));

#if ENABLE(INSPECTOR_EXTENSIONS)
        // Inspector pages use a different process pool, and should not be related to other extension web views.
        if (isInspectorBackgroundPage(webView))
            return;
#endif

        extensionWebView.reset(webView.leakRef());
        stop = true;
    });

    if (!extensionWebView)
        return nullptr;

    return extensionWebView.get();
}

GRefPtr<WebKitSettings> WebExtensionContext::webViewConfiguration(WebViewPurpose purpose)
{
    if (!isLoaded())
        return nullptr;

    GRefPtr<WebKitSettings> settings(adoptGRef(webkit_settings_new()));
    WebKit::WebPreferences* preferences = webkitSettingsGetPreferences(settings.get());

    webkit_settings_set_javascript_can_access_clipboard(settings.get(), hasPermission(WebExtensionPermission::clipboardWrite()));

    if (purpose == WebViewPurpose::Background || purpose == WebViewPurpose::Inspector) {
        // FIXME: <https://webkit.org/b/263286> Consider allowing the background page to throttle or be suspended.
        preferences->setHiddenPageDOMTimerThrottlingEnabled(false);
        preferences->setPageVisibilityBasedProcessSuppressionEnabled(false);
        preferences->setShouldTakeNearSuspendedAssertions(true);
        preferences->setBackgroundWebContentRunningBoardThrottlingEnabled(false);
        preferences->setShouldDropNearSuspendedAssertionAfterDelay(false);
    }

    return settings;
}

bool WebExtensionContext::backgroundContentIsLoaded() const
{
    return m_backgroundWebView && m_backgroundContentIsLoaded && m_actionsToPerformAfterBackgroundContentLoads.isEmpty();
}

void WebExtensionContext::loadBackgroundWebViewIfNeeded()
{
    ASSERT(isLoaded());

    if (!protect(extension())->hasBackgroundContent() || m_backgroundWebView || !safeToLoadBackgroundContent())
        return;

    loadBackgroundWebView();
}

void WebExtensionContext::loadBackgroundWebView()
{
    ASSERT(isLoaded());

    if (!protect(extension())->hasBackgroundContent())
        return;

    RefPtr extensionController = this->extensionController();
    if (!extensionController)
        return;

    RELEASE_LOG_DEBUG(Extensions, "Loading background content");

    ASSERT(safeToLoadBackgroundContent());

    ASSERT(!m_backgroundContentIsLoaded);
    m_backgroundContentIsLoaded = false;

    ASSERT(!m_backgroundWebView);

    bool isManifestVersion3 = protect(extension())->supportsManifestVersion(3);

    GRefPtr<WebKitSettings> settings = webViewConfiguration(WebViewPurpose::Background);
    WebKit::WebPreferences* preferences = webkitSettingsGetPreferences(settings.get());
    WebKitWebView* floatingView = WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-extension-mode", isManifestVersion3 ? WEBKIT_WEB_EXTENSION_MODE_MANIFESTV3 : WEBKIT_WEB_EXTENSION_MODE_MANIFESTV2,
        "related-view", preferences->siteIsolationEnabled() ? nullptr : relatedWebView(),
        "settings", settings.get(),
        "web-extension-context", m_delegate.get(),
        nullptr));
#if PLATFORM(GTK)
    GRefPtr<WebKitWebView> webView = adoptGRef(WEBKIT_WEB_VIEW(g_object_ref_sink(floatingView)));
#else
    GRefPtr<WebKitWebView> webView = adoptGRef(floatingView);
#endif
    m_backgroundWebView = webView;

    g_signal_connect(m_backgroundWebView.get(), "decide-policy", G_CALLBACK(onDecidePolicy), this);
    g_signal_connect(m_backgroundWebView.get(), "load-changed", G_CALLBACK(onDidFinishDocumentLoad), this);
    g_signal_connect(m_backgroundWebView.get(), "load-failed", G_CALLBACK(onDidFailNavigation), this);
    g_signal_connect(m_backgroundWebView.get(), "web-process-terminated", G_CALLBACK(onWebProcessTerminated), this);

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());
    pageProxy->setInspectable(m_inspectable);

    setBackgroundWebViewInspectionName(backgroundWebViewInspectionName());
    clearError(Error::BackgroundContentFailedToLoad);
    m_backgroundContentLoadError = nullptr;

    Ref backgroundProcess = pageProxy->siteIsolatedProcess();

    bool siteIsolationEnabled = protect(preferences)->siteIsolationEnabled();
    constexpr ASCIILiteral activityName = "Web Extension background content"_s;

    // Use foreground activity to keep background content responsive to events.
    if (siteIsolationEnabled)
        m_backgroundWebViewActivity = protect(pageProxy->activityGroupContext())->foregroundProcessActivityGroup(activityName);
    else
        m_backgroundWebViewActivity = protect(backgroundProcess->throttler())->foregroundActivity(activityName);

    if (!protect(extension())->backgroundContentIsServiceWorker()) {
        GRefPtr<WebKitURIRequest> uriRequest(adoptGRef(webkit_uri_request_new(backgroundContentURL().string().utf8().legacyCStringPointer())));
        webkit_web_view_load_request(m_backgroundWebView.get(), uriRequest.get());
        return;
    }

    webkitWebViewLoadServiceWorker(m_backgroundWebView.get(), backgroundContentURL(), protect(extension())->backgroundContentUsesModules(), [this, protectedThis = Ref { *this }](bool success) {
        if (!success) {
            m_backgroundContentLoadError = createError(Error::BackgroundContentFailedToLoad);
            recordErrorIfNeeded(backgroundContentLoadError());
            return;
        }

        performTasksAfterBackgroundContentLoads();
    });
}

void WebExtensionContext::setBackgroundWebViewInspectionName(const String& name)
{
    m_backgroundWebViewInspectionName = name;

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());
    pageProxy->setRemoteInspectionNameOverride(name);
}

void WebExtensionContext::unloadBackgroundContentIfPossible()
{
    if (!m_backgroundWebView || protect(extension())->backgroundContentIsPersistent())
        return;

    if (m_pendingPermissionRequests) {
        RELEASE_LOG_DEBUG(Extensions, "Not unloading background content because it has pending permission requests");
        scheduleBackgroundContentToUnload();
        return;
    }

    Ref pageProxy = webkitWebViewGetPage(m_backgroundWebView.get());

    if (pageProxy->hasInspectorFrontend()) {
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

void WebExtensionContext::unloadBackgroundWebView()
{
    if (!m_backgroundWebView)
        return;

    m_backgroundContentIsLoaded = false;
    m_unloadBackgroundWebViewTimer = nullptr;
    m_backgroundWebViewActivity = { };

    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(onDecidePolicy), this);
    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(onDidFinishDocumentLoad), this);
    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(onDidFailNavigation), this);
    g_signal_handlers_disconnect_by_func(m_backgroundWebView.get(), reinterpret_cast<gpointer>(onWebProcessTerminated), this);
    webkit_web_view_try_close(m_backgroundWebView.get());
    RunLoop::mainSingleton().dispatch([webView = std::exchange(m_backgroundWebView, nullptr)] { });
}

void WebExtensionContext::determineInstallReasonDuringLoad()
{
    ASSERT(isLoaded());

    RefPtr extension = m_extension;
    String currentVersion = extension->version();
    GUniquePtr<char> previousVersion(g_key_file_get_string(m_state.get(), groupNameStateKey, lastSeenVersionStateKey, nullptr));
    m_previousVersion = String::fromUTF8(previousVersion.get());
    g_key_file_set_string(m_state.get(), groupNameStateKey, lastSeenVersionStateKey, currentVersion.utf8().legacyCStringPointer());

    bool extensionVersionDidChange = !m_previousVersion.isEmpty() && m_previousVersion != currentVersion;

    m_shouldFireStartupEvent = extensionController()->isFreshlyCreated();

    if (extensionVersionDidChange) {
        // Clear background event listeners on extension update.
        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr);
        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);

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
    if (!storageIsPersistent() || protect(extension())->backgroundContentIsPersistent())
        return;

    m_backgroundContentEventListeners.clear();

    auto backgroundContentListenersVersionNumber = g_key_file_get_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);
    if (backgroundContentListenersVersionNumber != currentBackgroundContentListenerStateVersion) {
        RELEASE_LOG_DEBUG(Extensions, "Background listener version mismatch %zu != %i", backgroundContentListenersVersionNumber, currentBackgroundContentListenerStateVersion);

        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, nullptr);
        g_key_file_remove_key(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);

        writeStateToStorage();
        return;
    }

    gsize savedListenersLength = 0;
    GUniquePtr<int> listenersData(g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, &savedListenersLength, nullptr));
    Vector<int> savedListenerData;
    if (listenersData)
        savedListenerData = Vector<int>(unsafeMakeSpan<const int>(listenersData.get(), savedListenersLength));

    HashCountedSet<int> savedListeners;
    for (auto listener : savedListenerData)
        savedListeners.add(listener);

    for (auto entry : savedListeners)
        m_backgroundContentEventListeners.add(static_cast<WebExtensionEventListenerType>(entry.key), entry.value);
}

void WebExtensionContext::saveBackgroundPageListenersToStorage()
{
    if (!storageIsPersistent() || protect(extension())->backgroundContentIsPersistent())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Saving %u background content event listeners to storage", m_backgroundContentEventListeners.size());

    ASSERT(m_state);
    Vector<int> listeners;
    listeners.reserveInitialCapacity(m_backgroundContentEventListeners.size());
    for (auto& entry : m_backgroundContentEventListeners)
        listeners.append(static_cast<int>(entry.key));
    std::ranges::sort(listeners);
    gsize savedListenersLength = 0;
    GUniquePtr<int> savedListenersData(g_key_file_get_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, &savedListenersLength, nullptr));
    Vector<int> savedListeners;
    if (savedListenersData)
        savedListeners = Vector<int>(unsafeMakeSpan<const int>(savedListenersData.get(), savedListenersLength));
    auto savedVersion = g_key_file_get_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, nullptr);

    if (listeners == savedListeners && savedVersion == currentBackgroundContentListenerStateVersion)
        return;

    g_key_file_set_integer_list(m_state.get(), groupNameStateKey, backgroundContentEventListenersKey, listeners.mutableSpan().data(), listeners.size());
    g_key_file_set_uint64(m_state.get(), groupNameStateKey, backgroundContentEventListenersVersionKey, currentBackgroundContentListenerStateVersion);

    writeStateToStorage();
}

void WebExtensionContext::performTasksAfterBackgroundContentLoads()
{
    if (!isLoaded())
        return;

    RELEASE_LOG_DEBUG(Extensions, "Background content loaded");

    if (m_shouldFireStartupEvent)
        m_shouldFireStartupEvent = false;

    if (m_installReason != InstallReason::None) {
        m_installReason = InstallReason::None;
        m_previousVersion = nullString();
    }

    RELEASE_LOG_DEBUG(Extensions, "Performing %zu task(s) after background content loaded", m_actionsToPerformAfterBackgroundContentLoads.size());

    for (auto& action : m_actionsToPerformAfterBackgroundContentLoads)
        action();

    m_backgroundContentIsLoaded = true;
    m_actionsToPerformAfterBackgroundContentLoads.clear();

    saveBackgroundPageListenersToStorage();
    scheduleBackgroundContentToUnload();
}

bool WebExtensionContext::decidePolicyForNavigationAction(WebKitWebView *webView, WebKitNavigationPolicyDecision *navigationPolicy)
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    ASSERT(webView == m_backgroundWebView || isInspectorBackgroundPage(webView));
#else
    ASSERT(webView == m_backgroundWebView);
#endif

    WebKitNavigationAction* action = webkit_navigation_policy_decision_get_navigation_action(navigationPolicy);
    RefPtr navigationAction = webkitNavigationActionGetAction(action);

    auto url = URL { String::fromUTF8(webkit_uri_request_get_uri(webkit_navigation_action_get_request(action))) };
    auto targetFrame = navigationAction->targetFrame();
    if ((targetFrame && !targetFrame->isMainFrame()) || isURLForThisExtension(url))
        return true;

    return false;
}

void WebExtensionContext::didFinishDocumentLoad(WebKitWebView *webView)
{
    if (webView != m_backgroundWebView)
        return;

    // The service worker will notify the load via a completion handler instead.
    if (protect(extension())->backgroundContentIsServiceWorker())
        return;

    performTasksAfterBackgroundContentLoads();
}

void WebExtensionContext::didFailNavigation(WebKitWebView *webView, RefPtr<API::Error> error)
{
    if (webView != m_backgroundWebView)
        return;

    m_backgroundContentLoadError = createError(Error::BackgroundContentFailedToLoad, nullString(), error);
    recordErrorIfNeeded(backgroundContentLoadError());

    unloadBackgroundWebView();
}

void WebExtensionContext::webViewWebContentProcessDidTerminate(WebKitWebView *webView)
{
    if (webView == m_backgroundWebView) {
        unloadBackgroundWebView();

        if (protect(extension())->backgroundContentIsPersistent())
            loadBackgroundWebView();

        return;
    }

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (isInspectorBackgroundPage(webView)) {
        GRefPtr<WebKitURIRequest> uriRequest(adoptGRef(webkit_uri_request_new(inspectorBackgroundPageURL().utf8().legacyCStringPointer())));
        webkit_web_view_load_request(webView, uriRequest.get());
        return;
    }
#endif

    ASSERT_NOT_REACHED();
}

bool WebExtensionContext::isNotRunningInTestRunner()
{
#if PLATFORM(WPE)
    return WTF::applicationID() != "org.webkit.app-TestWebKitWPE"_s;
#else
    return WTF::applicationID() != "org.webkit.app-TestWebKitGTK"_s;
#endif
}

std::optional<WebCore::PageIdentifier> WebExtensionContext::backgroundPageIdentifier(WebProcessProxy& destinationProcess) const
{
    if (!m_backgroundWebView || protect(extension())->backgroundContentIsServiceWorker())
        return std::nullopt;

    Ref backgroundPage = webkitWebViewGetPage(m_backgroundWebView.get());
    return backgroundPage->webPageIDInProcess(destinationProcess);
}

Vector<WebExtensionContext::PageIdentifierTuple> WebExtensionContext::popupPageIdentifiers(WebProcessProxy& destinationProcess) const
{
    return { };
}

Vector<WebExtensionContext::PageIdentifierTuple> WebExtensionContext::tabPageIdentifiers(WebProcessProxy& destinationProcess) const
{
    return { };
}

} // namespace WebKit

#endif // ENABLE(2022_GLIB_API)

#endif // ENABLE(WK_WEB_EXTENSIONS)
