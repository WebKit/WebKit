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
#include <glib.h>
#include <wtf/glib/Application.h>

#if ENABLE(WK_WEB_EXTENSIONS)

#if ENABLE(2022_GLIB_API)

static constexpr auto groupNameStateKey = "ExtensionState"_s;
static constexpr auto backgroundContentEventListenersKey = "BackgroundContentEventListeners"_s;
static constexpr auto backgroundContentEventListenersVersionKey = "BackgroundContentEventListenersVersion"_s;
static constexpr auto lastSeenBaseURLStateKey = "LastSeenBaseURL"_s;
static constexpr auto lastSeenDisplayNameStateKey = "LastSeenDisplayName"_s;

// Update this value when any changes are made to the WebExtensionEventListenerType enum.
static constexpr auto currentBackgroundContentListenerStateVersion = 4;

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
    RELEASE_LOG_ERROR(Extensions, "Error recorded: %s", error->localizedDescription().utf8().data());

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

GRefPtr<GKeyFile> WebExtensionContext::currentState() const
{
    return m_state;
}

GRefPtr<GKeyFile> WebExtensionContext::readStateFromPath(const String& stateFilePath)
{
    GRefPtr<GKeyFile> stateFile(adoptGRef(g_key_file_new()));
    GUniqueOutPtr<GError> error;

    g_key_file_load_from_file(stateFile.get(), stateFilePath.utf8().data(), G_KEY_FILE_NONE, &error.outPtr());
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

    if (!g_key_file_save_to_file(currentState().get(), stateFilePath().utf8().data(), &error.outPtr()))
        RELEASE_LOG_ERROR(Extensions, "Unable to save extension state: %" PUBLIC_LOG_STRING, error->message);
}

void WebExtensionContext::enumerateExtensionPages(NOESCAPE Function<void(WebPageProxy&, bool&)>&& action)
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
    GRefPtr<WebKitWebView> webView = adoptGRef(WEBKIT_WEB_VIEW(g_object_new(WEBKIT_TYPE_WEB_VIEW,
        "web-extension-mode", isManifestVersion3 ? WEBKIT_WEB_EXTENSION_MODE_MANIFESTV3 : WEBKIT_WEB_EXTENSION_MODE_MANIFESTV2,
        "related-view", preferences->siteIsolationEnabled() ? nullptr : relatedWebView(),
        "settings", settings.get(),
        "web-extension-context", m_delegate.get(),
        nullptr)));
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
        GRefPtr<WebKitURIRequest> uriRequest(adoptGRef(webkit_uri_request_new(backgroundContentURL().string().utf8().data())));
        webkit_web_view_load_request(m_backgroundWebView.get(), uriRequest.get());
        return;
    }

    webkitWebViewLoadServiceWorker(m_backgroundWebView.get(), backgroundContentURL().string().utf8().data(), protect(extension())->backgroundContentUsesModules(), [this, protectedThis = Ref { *this }](bool success) {
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
    m_backgroundWebView = nullptr;
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
        GRefPtr<WebKitURIRequest> uriRequest(adoptGRef(webkit_uri_request_new(inspectorBackgroundPageURL().utf8().data())));
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

} // namespace WebKit

#endif // ENABLE(2022_GLIB_API)

#endif // ENABLE(WK_WEB_EXTENSIONS)
