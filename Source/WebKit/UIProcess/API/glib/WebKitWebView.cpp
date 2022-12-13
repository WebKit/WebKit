/*
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2014 Collabora Ltd.
 * Copyright (C) 2011, 2017, 2020 Igalia S.L.
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
#include "WebKitWebView.h"

#include "APIContentWorld.h"
#include "APIData.h"
#include "APINavigation.h"
#include "APISerializedScriptValue.h"
#include "DataReference.h"
#include "ImageOptions.h"
#include "NotificationService.h"
#include "ProvisionalPageProxy.h"
#include "WebContextMenuItem.h"
#include "WebContextMenuItemData.h"
#include "WebKitAuthenticationRequestPrivate.h"
#include "WebKitBackForwardListPrivate.h"
#include "WebKitContextMenuClient.h"
#include "WebKitContextMenuItemPrivate.h"
#include "WebKitContextMenuPrivate.h"
#include "WebKitDownloadPrivate.h"
#include "WebKitEditingCommands.h"
#include "WebKitEditorStatePrivate.h"
#include "WebKitEnumTypes.h"
#include "WebKitError.h"
#include "WebKitFaviconDatabasePrivate.h"
#include "WebKitFormClient.h"
#include "WebKitHitTestResultPrivate.h"
#include "WebKitIconLoadingClient.h"
#include "WebKitInputMethodContextPrivate.h"
#include "WebKitInstallMissingMediaPluginsPermissionRequestPrivate.h"
#include "WebKitJavascriptResultPrivate.h"
#include "WebKitNavigationClient.h"
#include "WebKitNotificationPrivate.h"
#include "WebKitPermissionStateQueryPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitResponsePolicyDecision.h"
#include "WebKitScriptDialogPrivate.h"
#include "WebKitSettingsPrivate.h"
#include "WebKitUIClient.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitUserMessagePrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebResourceLoadManager.h"
#include "WebKitWebResourcePrivate.h"
#include "WebKitWebViewInternal.h"
#include "WebKitWebViewPrivate.h"
#include "WebKitWebViewSessionStatePrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include "WebKitWebsitePoliciesPrivate.h"
#include "WebKitWindowPropertiesPrivate.h"
#include "WebPageMessages.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/JSDOMExceptionHandling.h>
#include <WebCore/SharedBuffer.h>
#include <WebCore/URLSoup.h>
#include <glib/gi18n-lib.h>
#include <jsc/JSCContextPrivate.h>
#include <libsoup/soup.h>
#include <wtf/SetForScope.h>
#include <wtf/URL.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(GTK)
#include "WebKitInputMethodContextImplGtk.h"
#include "WebKitPointerLockPermissionRequest.h"
#include "WebKitPrintOperationPrivate.h"
#include "WebKitWebInspectorPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include <WebCore/GUniquePtrGtk.h>
#include <WebCore/RefPtrCairo.h>
#endif

#if PLATFORM(WPE)
#include "WPEView.h"
#include "WebKitOptionMenuPrivate.h"
#include "WebKitWebViewBackendPrivate.h"
#include "WebKitWebViewClient.h"
#endif

using namespace WebKit;
using namespace WebCore;

/**
 * WebKitWebView:
 *
 * The central class of the WPE WebKit and WebKitGTK APIs.
 *
 * #WebKitWebView is the central class of the WPE WebKit and WebKitGTK
 * APIs. It is responsible for managing the drawing of the content and
 * forwarding of events. You can load any URI into the #WebKitWebView or
 * a data string. With #WebKitSettings you can control various aspects
 * of the rendering and loading of the content.
 *
 * Note that in WebKitGTK, #WebKitWebView is scrollable by itself, so
 * you don't need to embed it in a #GtkScrolledWindow.
 */

enum {
    LOAD_CHANGED,
    LOAD_FAILED,
    LOAD_FAILED_WITH_TLS_ERRORS,

    CREATE,
    READY_TO_SHOW,
    RUN_AS_MODAL,
    CLOSE,

    SCRIPT_DIALOG,

    DECIDE_POLICY,
    PERMISSION_REQUEST,

    MOUSE_TARGET_CHANGED,

#if PLATFORM(GTK)
    PRINT,
#endif

    RESOURCE_LOAD_STARTED,

    ENTER_FULLSCREEN,
    LEAVE_FULLSCREEN,

    RUN_FILE_CHOOSER,

    CONTEXT_MENU,
    CONTEXT_MENU_DISMISSED,

    SUBMIT_FORM,

    INSECURE_CONTENT_DETECTED,

#if PLATFORM(GTK)
    WEB_PROCESS_CRASHED,
#endif
    WEB_PROCESS_TERMINATED,

    AUTHENTICATE,

    SHOW_NOTIFICATION,

#if PLATFORM(GTK)
    RUN_COLOR_CHOOSER,
#endif
    SHOW_OPTION_MENU,

    USER_MESSAGE_RECEIVED,

    QUERY_PERMISSION_STATE,

    LAST_SIGNAL
};

enum {
    PROP_0,

#if PLATFORM(WPE)
    PROP_BACKEND,
#endif

    PROP_WEB_CONTEXT,
    PROP_RELATED_VIEW,
    PROP_SETTINGS,
    PROP_USER_CONTENT_MANAGER,
    PROP_TITLE,
    PROP_ESTIMATED_LOAD_PROGRESS,

#if PLATFORM(GTK)
    PROP_FAVICON,
#endif

    PROP_URI,
    PROP_ZOOM_LEVEL,
    PROP_IS_LOADING,
    PROP_IS_PLAYING_AUDIO,
    PROP_IS_EPHEMERAL,
    PROP_IS_CONTROLLED_BY_AUTOMATION,
    PROP_AUTOMATION_PRESENTATION_TYPE,
    PROP_EDITABLE,
    PROP_PAGE_ID,
    PROP_IS_MUTED,
    PROP_WEBSITE_POLICIES,
    PROP_IS_WEB_PROCESS_RESPONSIVE,

    PROP_CAMERA_CAPTURE_STATE,
    PROP_MICROPHONE_CAPTURE_STATE,
    PROP_DISPLAY_CAPTURE_STATE,

    PROP_WEB_EXTENSION_MODE,
    PROP_DEFAULT_CONTENT_SECURITY_POLICY,

    N_PROPERTIES,
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

class PageLoadStateObserver;

#if PLATFORM(WPE)
static unsigned frameDisplayCallbackID;
struct FrameDisplayedCallback {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    FrameDisplayedCallback(WebKitFrameDisplayedCallback callback, gpointer userData = nullptr, GDestroyNotify destroyNotifyFunction = nullptr)
        : id(++frameDisplayCallbackID)
        , callback(callback)
        , userData(userData)
        , destroyNotifyFunction(destroyNotifyFunction)
    {
    }

    ~FrameDisplayedCallback()
    {
        if (destroyNotifyFunction)
            destroyNotifyFunction(userData);
    }

    FrameDisplayedCallback(FrameDisplayedCallback&&) = default;
    FrameDisplayedCallback(const FrameDisplayedCallback&) = delete;
    FrameDisplayedCallback& operator=(const FrameDisplayedCallback&) = delete;

    unsigned id { 0 };
    WebKitFrameDisplayedCallback callback { nullptr };
    gpointer userData { nullptr };
    GDestroyNotify destroyNotifyFunction { nullptr };
};
#endif // PLATFORM(WPE)

struct _WebKitWebViewPrivate {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    ~_WebKitWebViewPrivate()
    {
        // For modal dialogs, make sure the main loop is stopped when finalizing the webView.
        if (modalLoop && g_main_loop_is_running(modalLoop.get()))
            g_main_loop_quit(modalLoop.get());
    }

#if PLATFORM(WPE)
    GRefPtr<WebKitWebViewBackend> backend;
    std::unique_ptr<WKWPE::View> view;
    Vector<FrameDisplayedCallback> frameDisplayedCallbacks;
    bool inFrameDisplayed;
    HashSet<unsigned> frameDisplayedCallbacksToRemove;
#endif

    WebKitWebView* relatedView;
    CString title;
    CString customTextEncoding;
    CString activeURI;
    bool isActiveURIChangeBlocked;
    bool isLoading;
    bool isEphemeral;
    bool isControlledByAutomation;
    WebKitAutomationBrowsingContextPresentation automationPresentationType;

    std::unique_ptr<PageLoadStateObserver> loadObserver;

    GRefPtr<WebKitBackForwardList> backForwardList;
    GRefPtr<WebKitSettings> settings;
    GRefPtr<WebKitUserContentManager> userContentManager;
    GRefPtr<WebKitWebContext> context;
    GRefPtr<WebKitWindowProperties> windowProperties;
    GRefPtr<WebKitEditorState> editorState;

    GRefPtr<GMainLoop> modalLoop;

    GRefPtr<WebKitHitTestResult> mouseTargetHitTestResult;
    OptionSet<WebEventModifier> mouseTargetModifiers;

    GRefPtr<WebKitFindController> findController;

    GRefPtr<WebKitWebResource> mainResource;
    std::unique_ptr<WebKitWebResourceLoadManager> resourceLoadManager;

    WebKitScriptDialog* currentScriptDialog;

#if PLATFORM(GTK)
    GRefPtr<JSCContext> jsContext;

    GRefPtr<WebKitWebInspector> inspector;

    RefPtr<cairo_surface_t> favicon;
    GRefPtr<GCancellable> faviconCancellable;

    CString faviconURI;
    unsigned long faviconChangedHandlerID;
#endif

    GRefPtr<WebKitAuthenticationRequest> authenticationRequest;

    GRefPtr<WebKitWebsiteDataManager> websiteDataManager;
    GRefPtr<WebKitWebsitePolicies> websitePolicies;

    CString defaultContentSecurityPolicy;
    WebKitWebExtensionMode webExtensionMode;

    double textScaleFactor;

    bool isWebProcessResponsive;
};

static guint signals[LAST_SIGNAL] = { 0, };

#if PLATFORM(GTK)
WEBKIT_DEFINE_TYPE(WebKitWebView, webkit_web_view, WEBKIT_TYPE_WEB_VIEW_BASE)
#elif PLATFORM(WPE)
WEBKIT_DEFINE_TYPE(WebKitWebView, webkit_web_view, G_TYPE_OBJECT)
#endif

static inline WebPageProxy& getPage(WebKitWebView* webView)
{
#if PLATFORM(GTK)
    auto* page = webkitWebViewBaseGetPage(reinterpret_cast<WebKitWebViewBase*>(webView));
    ASSERT(page);
    return *page;
#elif PLATFORM(WPE)
    ASSERT(webView->priv->view);
    return webView->priv->view->page();
#endif
}

static void webkitWebViewSetIsLoading(WebKitWebView* webView, bool isLoading)
{
    if (webView->priv->isLoading == isLoading)
        return;

    webView->priv->isLoading = isLoading;
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_IS_LOADING]);
}

void webkitWebViewIsPlayingAudioChanged(WebKitWebView* webView)
{
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_IS_PLAYING_AUDIO]);
}

void webkitWebViewMediaCaptureStateDidChange(WebKitWebView* webView, WebCore::MediaProducer::MediaStateFlags mediaStateFlags)
{
    if (mediaStateFlags.isEmpty()) {
        g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_CAMERA_CAPTURE_STATE]);
        g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_DISPLAY_CAPTURE_STATE]);
        g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_MICROPHONE_CAPTURE_STATE]);
        return;
    }
    if (mediaStateFlags.containsAny(WebCore::MediaProducer::MicrophoneCaptureMask))
        g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_MICROPHONE_CAPTURE_STATE]);
    if (mediaStateFlags.containsAny(WebCore::MediaProducer::VideoCaptureMask))
        g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_CAMERA_CAPTURE_STATE]);
    if (mediaStateFlags.containsAny(WebCore::MediaProducer::DisplayCaptureMask))
        g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_DISPLAY_CAPTURE_STATE]);
}

class PageLoadStateObserver final : public PageLoadState::Observer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageLoadStateObserver(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    void willChangeIsLoading() override
    {
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    void didChangeIsLoading() override
    {
        webkitWebViewSetIsLoading(m_webView, getPage(m_webView).pageLoadState().isLoading());
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    void willChangeTitle() override
    {
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    void didChangeTitle() override
    {
        m_webView->priv->title = getPage(m_webView).pageLoadState().title().utf8();
        g_object_notify_by_pspec(G_OBJECT(m_webView), sObjProperties[PROP_TITLE]);
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    void willChangeActiveURL() override
    {
        if (m_webView->priv->isActiveURIChangeBlocked)
            return;
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    void didChangeActiveURL() override
    {
        if (m_webView->priv->isActiveURIChangeBlocked)
            return;
        m_webView->priv->activeURI = getPage(m_webView).pageLoadState().activeURL().utf8();
        g_object_notify_by_pspec(G_OBJECT(m_webView), sObjProperties[PROP_URI]);
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    void willChangeHasOnlySecureContent() override { }
    void didChangeHasOnlySecureContent() override { }

    void willChangeEstimatedProgress() override
    {
        g_object_freeze_notify(G_OBJECT(m_webView));
    }
    void didChangeEstimatedProgress() override
    {
        g_object_notify_by_pspec(G_OBJECT(m_webView), sObjProperties[PROP_ESTIMATED_LOAD_PROGRESS]);
        g_object_thaw_notify(G_OBJECT(m_webView));
    }

    void willChangeCanGoBack() override { }
    void didChangeCanGoBack() override { }
    void willChangeCanGoForward() override { }
    void didChangeCanGoForward() override { }
    void willChangeNetworkRequestsInProgress() override { }
    void didChangeNetworkRequestsInProgress() override { }
    void willChangeCertificateInfo() override { }
    void didChangeCertificateInfo() override { }
    void willChangeWebProcessIsResponsive() override { }
    void didChangeWebProcessIsResponsive() override { }
    void didSwapWebProcesses() override { };

    WebKitWebView* m_webView;
};

#if PLATFORM(WPE)
WebKitWebViewClient::WebKitWebViewClient(WebKitWebView* webView)
    : m_webView(webView)
{ }

GRefPtr<WebKitOptionMenu> WebKitWebViewClient::showOptionMenu(WebKitPopupMenu& popupMenu, const WebCore::IntRect& rect, const Vector<WebPopupItem>& items, int32_t selectedIndex)
{
    GRefPtr<WebKitOptionMenu> menu = adoptGRef(webkitOptionMenuCreate(popupMenu, items, selectedIndex));
    if (webkitWebViewShowOptionMenu(WEBKIT_WEB_VIEW(m_webView), rect, menu.get()))
        return menu;
    return nullptr;
}

void WebKitWebViewClient::handleDownloadRequest(WKWPE::View&, DownloadProxy& downloadProxy)
{
    webkitWebViewHandleDownloadRequest(m_webView, &downloadProxy);
}

void WebKitWebViewClient::frameDisplayed(WKWPE::View&)
{
    {
        SetForScope inFrameDisplayedGuard(m_webView->priv->inFrameDisplayed, true);
        for (const auto& callback : m_webView->priv->frameDisplayedCallbacks) {
            if (!m_webView->priv->frameDisplayedCallbacksToRemove.contains(callback.id))
                callback.callback(m_webView, callback.userData);
        }
    }

    while (!m_webView->priv->frameDisplayedCallbacksToRemove.isEmpty()) {
        auto id = m_webView->priv->frameDisplayedCallbacksToRemove.takeAny();
        m_webView->priv->frameDisplayedCallbacks.removeFirstMatching([id](const auto& item) {
            return item.id == id;
        });
    }
}

void WebKitWebViewClient::willStartLoad(WKWPE::View&)
{
    webkitWebViewWillStartLoad(m_webView);
}

void WebKitWebViewClient::didChangePageID(WKWPE::View&)
{
    webkitWebViewDidChangePageID(m_webView);
}

void WebKitWebViewClient::didReceiveUserMessage(WKWPE::View&, UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    webkitWebViewDidReceiveUserMessage(m_webView, WTFMove(message), WTFMove(completionHandler));
}

WebKitWebResourceLoadManager* WebKitWebViewClient::webResourceLoadManager()
{
    return webkitWebViewGetWebResourceLoadManager(m_webView);
}
#endif

static gboolean webkitWebViewLoadFail(WebKitWebView* webView, WebKitLoadEvent, const char* failingURI, GError* error)
{
    if (g_error_matches(error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED)
        || g_error_matches(error, WEBKIT_PLUGIN_ERROR, WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD)
        || g_error_matches(error, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE))
        return FALSE;

    GUniquePtr<char> htmlString(g_strdup_printf("<html><body>%s</body></html>", error->message));
    webkit_web_view_load_alternate_html(webView, htmlString.get(), failingURI, 0);

    return TRUE;
}

#if PLATFORM(GTK)
static GtkWidget* webkitWebViewCreate(WebKitWebView*, WebKitNavigationAction*)
{
    return nullptr;
}
#else
static WebKitWebView* webkitWebViewCreate(WebKitWebView*, WebKitNavigationAction*)
{
    return nullptr;
}
#endif

static gboolean webkitWebViewDecidePolicy(WebKitWebView*, WebKitPolicyDecision* decision, WebKitPolicyDecisionType decisionType)
{
    if (decisionType != WEBKIT_POLICY_DECISION_TYPE_RESPONSE) {
        webkit_policy_decision_use(decision);
        return TRUE;
    }

    WebKitURIResponse* response = webkit_response_policy_decision_get_response(WEBKIT_RESPONSE_POLICY_DECISION(decision));
    const ResourceResponse& resourceResponse = webkitURIResponseGetResourceResponse(response);
    if (resourceResponse.isAttachment()) {
        webkit_policy_decision_download(decision);
        return TRUE;
    }

    if (webkit_response_policy_decision_is_mime_type_supported(WEBKIT_RESPONSE_POLICY_DECISION(decision)) || webkit_uri_response_get_status_code(response) == SOUP_STATUS_NO_CONTENT)
        webkit_policy_decision_use(decision);
    else
        webkit_policy_decision_ignore(decision);

    return TRUE;
}

static gboolean webkitWebViewPermissionRequest(WebKitWebView*, WebKitPermissionRequest* request)
{
#if ENABLE(POINTER_LOCK)
    if (WEBKIT_IS_POINTER_LOCK_PERMISSION_REQUEST(request)) {
        webkit_permission_request_allow(request);
        return TRUE;
    }
#endif

    webkit_permission_request_deny(request);
    return TRUE;
}

static void allowModalDialogsChanged(WebKitSettings* settings, GParamSpec*, WebKitWebView* webView)
{
    getPage(webView).setCanRunModal(webkit_settings_get_allow_modal_dialogs(settings));
}

static void zoomTextOnlyChanged(WebKitSettings* settings, GParamSpec*, WebKitWebView* webView)
{
    auto& page = getPage(webView);
    gboolean zoomTextOnly = webkit_settings_get_zoom_text_only(settings);
    gdouble pageZoomLevel = zoomTextOnly ? 1 : page.textZoomFactor();
    gdouble textZoomLevel = zoomTextOnly ? page.pageZoomFactor() : 1;
    page.setPageAndTextZoomFactors(pageZoomLevel, textZoomLevel);
}

static void userAgentChanged(WebKitSettings* settings, GParamSpec*, WebKitWebView* webView)
{
    getPage(webView).setCustomUserAgent(String::fromUTF8(webkit_settings_get_user_agent(settings)));
}

#if PLATFORM(GTK)
static void enableBackForwardNavigationGesturesChanged(WebKitSettings* settings, GParamSpec*, WebKitWebView* webView)
{
    gboolean enable = webkit_settings_get_enable_back_forward_navigation_gestures(settings);
    webkitWebViewBaseSetEnableBackForwardNavigationGesture(WEBKIT_WEB_VIEW_BASE(webView), enable);
}

static void webkitWebViewUpdateFavicon(WebKitWebView* webView, cairo_surface_t* favicon)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->favicon.get() == favicon)
        return;

    priv->favicon = favicon;
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_FAVICON]);
}

static void webkitWebViewCancelFaviconRequest(WebKitWebView* webView)
{
    if (!webView->priv->faviconCancellable)
        return;

    g_cancellable_cancel(webView->priv->faviconCancellable.get());
    webView->priv->faviconCancellable = 0;
}

static void gotFaviconCallback(GObject* object, GAsyncResult* result, gpointer userData)
{
    GUniqueOutPtr<GError> error;
    RefPtr<cairo_surface_t> favicon = adoptRef(webkit_favicon_database_get_favicon_finish(WEBKIT_FAVICON_DATABASE(object), result, &error.outPtr()));
    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    WebKitWebView* webView = WEBKIT_WEB_VIEW(userData);
    webkitWebViewUpdateFavicon(webView, favicon.get());
    webView->priv->faviconCancellable = 0;
}

static void webkitWebViewRequestFavicon(WebKitWebView* webView)
{
    webkitWebViewCancelFaviconRequest(webView);

    WebKitWebViewPrivate* priv = webView->priv;
    priv->faviconCancellable = adoptGRef(g_cancellable_new());
    WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(priv->context.get());
    webkitFaviconDatabaseGetFaviconInternal(database, priv->activeURI.data(), priv->isEphemeral, priv->faviconCancellable.get(), gotFaviconCallback, webView);
}

static void webkitWebViewUpdateFaviconURI(WebKitWebView* webView, const char* faviconURI)
{
    if (webView->priv->faviconURI == faviconURI)
        return;

    webView->priv->faviconURI = faviconURI;
    webkitWebViewRequestFavicon(webView);
}

static void faviconChangedCallback(WebKitFaviconDatabase*, const char* pageURI, const char* faviconURI, WebKitWebView* webView)
{
    if (webView->priv->activeURI != pageURI)
        return;

    webkitWebViewUpdateFaviconURI(webView, faviconURI);
}
#endif

static bool webkitWebViewIsConstructed(WebKitWebView* webView)
{
    // The loadObserver is set in webkitWebViewConstructed, right after the
    // WebPageProxy is created, so we use it to check if the view has been
    // constructed instead of adding a boolean member only for that.
    return !!webView->priv->loadObserver;
}

static void webkitWebViewUpdateSettings(WebKitWebView* webView)
{
    // The "settings" property is set on construction, and in that
    // case webkit_web_view_set_settings() will be called *before* the
    // WebPageProxy has been created so we should do an early return.
    if (!webkitWebViewIsConstructed(webView))
        return;

    auto& page = getPage(webView);
    WebKitSettings* settings = webView->priv->settings.get();
    page.setPreferences(*webkitSettingsGetPreferences(settings));
    page.setCanRunModal(webkit_settings_get_allow_modal_dialogs(settings));
    page.setCustomUserAgent(String::fromUTF8(webkit_settings_get_user_agent(settings)));
#if PLATFORM(GTK)
    enableBackForwardNavigationGesturesChanged(settings, nullptr, webView);
#endif

    g_signal_connect(settings, "notify::allow-modal-dialogs", G_CALLBACK(allowModalDialogsChanged), webView);
    g_signal_connect(settings, "notify::zoom-text-only", G_CALLBACK(zoomTextOnlyChanged), webView);
    g_signal_connect(settings, "notify::user-agent", G_CALLBACK(userAgentChanged), webView);
#if PLATFORM(GTK)
    g_signal_connect(settings, "notify::enable-back-forward-navigation-gestures", G_CALLBACK(enableBackForwardNavigationGesturesChanged), webView);
#endif
}

static void webkitWebViewDisconnectSettingsSignalHandlers(WebKitWebView* webView)
{
    if (!webkitWebViewIsConstructed(webView))
        return;

    WebKitSettings* settings = webView->priv->settings.get();
    g_signal_handlers_disconnect_by_func(settings, reinterpret_cast<gpointer>(allowModalDialogsChanged), webView);
    g_signal_handlers_disconnect_by_func(settings, reinterpret_cast<gpointer>(zoomTextOnlyChanged), webView);
    g_signal_handlers_disconnect_by_func(settings, reinterpret_cast<gpointer>(userAgentChanged), webView);
#if PLATFORM(GTK)
    g_signal_handlers_disconnect_by_func(settings, reinterpret_cast<gpointer>(enableBackForwardNavigationGesturesChanged), webView);
#endif
}

#if PLATFORM(GTK)
static void webkitWebViewWatchForChangesInFavicon(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->faviconChangedHandlerID)
        return;

    WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(priv->context.get());
    priv->faviconChangedHandlerID = g_signal_connect(database, "favicon-changed", G_CALLBACK(faviconChangedCallback), webView);
}

static void webkitWebViewDisconnectFaviconDatabaseSignalHandlers(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->faviconChangedHandlerID)
        g_signal_handler_disconnect(webkit_web_context_get_favicon_database(priv->context.get()), priv->faviconChangedHandlerID);
    priv->faviconChangedHandlerID = 0;
}
#endif

static void webkitWebViewConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_view_parent_class)->constructed(object);

    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->relatedView) {
        priv->context = webkit_web_view_get_context(priv->relatedView);
        priv->isEphemeral = webkit_web_view_is_ephemeral(priv->relatedView);
        priv->isControlledByAutomation = webkit_web_view_is_controlled_by_automation(priv->relatedView);
    } else if (!priv->context)
        priv->context = webkit_web_context_get_default();
    else if (!priv->isEphemeral)
        priv->isEphemeral = webkit_web_context_is_ephemeral(priv->context.get());

    if (!priv->settings)
        priv->settings = adoptGRef(webkit_settings_new());

    if (!priv->userContentManager)
        priv->userContentManager = adoptGRef(webkit_user_content_manager_new());

    if (priv->isEphemeral && !webkit_web_context_is_ephemeral(priv->context.get())) {
        priv->websiteDataManager = adoptGRef(webkit_website_data_manager_new_ephemeral());
        auto* contextDataManager = webkit_web_context_get_website_data_manager(priv->context.get());
        webkit_website_data_manager_set_tls_errors_policy(priv->websiteDataManager.get(), webkit_website_data_manager_get_tls_errors_policy(contextDataManager));
        auto proxySettings = webkitWebsiteDataManagerGetDataStore(contextDataManager).networkProxySettings();
        webkitWebsiteDataManagerGetDataStore(priv->websiteDataManager.get()).setNetworkProxySettings(WTFMove(proxySettings));
    }

    if (!priv->websitePolicies)
        priv->websitePolicies = adoptGRef(webkit_website_policies_new());

    webkitWebContextCreatePageForWebView(priv->context.get(), webView, priv->userContentManager.get(), priv->relatedView, priv->websitePolicies.get());

    priv->loadObserver = makeUnique<PageLoadStateObserver>(webView);
    getPage(webView).pageLoadState().addObserver(*priv->loadObserver);

    priv->resourceLoadManager = makeUnique<WebKitWebResourceLoadManager>(webView);

    // The related view is only valid during the construction.
    priv->relatedView = nullptr;

    attachNavigationClientToView(webView);
    attachUIClientToView(webView);
    attachContextMenuClientToView(webView);
    attachFormClientToView(webView);

#if PLATFORM(GTK)
    attachIconLoadingClientToView(webView);

    GRefPtr<WebKitInputMethodContext> imContext = adoptGRef(webkitInputMethodContextImplGtkNew());
    webkitInputMethodContextSetWebView(imContext.get(), webView);
    webkitWebViewBaseSetInputMethodContext(WEBKIT_WEB_VIEW_BASE(webView), imContext.get());
#endif

#if PLATFORM(WPE)
    priv->view->setClient(makeUnique<WebKitWebViewClient>(webView));
#endif

    // This needs to be after attachUIClientToView() because WebPageProxy::setUIClient() calls setCanRunModal() with true.
    // See https://bugs.webkit.org/show_bug.cgi?id=135412.
    webkitWebViewUpdateSettings(webView);

    priv->backForwardList = adoptGRef(webkitBackForwardListCreate(&getPage(webView).backForwardList()));
    priv->windowProperties = adoptGRef(webkitWindowPropertiesCreate());

    priv->textScaleFactor = WebCore::screenDPI() / 96.;
    getPage(webView).setTextZoomFactor(priv->textScaleFactor);
    WebCore::setScreenDPIObserverHandler([webView] {
        auto& page = getPage(webView);
        auto zoomFactor = page.textZoomFactor() / webView->priv->textScaleFactor;
        webView->priv->textScaleFactor = WebCore::screenDPI() / 96.;
        page.setTextZoomFactor(zoomFactor * webView->priv->textScaleFactor);
    }, webView);

    priv->isWebProcessResponsive = true;
}

static void webkitWebViewSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
#if PLATFORM(WPE)
    case PROP_BACKEND: {
        gpointer backend = g_value_get_boxed(value);
        webView->priv->backend = backend ? adoptGRef(static_cast<WebKitWebViewBackend*>(backend)) : nullptr;
        break;
    }
#endif
    case PROP_WEB_CONTEXT: {
        gpointer webContext = g_value_get_object(value);
        webView->priv->context = webContext ? WEBKIT_WEB_CONTEXT(webContext) : nullptr;
        break;
    }
    case PROP_RELATED_VIEW: {
        gpointer relatedView = g_value_get_object(value);
        webView->priv->relatedView = relatedView ? WEBKIT_WEB_VIEW(relatedView) : nullptr;
        break;
    }
    case PROP_SETTINGS: {
        if (gpointer settings = g_value_get_object(value))
            webkit_web_view_set_settings(webView, WEBKIT_SETTINGS(settings));
        break;
    }
    case PROP_USER_CONTENT_MANAGER: {
        gpointer userContentManager = g_value_get_object(value);
        webView->priv->userContentManager = userContentManager ? WEBKIT_USER_CONTENT_MANAGER(userContentManager) : nullptr;
        break;
    }
    case PROP_ZOOM_LEVEL:
        webkit_web_view_set_zoom_level(webView, g_value_get_double(value));
        break;
    case PROP_IS_EPHEMERAL:
        webView->priv->isEphemeral = g_value_get_boolean(value);
        break;
    case PROP_IS_CONTROLLED_BY_AUTOMATION:
        webView->priv->isControlledByAutomation = g_value_get_boolean(value);
        break;
    case PROP_AUTOMATION_PRESENTATION_TYPE:
        webView->priv->automationPresentationType = static_cast<WebKitAutomationBrowsingContextPresentation>(g_value_get_enum(value));
        break;
    case PROP_EDITABLE:
        webkit_web_view_set_editable(webView, g_value_get_boolean(value));
        break;
    case PROP_IS_MUTED:
        webkit_web_view_set_is_muted(webView, g_value_get_boolean(value));
        break;
    case PROP_WEBSITE_POLICIES:
        webView->priv->websitePolicies = static_cast<WebKitWebsitePolicies*>(g_value_get_object(value));
        break;
    case PROP_CAMERA_CAPTURE_STATE:
        webkit_web_view_set_camera_capture_state(webView, static_cast<WebKitMediaCaptureState>(g_value_get_enum(value)));
        break;
    case PROP_MICROPHONE_CAPTURE_STATE:
        webkit_web_view_set_microphone_capture_state(webView, static_cast<WebKitMediaCaptureState>(g_value_get_enum(value)));
        break;
    case PROP_DISPLAY_CAPTURE_STATE:
        webkit_web_view_set_display_capture_state(webView, static_cast<WebKitMediaCaptureState>(g_value_get_enum(value)));
        break;
    case PROP_WEB_EXTENSION_MODE:
        webView->priv->webExtensionMode = static_cast<WebKitWebExtensionMode>(g_value_get_enum(value));
        break;
    case PROP_DEFAULT_CONTENT_SECURITY_POLICY:
        webView->priv->defaultContentSecurityPolicy = CString(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

    switch (propId) {
#if PLATFORM(WPE)
    case PROP_BACKEND:
        g_value_set_static_boxed(value, webView->priv->backend.get());
        break;
#endif
    case PROP_WEB_CONTEXT:
        g_value_set_object(value, webView->priv->context.get());
        break;
    case PROP_SETTINGS:
        g_value_set_object(value, webkit_web_view_get_settings(webView));
        break;
    case PROP_USER_CONTENT_MANAGER:
        g_value_set_object(value, webkit_web_view_get_user_content_manager(webView));
        break;
    case PROP_TITLE:
        g_value_set_string(value, webView->priv->title.data());
        break;
    case PROP_ESTIMATED_LOAD_PROGRESS:
        g_value_set_double(value, webkit_web_view_get_estimated_load_progress(webView));
        break;
#if PLATFORM(GTK)
    case PROP_FAVICON:
        g_value_set_pointer(value, webkit_web_view_get_favicon(webView));
        break;
#endif
    case PROP_URI:
        g_value_set_string(value, webkit_web_view_get_uri(webView));
        break;
    case PROP_ZOOM_LEVEL:
        g_value_set_double(value, webkit_web_view_get_zoom_level(webView));
        break;
    case PROP_IS_LOADING:
        g_value_set_boolean(value, webkit_web_view_is_loading(webView));
        break;
    case PROP_IS_PLAYING_AUDIO:
        g_value_set_boolean(value, webkit_web_view_is_playing_audio(webView));
        break;
    case PROP_IS_EPHEMERAL:
        g_value_set_boolean(value, webkit_web_view_is_ephemeral(webView));
        break;
    case PROP_IS_CONTROLLED_BY_AUTOMATION:
        g_value_set_boolean(value, webkit_web_view_is_controlled_by_automation(webView));
        break;
    case PROP_AUTOMATION_PRESENTATION_TYPE:
        g_value_set_enum(value, webkit_web_view_get_automation_presentation_type(webView));
        break;
    case PROP_EDITABLE:
        g_value_set_boolean(value, webkit_web_view_is_editable(webView));
        break;
    case PROP_PAGE_ID:
        g_value_set_uint64(value, webkit_web_view_get_page_id(webView));
        break;
    case PROP_IS_MUTED:
        g_value_set_boolean(value, webkit_web_view_get_is_muted(webView));
        break;
    case PROP_WEBSITE_POLICIES:
        g_value_set_object(value, webkit_web_view_get_website_policies(webView));
        break;
    case PROP_IS_WEB_PROCESS_RESPONSIVE:
        g_value_set_boolean(value, webkit_web_view_get_is_web_process_responsive(webView));
        break;
    case PROP_CAMERA_CAPTURE_STATE:
        g_value_set_enum(value, webkit_web_view_get_camera_capture_state(webView));
        break;
    case PROP_MICROPHONE_CAPTURE_STATE:
        g_value_set_enum(value, webkit_web_view_get_microphone_capture_state(webView));
        break;
    case PROP_DISPLAY_CAPTURE_STATE:
        g_value_set_enum(value, webkit_web_view_get_display_capture_state(webView));
        break;
    case PROP_WEB_EXTENSION_MODE:
        g_value_set_enum(value, webkit_web_view_get_web_extension_mode(webView));
        break;
    case PROP_DEFAULT_CONTENT_SECURITY_POLICY:
        g_value_set_string(value, webkit_web_view_get_default_content_security_policy(webView));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebViewDispose(GObject* object)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(object);

#if PLATFORM(GTK)
    webkitWebViewCancelFaviconRequest(webView);
    webkitWebViewDisconnectFaviconDatabaseSignalHandlers(webView);
#endif

    webkitWebViewDisconnectSettingsSignalHandlers(webView);

    if (webView->priv->loadObserver) {
        getPage(webView).pageLoadState().removeObserver(*webView->priv->loadObserver);
        webView->priv->loadObserver.reset();

        // We notify the context here to ensure it's called only once. Ideally we should
        // call this in finalize, not dispose, but finalize is used internally and we don't
        // have access to the instance pointer from the private struct destructor.
        webkitWebContextWebViewDestroyed(webView->priv->context.get(), webView);
    }

    if (webView->priv->currentScriptDialog) {
        webkit_script_dialog_close(webView->priv->currentScriptDialog);
        ASSERT(!webView->priv->currentScriptDialog);
    }

#if PLATFORM(WPE)
    webView->priv->view->close();
#endif

    WebCore::setScreenDPIObserverHandler(nullptr, webView);

    G_OBJECT_CLASS(webkit_web_view_parent_class)->dispose(object);
}

static gboolean webkitWebViewAccumulatorObjectHandled(GSignalInvocationHint*, GValue* returnValue, const GValue* handlerReturn, gpointer)
{
    void* object = g_value_get_object(handlerReturn);
    if (object)
        g_value_set_object(returnValue, object);

    return !object;
}

static void webkit_web_view_class_init(WebKitWebViewClass* webViewClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(webViewClass);

    gObjectClass->constructed = webkitWebViewConstructed;
    gObjectClass->set_property = webkitWebViewSetProperty;
    gObjectClass->get_property = webkitWebViewGetProperty;
    gObjectClass->dispose = webkitWebViewDispose;

    webViewClass->load_failed = webkitWebViewLoadFail;
    webViewClass->create = webkitWebViewCreate;
    webViewClass->script_dialog = webkitWebViewScriptDialog;
    webViewClass->decide_policy = webkitWebViewDecidePolicy;
    webViewClass->permission_request = webkitWebViewPermissionRequest;
    webViewClass->run_file_chooser = webkitWebViewRunFileChooser;
    webViewClass->authenticate = webkitWebViewAuthenticate;

#if PLATFORM(WPE)
    /**
     * WebKitWebView:backend:
     *
     * The #WebKitWebViewBackend of the view.
     *
     * since: 2.20
     */
    sObjProperties[PROP_BACKEND] =
        g_param_spec_boxed(
            "backend",
            _("Backend"),
            _("The backend for the web view"),
            WEBKIT_TYPE_WEB_VIEW_BACKEND,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
#endif

    /**
     * WebKitWebView:web-context:
     *
     * The #WebKitWebContext of the view.
     */
    sObjProperties[PROP_WEB_CONTEXT] =
        g_param_spec_object(
            "web-context",
            _("Web Context"),
            _("The web context for the view"),
            WEBKIT_TYPE_WEB_CONTEXT,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    /**
     * WebKitWebView:related-view:
     *
     * The related #WebKitWebView used when creating the view to share the
     * same web process. This property is not readable because the related
     * web view is only valid during the object construction.
     *
     * Since: 2.4
     */
    sObjProperties[PROP_RELATED_VIEW] =
        g_param_spec_object(
            "related-view",
            _("Related WebView"),
            _("The related WebKitWebView used when creating the view to share the same web process"),
            WEBKIT_TYPE_WEB_VIEW,
            static_cast<GParamFlags>(WEBKIT_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebView:settings:
     *
     * The #WebKitSettings of the view.
     *
     * Since: 2.6
     */
    sObjProperties[PROP_SETTINGS] =
        g_param_spec_object(
            "settings",
            _("WebView settings"),
            _("The WebKitSettings of the view"),
            WEBKIT_TYPE_SETTINGS,
            static_cast<GParamFlags>(WEBKIT_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

    /**
     * WebKitWebView:user-content-manager:
     *
     * The #WebKitUserContentManager of the view.
     *
     * Since: 2.6
     */
    sObjProperties[PROP_USER_CONTENT_MANAGER] =
        g_param_spec_object(
            "user-content-manager",
            _("WebView user content manager"),
            _("The WebKitUserContentManager of the view"),
            WEBKIT_TYPE_USER_CONTENT_MANAGER,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebView:title:
     *
     * The main frame document title of this #WebKitWebView. If
     * the title has not been received yet, it will be %NULL.
     */
    sObjProperties[PROP_TITLE] =
        g_param_spec_string(
            "title",
            _("Title"),
            _("Main frame document title"),
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebView:estimated-load-progress:
     *
     * An estimate of the percent completion for the current loading operation.
     * This value will range from 0.0 to 1.0 and, once a load completes,
     * will remain at 1.0 until a new load starts, at which point it
     * will be reset to 0.0.
     * The value is an estimate based on the total number of bytes expected
     * to be received for a document, including all its possible subresources
     * and child documents.
     */
    sObjProperties[PROP_ESTIMATED_LOAD_PROGRESS] =
        g_param_spec_double(
            "estimated-load-progress",
            _("Estimated Load Progress"),
            _("An estimate of the percent completion for a document load"),
            0.0, 1.0, 0.0,
            WEBKIT_PARAM_READABLE);

#if PLATFORM(GTK)
    /**
     * WebKitWebView:favicon:
     *
     * The favicon currently associated to the #WebKitWebView.
     * See webkit_web_view_get_favicon() for more details.
     */
    sObjProperties[PROP_FAVICON] =
        g_param_spec_pointer(
            "favicon",
            _("Favicon"),
            _("The favicon associated to the view, if any"),
            WEBKIT_PARAM_READABLE);
#endif

    /**
     * WebKitWebView:uri:
     *
     * The current active URI of the #WebKitWebView.
     * See webkit_web_view_get_uri() for more details.
     */
    sObjProperties[PROP_URI] =
        g_param_spec_string(
            "uri",
            _("URI"),
            _("The current active URI of the view"),
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebView:zoom-level:
     *
     * The zoom level of the #WebKitWebView content.
     * See webkit_web_view_set_zoom_level() for more details.
     */
    sObjProperties[PROP_ZOOM_LEVEL] =
        g_param_spec_double(
            "zoom-level",
            _("Zoom level"),
            _("The zoom level of the view content"),
            0, G_MAXDOUBLE, 1,
            WEBKIT_PARAM_READWRITE);

    /**
     * WebKitWebView:is-loading:
     *
     * Whether the #WebKitWebView is currently loading a page. This property becomes
     * %TRUE as soon as a new load operation is requested and before the
     * #WebKitWebView::load-changed signal is emitted with %WEBKIT_LOAD_STARTED and
     * at that point the active URI is the requested one.
     * When the load operation finishes the property is set to %FALSE before
     * #WebKitWebView::load-changed is emitted with %WEBKIT_LOAD_FINISHED.
     */
    sObjProperties[PROP_IS_LOADING] =
        g_param_spec_boolean(
            "is-loading",
            _("Is Loading"),
            _("Whether the view is loading a page"),
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebView:is-playing-audio:
     *
     * Whether the #WebKitWebView is currently playing audio from a page.
     * This property becomes %TRUE as soon as web content starts playing any
     * kind of audio. When a page is no longer playing any kind of sound,
     * the property is set back to %FALSE.
     *
     * Since: 2.8
     */
    sObjProperties[PROP_IS_PLAYING_AUDIO] =
        g_param_spec_boolean(
            "is-playing-audio",
            "Is Playing Audio",
            _("Whether the view is playing audio"),
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebView:is-ephemeral:
     *
     * Whether the #WebKitWebView is ephemeral. An ephemeral web view never writes
     * website data to the client storage, no matter what #WebKitWebsiteDataManager
     * its context is using. This is normally used to implement private browsing mode.
     * This is a %G_PARAM_CONSTRUCT_ONLY property, so you have to create an ephemeral
     * #WebKitWebView and it can't be changed. The ephemeral #WebKitWebsiteDataManager
     * created for the #WebKitWebView will inherit the network settings from the
     * #WebKitWebContext<!-- -->'s #WebKitWebsiteDataManager. To use different settings
     * you can get the #WebKitWebsiteDataManager with webkit_web_view_get_website_data_manager()
     * and set the new ones.
     * Note that all #WebKitWebView<!-- -->s created with an ephemeral #WebKitWebContext
     * will be ephemeral automatically.
     * See also webkit_web_context_new_ephemeral().
     *
     * Since: 2.16
     */
    sObjProperties[PROP_IS_EPHEMERAL] =
        g_param_spec_boolean(
            "is-ephemeral",
            "Is Ephemeral",
            _("Whether the web view is ephemeral"),
            FALSE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebView:is-controlled-by-automation:
     *
     * Whether the #WebKitWebView is controlled by automation. This should only be used when
     * creating a new #WebKitWebView as a response to #WebKitAutomationSession::create-web-view
     * signal request.
     *
     * Since: 2.18
     */
    sObjProperties[PROP_IS_CONTROLLED_BY_AUTOMATION] =
        g_param_spec_boolean(
            "is-controlled-by-automation",
            "Is Controlled By Automation",
            _("Whether the web view is controlled by automation"),
            FALSE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebView:automation-presentation-type:
     *
     * The #WebKitAutomationBrowsingContextPresentation of #WebKitWebView. This should only be used when
     * creating a new #WebKitWebView as a response to #WebKitAutomationSession::create-web-view
     * signal request. If the new WebView was added to a new tab of current browsing context window
     * %WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_TAB should be used.
     *
     * Since: 2.28
     */
    sObjProperties[PROP_AUTOMATION_PRESENTATION_TYPE] =
        g_param_spec_enum(
            "automation-presentation-type",
            "Automation Presentation Type",
            _("The browsing context presentation type for automation"),
            WEBKIT_TYPE_AUTOMATION_BROWSING_CONTEXT_PRESENTATION,
            WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_WINDOW,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebView:editable:
     *
     * Whether the pages loaded inside #WebKitWebView are editable. For more
     * information see webkit_web_view_set_editable().
     *
     * Since: 2.8
     */
    sObjProperties[PROP_EDITABLE] =
        g_param_spec_boolean(
            "editable",
            _("Editable"),
            _("Whether the content can be modified by the user."),
            FALSE,
            WEBKIT_PARAM_READWRITE);

    /**
     * WebKitWebView:page-id:
     *
     * The identifier of the #WebKitWebPage corresponding to the #WebKitWebView.
     *
     * Since: 2.28
     */
    sObjProperties[PROP_PAGE_ID] =
        g_param_spec_uint64(
            "page-id",
            _("Page Identifier"),
            _("The page identifier."),
            0, G_MAXUINT64, 0,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebView:is-muted:
     *
     * Whether the #WebKitWebView audio is muted. When %TRUE, audio is silenced.
     * It may still be playing, i.e. #WebKitWebView:is-playing-audio may be %TRUE.
     *
     * Since: 2.30
     */
    sObjProperties[PROP_IS_MUTED] =
        g_param_spec_boolean(
            "is-muted",
            "Is Muted",
            _("Whether the view audio is muted"),
            FALSE,
            WEBKIT_PARAM_READWRITE);

    /**
     * WebKitWebView:website-policies:
     *
     * The #WebKitWebsitePolicies for the view.
     *
     * Since: 2.30
     */
    sObjProperties[PROP_WEBSITE_POLICIES] =
        g_param_spec_object(
            "website-policies",
            _("Default Website Policies"),
            _("The default policy object for sites loaded in this view"),
            WEBKIT_TYPE_WEBSITE_POLICIES,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebView:is-web-process-responsive:
     *
     * Whether the web process currently associated to the #WebKitWebView is responsive.
     *
     * Since: 2.34
     */
    sObjProperties[PROP_IS_WEB_PROCESS_RESPONSIVE] =
        g_param_spec_boolean(
            "is-web-process-responsive",
            "Is Web Process Responsive",
            _("Whether the web process currently associated to the web view is responsive"),
            TRUE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebView:camera-capture-state:
     *
     * Capture state of the camera device. Whenever the user grants a media-request sent by the web
     * page, requesting video capture capabilities (`navigator.mediaDevices.getUserMedia({video:
     * true})`) this property will be set to %WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE.
     *
     * The application can monitor this property and provide a visual indicator allowing to optionally
     * deactivate or mute the capture device by setting this property respectively to
     * %WEBKIT_MEDIA_CAPTURE_STATE_NONE or %WEBKIT_MEDIA_CAPTURE_STATE_MUTED.
     *
     * If the capture state of the device is set to %WEBKIT_MEDIA_CAPTURE_STATE_NONE the web-page
     * can still re-request the permission to the user. Permission desision caching is left to the
     * application.
     *
     * Since: 2.34
     */
    sObjProperties[PROP_CAMERA_CAPTURE_STATE] = g_param_spec_enum(
        "camera-capture-state",
        "Camera Capture State",
        _("The capture state of the camera device"),
        WEBKIT_TYPE_MEDIA_CAPTURE_STATE,
        WEBKIT_MEDIA_CAPTURE_STATE_NONE,
        WEBKIT_PARAM_READWRITE);

    /**
     * WebKitWebView:microphone-capture-state:
     *
     * Capture state of the microphone device. Whenever the user grants a media-request sent by the web
     * page, requesting audio capture capabilities (`navigator.mediaDevices.getUserMedia({audio:
     * true})`) this property will be set to %WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE.
     *
     * The application can monitor this property and provide a visual indicator allowing to
     * optionally deactivate or mute the capture device by setting this property respectively to
     * %WEBKIT_MEDIA_CAPTURE_STATE_NONE or %WEBKIT_MEDIA_CAPTURE_STATE_MUTED.
     *
     * If the capture state of the device is set to %WEBKIT_MEDIA_CAPTURE_STATE_NONE the web-page
     * can still re-request the permission to the user. Permission desision caching is left to the
     * application.
     *
     * Since: 2.34
     */
    sObjProperties[PROP_MICROPHONE_CAPTURE_STATE] = g_param_spec_enum(
        "microphone-capture-state",
        "Microphone Capture State",
        _("The capture state of the microphone device"),
        WEBKIT_TYPE_MEDIA_CAPTURE_STATE,
        WEBKIT_MEDIA_CAPTURE_STATE_NONE,
        WEBKIT_PARAM_READWRITE);

    /**
     * WebKitWebView:display-capture-state:
     *
     * Capture state of the display device. Whenever the user grants a media-request sent by the web
     * page, requesting screencasting capabilities (`navigator.mediaDevices.getDisplayMedia() this
     * property will be set to %WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE.
     *
     * The application can monitor this property and provide a visual indicator allowing to
     * optionally deactivate or mute the capture device by setting this property respectively to
     * %WEBKIT_MEDIA_CAPTURE_STATE_NONE or %WEBKIT_MEDIA_CAPTURE_STATE_MUTED.
     *
     * If the capture state of the device is set to %WEBKIT_MEDIA_CAPTURE_STATE_NONE the web-page
     * can still re-request the permission to the user. Permission desision caching is left to the
     * application.
     *
     * Since: 2.34
     */
    sObjProperties[PROP_DISPLAY_CAPTURE_STATE] = g_param_spec_enum(
        "display-capture-state",
        "Display Capture State",
        _("The capture state of the display device"),
        WEBKIT_TYPE_MEDIA_CAPTURE_STATE,
        WEBKIT_MEDIA_CAPTURE_STATE_NONE,
        WEBKIT_PARAM_READWRITE);

    /**
     * WebKitWebView:web-extension-mode:
     *
     * This configures @web_view to treat the content as a WebExtension.
     *
     * Note that this refers to the web standard [WebExtensions](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions)
     * and not WebKitWebExtensions.
     *
     * In practice this limits the Content-Security-Policies that are allowed to be set. Some details can be found in
     * [Chrome's documentation](https://developer.chrome.com/docs/extensions/mv3/intro/mv3-migration/#content-security-policy).
     *
     * Since: 2.38
     */
    sObjProperties[PROP_WEB_EXTENSION_MODE] = g_param_spec_enum(
        "web-extension-mode",
        "WebExtension Mode",
        _("Enables WebExtension mode"),
        WEBKIT_TYPE_WEB_EXTENSION_MODE,
        WEBKIT_WEB_EXTENSION_MODE_NONE,
        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebView:default-content-security-policy:
     *
     * The default Content-Security-Policy used by the webview as if it were set
     * by an HTTP header.
     *
     * This applies to all content loaded including through navigation or via the various
     * webkit_web_view_load_\* APIs. However do note that many WebKit APIs bypass
     * Content-Security-Policy in general such as #WebKitUserContentManager and
     * webkit_web_view_run_javascript().
     *
     * Policies are additive so if a website sets its own policy it still applies
     * on top of the policy set here.
     *
     * Since: 2.38
     */
    sObjProperties[PROP_DEFAULT_CONTENT_SECURITY_POLICY] = g_param_spec_string(
        "default-content-security-policy",
        "Default Content-Security-Policy",
        _("The default Content-Security-Policy"),
        nullptr,
        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(gObjectClass, N_PROPERTIES, sObjProperties);

    /**
     * WebKitWebView::load-changed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @load_event: the #WebKitLoadEvent
     *
     * Emitted when a load operation in @web_view changes.
     * The signal is always emitted with %WEBKIT_LOAD_STARTED when a
     * new load request is made and %WEBKIT_LOAD_FINISHED when the load
     * finishes successfully or due to an error. When the ongoing load
     * operation fails #WebKitWebView::load-failed signal is emitted
     * before #WebKitWebView::load-changed is emitted with
     * %WEBKIT_LOAD_FINISHED.
     * If a redirection is received from the server, this signal is emitted
     * with %WEBKIT_LOAD_REDIRECTED after the initial emission with
     * %WEBKIT_LOAD_STARTED and before %WEBKIT_LOAD_COMMITTED.
     * When the page content starts arriving the signal is emitted with
     * %WEBKIT_LOAD_COMMITTED event.
     *
     * You can handle this signal and use a switch to track any ongoing
     * load operation.
     *
     * ```c
     * static void web_view_load_changed (WebKitWebView  *web_view,
     *                                    WebKitLoadEvent load_event,
     *                                    gpointer        user_data)
     * {
     *     switch (load_event) {
     *     case WEBKIT_LOAD_STARTED:
     *         // New load, we have now a provisional URI
     *         provisional_uri = webkit_web_view_get_uri (web_view);
     *         // Here we could start a spinner or update the
     *         // location bar with the provisional URI
     *         break;
     *     case WEBKIT_LOAD_REDIRECTED:
     *         redirected_uri = webkit_web_view_get_uri (web_view);
     *         break;
     *     case WEBKIT_LOAD_COMMITTED:
     *         // The load is being performed. Current URI is
     *         // the final one and it won't change unless a new
     *         // load is requested or a navigation within the
     *         // same page is performed
     *         uri = webkit_web_view_get_uri (web_view);
     *         break;
     *     case WEBKIT_LOAD_FINISHED:
     *         // Load finished, we can now stop the spinner
     *         break;
     *     }
     * }
     * ```
     */
    signals[LOAD_CHANGED] =
        g_signal_new("load-changed",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, load_changed),
                     0, 0,
                     g_cclosure_marshal_VOID__ENUM,
                     G_TYPE_NONE, 1,
                     WEBKIT_TYPE_LOAD_EVENT);

    /**
     * WebKitWebView::load-failed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @load_event: the #WebKitLoadEvent of the load operation
     * @failing_uri: the URI that failed to load
     * @error: the #GError that was triggered
     *
     * Emitted when an error occurs during a load operation.
     * If the error happened when starting to load data for a page
     * @load_event will be %WEBKIT_LOAD_STARTED. If it happened while
     * loading a committed data source @load_event will be %WEBKIT_LOAD_COMMITTED.
     * Since a load error causes the load operation to finish, the signal
     * WebKitWebView::load-changed will always be emitted with
     * %WEBKIT_LOAD_FINISHED event right after this one.
     *
     * By default, if the signal is not handled, a stock error page will be displayed.
     * You need to handle the signal if you want to provide your own error page.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[LOAD_FAILED] =
        g_signal_new(
            "load-failed",
            G_TYPE_FROM_CLASS(webViewClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, load_failed),
            g_signal_accumulator_true_handled, 0,
            g_cclosure_marshal_generic,
            G_TYPE_BOOLEAN, 3,
            WEBKIT_TYPE_LOAD_EVENT,
            G_TYPE_STRING,
            G_TYPE_ERROR | G_SIGNAL_TYPE_STATIC_SCOPE);

    /**
     * WebKitWebView::load-failed-with-tls-errors:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @failing_uri: the URI that failed to load
     * @certificate: a #GTlsCertificate
     * @errors: a #GTlsCertificateFlags with the verification status of @certificate
     *
     * Emitted when a TLS error occurs during a load operation.
     * To allow an exception for this @certificate
     * and the host of @failing_uri use webkit_web_context_allow_tls_certificate_for_host().
     *
     * To handle this signal asynchronously you should call g_object_ref() on @certificate
     * and return %TRUE.
     *
     * If %FALSE is returned, #WebKitWebView::load-failed will be emitted. The load
     * will finish regardless of the returned value.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     * Since: 2.6
     */
    signals[LOAD_FAILED_WITH_TLS_ERRORS] =
        g_signal_new("load-failed-with-tls-errors",
            G_TYPE_FROM_CLASS(webViewClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, load_failed_with_tls_errors),
            g_signal_accumulator_true_handled, 0 /* accumulator data */,
            g_cclosure_marshal_generic,
            G_TYPE_BOOLEAN, 3,
            G_TYPE_STRING,
            G_TYPE_TLS_CERTIFICATE,
            G_TYPE_TLS_CERTIFICATE_FLAGS);

    /**
     * WebKitWebView::create:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @navigation_action: a #WebKitNavigationAction
     *
     * Emitted when the creation of a new #WebKitWebView is requested.
     * If this signal is handled the signal handler should return the
     * newly created #WebKitWebView.
     *
     * The #WebKitNavigationAction parameter contains information about the
     * navigation action that triggered this signal.
     *
     * The new #WebKitWebView must be related to @web_view, see
     * webkit_web_view_new_with_related_view() for more details.
     *
     * The new #WebKitWebView should not be displayed to the user
     * until the #WebKitWebView::ready-to-show signal is emitted.
     *
     * Returns: (transfer full): a newly allocated #WebKitWebView widget
     *    or %NULL to propagate the event further.
     */
    signals[CREATE] = g_signal_new(
        "create",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, create),
        webkitWebViewAccumulatorObjectHandled, 0,
        g_cclosure_marshal_generic,
#if PLATFORM(GTK)
        GTK_TYPE_WIDGET,
#else
        WEBKIT_TYPE_WEB_VIEW,
#endif
        1,
        WEBKIT_TYPE_NAVIGATION_ACTION | G_SIGNAL_TYPE_STATIC_SCOPE);

    /**
     * WebKitWebView::ready-to-show:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted after #WebKitWebView::create on the newly created #WebKitWebView
     * when it should be displayed to the user. When this signal is emitted
     * all the information about how the window should look, including
     * size, position, whether the location, status and scrollbars
     * should be displayed, is already set on the #WebKitWindowProperties
     * of @web_view. See also webkit_web_view_get_window_properties().
     */
    signals[READY_TO_SHOW] =
        g_signal_new("ready-to-show",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, ready_to_show),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

     /**
     * WebKitWebView::run-as-modal:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted after #WebKitWebView::ready-to-show on the newly
     * created #WebKitWebView when JavaScript code calls
     * <function>window.showModalDialog</function>. The purpose of
     * this signal is to allow the client application to prepare the
     * new view to behave as modal. Once the signal is emitted a new
     * main loop will be run to block user interaction in the parent
     * #WebKitWebView until the new dialog is closed.
     */
    signals[RUN_AS_MODAL] =
            g_signal_new("run-as-modal",
                         G_TYPE_FROM_CLASS(webViewClass),
                         G_SIGNAL_RUN_LAST,
                         G_STRUCT_OFFSET(WebKitWebViewClass, run_as_modal),
                         0, 0,
                         g_cclosure_marshal_VOID__VOID,
                         G_TYPE_NONE, 0);

    /**
     * WebKitWebView::close:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted when closing a #WebKitWebView is requested. This occurs when a
     * call is made from JavaScript's <function>window.close</function> function or
     * after trying to close the @web_view with webkit_web_view_try_close().
     * It is the owner's responsibility to handle this signal to hide or
     * destroy the #WebKitWebView, if necessary.
     */
    signals[CLOSE] =
        g_signal_new("close",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, close),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebView::script-dialog:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @dialog: the #WebKitScriptDialog to show
     *
     * Emitted when JavaScript code calls <function>window.alert</function>,
     * <function>window.confirm</function> or <function>window.prompt</function>,
     * or when <function>onbeforeunload</function> event is fired.
     * The @dialog parameter should be used to build the dialog.
     * If the signal is not handled a different dialog will be built and shown depending
     * on the dialog type:
     * <itemizedlist>
     * <listitem><para>
     *  %WEBKIT_SCRIPT_DIALOG_ALERT: message dialog with a single Close button.
     * </para></listitem>
     * <listitem><para>
     *  %WEBKIT_SCRIPT_DIALOG_CONFIRM: message dialog with OK and Cancel buttons.
     * </para></listitem>
     * <listitem><para>
     *  %WEBKIT_SCRIPT_DIALOG_PROMPT: message dialog with OK and Cancel buttons and
     *  a text entry with the default text.
     * </para></listitem>
     * <listitem><para>
     *  %WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM: message dialog with Stay and Leave buttons.
     * </para></listitem>
     * </itemizedlist>
     *
     * It is possible to handle the script dialog request asynchronously, by simply
     * caling webkit_script_dialog_ref() on the @dialog argument and calling
     * webkit_script_dialog_close() when done.
     * If the last reference is removed on a #WebKitScriptDialog and the dialog has not been
     * closed, webkit_script_dialog_close() will be called.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[SCRIPT_DIALOG] = g_signal_new(
        "script-dialog",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, script_dialog),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WEBKIT_TYPE_SCRIPT_DIALOG);

    /**
     * WebKitWebView::decide-policy:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @decision: the #WebKitPolicyDecision
     * @decision_type: a #WebKitPolicyDecisionType denoting the type of @decision
     *
     * This signal is emitted when WebKit is requesting the client to decide a policy
     * decision, such as whether to navigate to a page, open a new window or whether or
     * not to download a resource. The #WebKitNavigationPolicyDecision passed in the
     * @decision argument is a generic type, but should be casted to a more
     * specific type when making the decision. For example:
     *
     * ```c
     * static gboolean
     * decide_policy_cb (WebKitWebView *web_view,
     *                   WebKitPolicyDecision *decision,
     *                   WebKitPolicyDecisionType type)
     * {
     *     switch (type) {
     *     case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION: {
     *         WebKitNavigationPolicyDecision *navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
     *         // Make a policy decision here
     *         break;
     *     }
     *     case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION: {
     *         WebKitNavigationPolicyDecision *navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
     *         // Make a policy decision here
     *         break;
     *     }
     *     case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
     *         WebKitResponsePolicyDecision *response = WEBKIT_RESPONSE_POLICY_DECISION (decision);
     *         // Make a policy decision here
     *         break;
     *     default:
     *         // Making no decision results in webkit_policy_decision_use()
     *         return FALSE;
     *     }
     *     return TRUE;
     * }
     * ```
     *
     * It is possible to make policy decision asynchronously, by simply calling g_object_ref()
     * on the @decision argument and returning %TRUE to block the default signal handler.
     * If the last reference is removed on a #WebKitPolicyDecision and no decision has been
     * made explicitly, webkit_policy_decision_use() will be the default policy decision. The
     * default signal handler will simply call webkit_policy_decision_use(). Only the first
     * policy decision chosen for a given #WebKitPolicyDecision will have any affect.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     */
    signals[DECIDE_POLICY] = g_signal_new(
        "decide-policy",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, decide_policy),
        g_signal_accumulator_true_handled, nullptr /* accumulator data */,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 2, /* number of parameters */
        WEBKIT_TYPE_POLICY_DECISION,
        WEBKIT_TYPE_POLICY_DECISION_TYPE);

    /**
     * WebKitWebView::permission-request:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: the #WebKitPermissionRequest
     *
     * This signal is emitted when WebKit is requesting the client to
     * decide about a permission request, such as allowing the browser
     * to switch to fullscreen mode, sharing its location or similar
     * operations.
     *
     * A possible way to use this signal could be through a dialog
     * allowing the user decide what to do with the request:
     *
     * ```c
     * static gboolean permission_request_cb (WebKitWebView *web_view,
     *                                        WebKitPermissionRequest *request,
     *                                        GtkWindow *parent_window)
     * {
     *     GtkWidget *dialog = gtk_message_dialog_new (parent_window,
     *                                                 GTK_DIALOG_MODAL,
     *                                                 GTK_MESSAGE_QUESTION,
     *                                                 GTK_BUTTONS_YES_NO,
     *                                                 "Allow Permission Request?");
     *     gtk_widget_show (dialog);
     *     gint result = gtk_dialog_run (GTK_DIALOG (dialog));
     *
     *     switch (result) {
     *     case GTK_RESPONSE_YES:
     *         webkit_permission_request_allow (request);
     *         break;
     *     default:
     *         webkit_permission_request_deny (request);
     *         break;
     *     }
     *     gtk_widget_destroy (dialog);
     *
     *     return TRUE;
     * }
     * ```
     *
     * It is possible to handle permission requests asynchronously, by
     * simply calling g_object_ref() on the @request argument and
     * returning %TRUE to block the default signal handler.  If the
     * last reference is removed on a #WebKitPermissionRequest and the
     * request has not been handled, webkit_permission_request_deny()
     * will be the default action.
     *
     * If the signal is not handled, the @request will be completed automatically
     * by the specific #WebKitPermissionRequest that could allow or deny it. Check the
     * documentation of classes implementing #WebKitPermissionRequest interface to know
     * their default action.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     */
    signals[PERMISSION_REQUEST] = g_signal_new(
        "permission-request",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, permission_request),
        g_signal_accumulator_true_handled, nullptr /* accumulator data */,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1, /* number of parameters */
        WEBKIT_TYPE_PERMISSION_REQUEST);
    /**
     * WebKitWebView::mouse-target-changed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @hit_test_result: a #WebKitHitTestResult
     * @modifiers: a bitmask of #GdkModifierType
     *
     * This signal is emitted when the mouse cursor moves over an
     * element such as a link, image or a media element. To determine
     * what type of element the mouse cursor is over, a Hit Test is performed
     * on the current mouse coordinates and the result is passed in the
     * @hit_test_result argument. The @modifiers argument is a bitmask of
     * #GdkModifierType flags indicating the state of modifier keys.
     * The signal is emitted again when the mouse is moved out of the
     * current element with a new @hit_test_result.
     */
    signals[MOUSE_TARGET_CHANGED] = g_signal_new(
        "mouse-target-changed",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, mouse_target_changed),
        nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        WEBKIT_TYPE_HIT_TEST_RESULT,
        G_TYPE_UINT);

#if PLATFORM(GTK)
    /**
     * WebKitWebView::print:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @print_operation: the #WebKitPrintOperation that will handle the print request
     *
     * Emitted when printing is requested on @web_view, usually by a JavaScript call,
     * before the print dialog is shown. This signal can be used to set the initial
     * print settings and page setup of @print_operation to be used as default values in
     * the print dialog. You can call webkit_print_operation_set_print_settings() and
     * webkit_print_operation_set_page_setup() and then return %FALSE to propagate the
     * event so that the print dialog is shown.
     *
     * You can connect to this signal and return %TRUE to cancel the print operation
     * or implement your own print dialog.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     */
    signals[PRINT] = g_signal_new(
        "print",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, print),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WEBKIT_TYPE_PRINT_OPERATION);
#endif // PLATFORM(GTK)

    /**
     * WebKitWebView::resource-load-started:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @resource: a #WebKitWebResource
     * @request: a #WebKitURIRequest
     *
     * Emitted when a new resource is going to be loaded. The @request parameter
     * contains the #WebKitURIRequest that will be sent to the server.
     * You can monitor the load operation by connecting to the different signals
     * of @resource.
     */
    signals[RESOURCE_LOAD_STARTED] = g_signal_new(
        "resource-load-started",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, resource_load_started),
        nullptr, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        WEBKIT_TYPE_WEB_RESOURCE,
                     WEBKIT_TYPE_URI_REQUEST);

    /**
     * WebKitWebView::enter-fullscreen:
     * @web_view: the #WebKitWebView on which the signal is emitted.
     *
     * Emitted when JavaScript code calls
     * <function>element.webkitRequestFullScreen</function>. If the
     * signal is not handled the #WebKitWebView will proceed to full screen
     * its top level window. This signal can be used by client code to
     * request permission to the user prior doing the full screen
     * transition and eventually prepare the top-level window
     * (e.g. hide some widgets that would otherwise be part of the
     * full screen window).
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to continue emission of the event.
     */
    signals[ENTER_FULLSCREEN] = g_signal_new(
        "enter-fullscreen",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, enter_fullscreen),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);

    /**
     * WebKitWebView::leave-fullscreen:
     * @web_view: the #WebKitWebView on which the signal is emitted.
     *
     * Emitted when the #WebKitWebView is about to restore its top level
     * window out of its full screen state. This signal can be used by
     * client code to restore widgets hidden during the
     * #WebKitWebView::enter-fullscreen stage for instance.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to continue emission of the event.
     */
    signals[LEAVE_FULLSCREEN] = g_signal_new(
        "leave-fullscreen",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, leave_fullscreen),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);

     /**
     * WebKitWebView::run-file-chooser:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: a #WebKitFileChooserRequest
     *
     * This signal is emitted when the user interacts with a <input
     * type='file' /> HTML element, requesting from WebKit to show
     * a dialog to select one or more files to be uploaded. To let the
     * application know the details of the file chooser, as well as to
     * allow the client application to either cancel the request or
     * perform an actual selection of files, the signal will pass an
     * instance of the #WebKitFileChooserRequest in the @request
     * argument.
     *
     * The default signal handler will asynchronously run a regular
     * #GtkFileChooserDialog for the user to interact with.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     */
    signals[RUN_FILE_CHOOSER] = g_signal_new(
        "run-file-chooser",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, run_file_chooser),
        g_signal_accumulator_true_handled, nullptr /* accumulator data */,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1, /* number of parameters */
        WEBKIT_TYPE_FILE_CHOOSER_REQUEST);

    signals[CONTEXT_MENU] = createContextMenuSignal(webViewClass);

    /**
     * WebKitWebView::context-menu-dismissed:
     * @web_view: the #WebKitWebView on which the signal is emitted
     *
     * Emitted after #WebKitWebView::context-menu signal, if the context menu is shown,
     * to notify that the context menu is dismissed.
     */
    signals[CONTEXT_MENU_DISMISSED] =
        g_signal_new("context-menu-dismissed",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, context_menu_dismissed),
                     0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    /**
     * WebKitWebView::submit-form:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: a #WebKitFormSubmissionRequest
     *
     * This signal is emitted when a form is about to be submitted. The @request
     * argument passed contains information about the text fields of the form. This
     * is typically used to store login information that can be used later to
     * pre-fill the form.
     * The form will not be submitted until webkit_form_submission_request_submit() is called.
     *
     * It is possible to handle the form submission request asynchronously, by
     * simply calling g_object_ref() on the @request argument and calling
     * webkit_form_submission_request_submit() when done to continue with the form submission.
     * If the last reference is removed on a #WebKitFormSubmissionRequest and the
     * form has not been submitted, webkit_form_submission_request_submit() will be called.
     */
    signals[SUBMIT_FORM] =
        g_signal_new("submit-form",
                     G_TYPE_FROM_CLASS(webViewClass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(WebKitWebViewClass, submit_form),
                     0, 0,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE, 1,
                     WEBKIT_TYPE_FORM_SUBMISSION_REQUEST);

    /**
     * WebKitWebView::insecure-content-detected:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @event: the #WebKitInsecureContentEvent
     *
     * This signal is emitted when insecure content has been detected
     * in a page loaded through a secure connection. This typically
     * means that a external resource from an unstrusted source has
     * been run or displayed, resulting in a mix of HTTPS and
     * non-HTTPS content.
     *
     * You can check the @event parameter to know exactly which kind
     * of event has been detected (see #WebKitInsecureContentEvent).
     */
    signals[INSECURE_CONTENT_DETECTED] =
        g_signal_new("insecure-content-detected",
            G_TYPE_FROM_CLASS(webViewClass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET(WebKitWebViewClass, insecure_content_detected),
            0, 0,
            g_cclosure_marshal_VOID__ENUM,
            G_TYPE_NONE, 1,
            WEBKIT_TYPE_INSECURE_CONTENT_EVENT);

#if PLATFORM(GTK)
    /**
     * WebKitWebView::web-process-crashed:
     * @web_view: the #WebKitWebView
     *
     * This signal is emitted when the web process crashes.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to propagate the event further.
     *
     * Deprecated: 2.20: Use WebKitWebView::web-process-terminated instead.
     */
    signals[WEB_PROCESS_CRASHED] = g_signal_new(
        "web-process-crashed",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, web_process_crashed),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 0);
#endif

    /**
     * WebKitWebView::web-process-terminated:
     * @web_view: the #WebKitWebView
     * @reason: the a #WebKitWebProcessTerminationReason
     *
     * This signal is emitted when the web process terminates abnormally due
     * to @reason.
     *
     * Since: 2.20
     */
    signals[WEB_PROCESS_TERMINATED] = g_signal_new(
        "web-process-terminated",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, web_process_terminated),
        0, 0,
        g_cclosure_marshal_VOID__ENUM,
        G_TYPE_NONE, 1,
        WEBKIT_TYPE_WEB_PROCESS_TERMINATION_REASON);

    /**
     * WebKitWebView::authenticate:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @request: a #WebKitAuthenticationRequest
     *
     * This signal is emitted when the user is challenged with HTTP
     * authentication. To let the  application access or supply
     * the credentials as well as to allow the client application
     * to either cancel the request or perform the authentication,
     * the signal will pass an instance of the
     * #WebKitAuthenticationRequest in the @request argument.
     * To handle this signal asynchronously you should keep a ref
     * of the request and return %TRUE. To disable HTTP authentication
     * entirely, connect to this signal and simply return %TRUE.
     *
     * The default signal handler will run a default authentication
     * dialog asynchronously for the user to interact with.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *   %FALSE to propagate the event further.
     *
     * Since: 2.2
     */
    signals[AUTHENTICATE] = g_signal_new(
        "authenticate",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, authenticate),
        g_signal_accumulator_true_handled, nullptr /* accumulator data */,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1, /* number of parameters */
        WEBKIT_TYPE_AUTHENTICATION_REQUEST);

    /**
     * WebKitWebView::show-notification:
     * @web_view: the #WebKitWebView
     * @notification: a #WebKitNotification
     *
     * This signal is emitted when a notification should be presented to the
     * user. The @notification is kept alive until either: 1) the web page cancels it
     * or 2) a navigation happens.
     *
     * The default handler will emit a notification using libnotify, if built with
     * support for it.
     *
     * Returns: %TRUE to stop other handlers from being invoked. %FALSE otherwise.
     *
     * Since: 2.8
     */
    signals[SHOW_NOTIFICATION] = g_signal_new(
        "show-notification",
        G_TYPE_FROM_CLASS(gObjectClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, show_notification),
        g_signal_accumulator_true_handled, nullptr /* accumulator data */,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WEBKIT_TYPE_NOTIFICATION);

#if PLATFORM(GTK)
     /**
      * WebKitWebView::run-color-chooser:
      * @web_view: the #WebKitWebView on which the signal is emitted
      * @request: a #WebKitColorChooserRequest
      *
      * This signal is emitted when the user interacts with a <input
      * type='color' /> HTML element, requesting from WebKit to show
      * a dialog to select a color. To let the application know the details of
      * the color chooser, as well as to allow the client application to either
      * cancel the request or perform an actual color selection, the signal will
      * pass an instance of the #WebKitColorChooserRequest in the @request
      * argument.
      *
      * It is possible to handle this request asynchronously by increasing the
      * reference count of the request.
      *
      * The default signal handler will asynchronously run a regular
      * #GtkColorChooser for the user to interact with.
      *
      * Returns: %TRUE to stop other handlers from being invoked for the event.
      *   %FALSE to propagate the event further.
      *
      * Since: 2.8
      */
    signals[RUN_COLOR_CHOOSER] = g_signal_new(
        "run-color-chooser",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, run_color_chooser),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WEBKIT_TYPE_COLOR_CHOOSER_REQUEST);
#endif // PLATFORM(GTK)

    // This signal is different for WPE and GTK, so it's declared in
    // WebKitWebView[Gtk,WPE].cpp to ensure we don't break the introspection.
    signals[SHOW_OPTION_MENU] = createShowOptionMenuSignal(webViewClass);

    /**
     * WebKitWebView::user-message-received:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @message: the #WebKitUserMessage received
     *
     * This signal is emitted when a #WebKitUserMessage is received from the
     * #WebKitWebPage corresponding to @web_view. You can reply to the message
     * using webkit_user_message_send_reply().
     *
     * You can handle the user message asynchronously by calling g_object_ref() on
     * @message and returning %TRUE. If the last reference of @message is removed
     * and the message has not been replied to, the operation in the #WebKitWebPage will
     * finish with error %WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE.
     *
     * Returns: %TRUE if the message was handled, or %FALSE otherwise.
     *
     * Since: 2.28
     */
    signals[USER_MESSAGE_RECEIVED] = g_signal_new(
        "user-message-received",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, user_message_received),
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WEBKIT_TYPE_USER_MESSAGE);

    /**
     * WebKitWebView::query-permission-state:
     * @web_view: the #WebKitWebView on which the signal is emitted
     * @query: the #WebKitPermissionStateQuery
     *
     * This signal allows the User-Agent to respond to permission requests for powerful features, as
     * specified by the [Permissions W3C Specification](https://w3c.github.io/permissions/).
     * You can reply to the query using webkit_permission_state_query_finish().
     *
     * You can handle the query asynchronously by calling webkit_permission_state_query_ref() on
     * @query and returning %TRUE. If the last reference of @query is removed and the query has not
     * been handled, the query result will be set to %WEBKIT_QUERY_PERMISSION_PROMPT.
     *
     * Returns: %TRUE if the message was handled, or %FALSE otherwise.
     *
     * Since: 2.40
     */
    signals[QUERY_PERMISSION_STATE] = g_signal_new(
        "query-permission-state",
        G_TYPE_FROM_CLASS(webViewClass),
        G_SIGNAL_RUN_LAST,
        G_STRUCT_OFFSET(WebKitWebViewClass, query_permission_state),
        g_signal_accumulator_true_handled, nullptr /* accumulator data */,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1, /* number of parameters */
        WEBKIT_TYPE_PERMISSION_STATE_QUERY);
}

static void webkitWebViewCompleteAuthenticationRequest(WebKitWebView* webView)
{
    WebKitWebViewPrivate* priv = webView->priv;
    if (!priv->authenticationRequest)
        return;

    if (priv->mainResource) {
        if (auto* response = webkit_web_resource_get_response(priv->mainResource.get())) {
            auto statusCode = webkit_uri_response_get_status_code(response);
            if (statusCode != SOUP_STATUS_UNAUTHORIZED && statusCode != SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED && statusCode < 500) {
                webkitAuthenticationRequestDidAuthenticate(priv->authenticationRequest.get());
                priv->authenticationRequest = nullptr;
                return;
            }
        }
    }

    webkit_authentication_request_cancel(priv->authenticationRequest.get());
    priv->authenticationRequest = nullptr;
}

void webkitWebViewCreatePage(WebKitWebView* webView, Ref<API::PageConfiguration>&& configuration)
{
#if PLATFORM(GTK)
    webkitWebViewBaseCreateWebPage(WEBKIT_WEB_VIEW_BASE(webView), WTFMove(configuration));
#elif PLATFORM(WPE)
    webView->priv->view.reset(WKWPE::View::create(webkit_web_view_backend_get_wpe_backend(webView->priv->backend.get()), configuration.get()));
#endif
}

WebPageProxy& webkitWebViewGetPage(WebKitWebView* webView)
{
    return getPage(webView);
}

void webkitWebViewWillStartLoad(WebKitWebView* webView)
{
    // Ignore the active URI changes happening before WEBKIT_LOAD_STARTED. If they are not user-initiated,
    // they could be a malicious attempt to trick users by loading an invalid URI on a trusted host, with the load
    // intended to stall, or perhaps be repeated. If we trust the URI here and display it to the user, then the user's
    // only indication that something is wrong would be a page loading indicator. If the load request is not
    // user-initiated, we must not trust it until WEBKIT_LOAD_COMMITTED. If the load is triggered by API
    // request, then the active URI is already the pending API request URL, so the blocking is harmless and the
    // client application will still see the URI update immediately. Otherwise, the URI update will be delayed a bit.
    webView->priv->isActiveURIChangeBlocked = true;

    // This is called before NavigationClient::didStartProvisionalNavigation(), the page load state hasn't been committed yet.
    auto& pageLoadState = getPage(webView).pageLoadState();
    if (pageLoadState.isFinished())
        return;

    GUniquePtr<GError> error(g_error_new_literal(WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED, _("Load request cancelled")));
    webkitWebViewLoadFailed(webView, pageLoadState.isProvisional() ? WEBKIT_LOAD_STARTED : WEBKIT_LOAD_COMMITTED,
        pageLoadState.isProvisional() ? pageLoadState.provisionalURL().utf8().data() : pageLoadState.url().utf8().data(),
        error.get());
}

void webkitWebViewLoadChanged(WebKitWebView* webView, WebKitLoadEvent loadEvent)
{
    WebKitWebViewPrivate* priv = webView->priv;
    switch (loadEvent) {
    case WEBKIT_LOAD_STARTED:
#if PLATFORM(GTK)
        webkitWebViewCancelFaviconRequest(webView);
        webkitWebViewWatchForChangesInFavicon(webView);
#endif
        webkitWebViewCompleteAuthenticationRequest(webView);
        priv->mainResource = nullptr;
        webView->priv->isActiveURIChangeBlocked = false;
        break;
    case WEBKIT_LOAD_COMMITTED: {
        auto activeURL = getPage(webView).pageLoadState().activeURL().utf8();
        // Active URL is trusted now. If it's different to our active URI, due to the
        // update block before WEBKIT_LOAD_STARTED, we update it here to be in sync
        // again with the page load state.
        if (activeURL != priv->activeURI) {
            priv->activeURI = activeURL;
            g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_URI]);
        }
#if PLATFORM(GTK)
        WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(priv->context.get());
        GUniquePtr<char> faviconURI(webkit_favicon_database_get_favicon_uri(database, priv->activeURI.data()));
        webkitWebViewUpdateFaviconURI(webView, faviconURI.get());
#endif
        break;
    }
    case WEBKIT_LOAD_FINISHED:
        webkitWebViewCompleteAuthenticationRequest(webView);
        break;
    default:
        break;
    }

    g_signal_emit(webView, signals[LOAD_CHANGED], 0, loadEvent);
}

void webkitWebViewLoadFailed(WebKitWebView* webView, WebKitLoadEvent loadEvent, const char* failingURI, GError *error)
{
    webkitWebViewCompleteAuthenticationRequest(webView);

    gboolean returnValue;
    g_signal_emit(webView, signals[LOAD_FAILED], 0, loadEvent, failingURI, error, &returnValue);
    g_signal_emit(webView, signals[LOAD_CHANGED], 0, WEBKIT_LOAD_FINISHED);
}

void webkitWebViewLoadFailedWithTLSErrors(WebKitWebView* webView, const char* failingURI, GError* error, GTlsCertificateFlags tlsErrors, GTlsCertificate* certificate)
{
    webkitWebViewCompleteAuthenticationRequest(webView);

    auto* websiteDataManager = webkit_web_view_get_website_data_manager(webView);
    WebKitTLSErrorsPolicy tlsErrorsPolicy = webkit_website_data_manager_get_tls_errors_policy(websiteDataManager);
    if (tlsErrorsPolicy == WEBKIT_TLS_ERRORS_POLICY_FAIL) {
        gboolean returnValue;
        g_signal_emit(webView, signals[LOAD_FAILED_WITH_TLS_ERRORS], 0, failingURI, certificate, tlsErrors, &returnValue);
        if (!returnValue)
            g_signal_emit(webView, signals[LOAD_FAILED], 0, WEBKIT_LOAD_STARTED, failingURI, error, &returnValue);
    }

    g_signal_emit(webView, signals[LOAD_CHANGED], 0, WEBKIT_LOAD_FINISHED);
}

#if PLATFORM(GTK)
void webkitWebViewGetLoadDecisionForIcon(WebKitWebView* webView, const LinkIcon& icon, Function<void(bool)>&& completionHandler)
{
    // We only support favicons for now.
    if (icon.type != LinkIconType::Favicon) {
        completionHandler(false);
        return;
    }
    WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(webView->priv->context.get());
    webkitFaviconDatabaseGetLoadDecisionForIcon(database, icon, getPage(webView).pageLoadState().activeURL(), webView->priv->isEphemeral, WTFMove(completionHandler));
}

void webkitWebViewSetIcon(WebKitWebView* webView, const LinkIcon& icon, API::Data& iconData)
{
    WebKitFaviconDatabase* database = webkit_web_context_get_favicon_database(webView->priv->context.get());
    webkitFaviconDatabaseSetIconForPageURL(database, icon, iconData, getPage(webView).pageLoadState().activeURL(), webView->priv->isEphemeral);
}
#endif

RefPtr<WebPageProxy> webkitWebViewCreateNewPage(WebKitWebView* webView, const WindowFeatures& windowFeatures, WebKitNavigationAction* navigationAction)
{
    WebKitWebView* newWebView;
    g_signal_emit(webView, signals[CREATE], 0, navigationAction, &newWebView);
    if (!newWebView)
        return nullptr;

    if (&getPage(webView).process() != &getPage(newWebView).process()) {
        g_warning("WebKitWebView returned by WebKitWebView::create signal was not created with the related WebKitWebView");
        return nullptr;
    }

    webkitWindowPropertiesUpdateFromWebWindowFeatures(newWebView->priv->windowProperties.get(), windowFeatures);

    return &getPage(newWebView);
}

void webkitWebViewReadyToShowPage(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[READY_TO_SHOW], 0, NULL);
}

void webkitWebViewRunAsModal(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[RUN_AS_MODAL], 0, NULL);

    webView->priv->modalLoop = adoptGRef(g_main_loop_new(0, FALSE));

#if PLATFORM(GTK) && !USE(GTK4)
// This is to suppress warnings about gdk_threads_leave and gdk_threads_enter.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    gdk_threads_leave();
#endif

    g_main_loop_run(webView->priv->modalLoop.get());

#if PLATFORM(GTK) && !USE(GTK4)
    gdk_threads_enter();
    ALLOW_DEPRECATED_DECLARATIONS_END
#endif
}

void webkitWebViewClosePage(WebKitWebView* webView)
{
    g_signal_emit(webView, signals[CLOSE], 0, NULL);
}

void webkitWebViewRunJavaScriptAlert(WebKitWebView* webView, const CString& message, Function<void()>&& completionHandler)
{
    ASSERT(!webView->priv->currentScriptDialog);
    webView->priv->currentScriptDialog = webkitScriptDialogCreate(WEBKIT_SCRIPT_DIALOG_ALERT, message, { }, [webView, completionHandler = WTFMove(completionHandler)](bool, const String&) {
        completionHandler();
        webView->priv->currentScriptDialog = nullptr;
    });
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_DIALOG], 0, webView->priv->currentScriptDialog, &returnValue);
    webkit_script_dialog_unref(webView->priv->currentScriptDialog);
}

void webkitWebViewRunJavaScriptConfirm(WebKitWebView* webView, const CString& message, Function<void(bool)>&& completionHandler)
{
    ASSERT(!webView->priv->currentScriptDialog);
    webView->priv->currentScriptDialog = webkitScriptDialogCreate(WEBKIT_SCRIPT_DIALOG_CONFIRM, message, { }, [webView, completionHandler = WTFMove(completionHandler)](bool result, const String&) {
        completionHandler(result);
        webView->priv->currentScriptDialog = nullptr;
    });
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_DIALOG], 0, webView->priv->currentScriptDialog, &returnValue);
    webkit_script_dialog_unref(webView->priv->currentScriptDialog);
}

void webkitWebViewRunJavaScriptPrompt(WebKitWebView* webView, const CString& message, const CString& defaultText, Function<void(const String&)>&& completionHandler)
{
    ASSERT(!webView->priv->currentScriptDialog);
    webView->priv->currentScriptDialog = webkitScriptDialogCreate(WEBKIT_SCRIPT_DIALOG_PROMPT, message, defaultText, [webView, completionHandler = WTFMove(completionHandler)](bool, const String& result) {
        completionHandler(result);
        webView->priv->currentScriptDialog = nullptr;
    });
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_DIALOG], 0, webView->priv->currentScriptDialog, &returnValue);
    webkit_script_dialog_unref(webView->priv->currentScriptDialog);
}

void webkitWebViewRunJavaScriptBeforeUnloadConfirm(WebKitWebView* webView, const CString& message, Function<void(bool)>&& completionHandler)
{
    ASSERT(!webView->priv->currentScriptDialog);
    webView->priv->currentScriptDialog = webkitScriptDialogCreate(WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM, message, { }, [webView, completionHandler = WTFMove(completionHandler)](bool result, const String&) {
        completionHandler(result);
        webView->priv->currentScriptDialog = nullptr;
    });
    gboolean returnValue;
    g_signal_emit(webView, signals[SCRIPT_DIALOG], 0, webView->priv->currentScriptDialog, &returnValue);
    webkit_script_dialog_unref(webView->priv->currentScriptDialog);
}

bool webkitWebViewIsShowingScriptDialog(WebKitWebView* webView)
{
    if (!webView->priv->currentScriptDialog)
        return false;

    // FIXME: Add API to ask the user in case default implementation is not being used.
    return webkitScriptDialogIsRunning(webView->priv->currentScriptDialog);
}

String webkitWebViewGetCurrentScriptDialogMessage(WebKitWebView* webView)
{
    if (!webView->priv->currentScriptDialog)
        return { };

    return String::fromUTF8(webView->priv->currentScriptDialog->message);
}

void webkitWebViewSetCurrentScriptDialogUserInput(WebKitWebView* webView, const String& userInput)
{
    if (!webView->priv->currentScriptDialog)
        return;

    if (webkitScriptDialogIsUserHandled(webView->priv->currentScriptDialog)) {
        // FIXME: Add API to ask the user in case default implementation is not being used.
        return;
    }

    if (webkitScriptDialogIsRunning(webView->priv->currentScriptDialog))
        webkitScriptDialogSetUserInput(webView->priv->currentScriptDialog, userInput);
}

void webkitWebViewAcceptCurrentScriptDialog(WebKitWebView* webView)
{
    if (!webView->priv->currentScriptDialog)
        return;

    if (webkitScriptDialogIsUserHandled(webView->priv->currentScriptDialog)) {
        // FIXME: Add API to ask the user in case default implementation is not being used.
        return;
    }

    if (webkitScriptDialogIsRunning(webView->priv->currentScriptDialog))
        webkitScriptDialogAccept(webView->priv->currentScriptDialog);
}

void webkitWebViewDismissCurrentScriptDialog(WebKitWebView* webView)
{
    if (!webView->priv->currentScriptDialog)
        return;

    if (webkitScriptDialogIsUserHandled(webView->priv->currentScriptDialog)) {
        // FIXME: Add API to ask the user in case default implementation is not being used.
        return;
    }

    if (webkitScriptDialogIsRunning(webView->priv->currentScriptDialog))
        webkitScriptDialogDismiss(webView->priv->currentScriptDialog);
}

std::optional<WebKitScriptDialogType> webkitWebViewGetCurrentScriptDialogType(WebKitWebView* webView)
{
    if (!webView->priv->currentScriptDialog)
        return std::nullopt;

    return static_cast<WebKitScriptDialogType>(webView->priv->currentScriptDialog->type);
}

void webkitWebViewMakePolicyDecision(WebKitWebView* webView, WebKitPolicyDecisionType type, WebKitPolicyDecision* decision)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[DECIDE_POLICY], 0, decision, type, &returnValue);
}

void webkitWebViewMakePermissionRequest(WebKitWebView* webView, WebKitPermissionRequest* request)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[PERMISSION_REQUEST], 0, request, &returnValue);
}

void webkitWebViewMouseTargetChanged(WebKitWebView* webView, const WebHitTestResultData& hitTestResult, OptionSet<WebEventModifier> modifiers)
{
#if PLATFORM(GTK)
    webkitWebViewBaseSetTooltipArea(WEBKIT_WEB_VIEW_BASE(webView), hitTestResult.elementBoundingBox);
    webkitWebViewBaseSetMouseIsOverScrollbar(WEBKIT_WEB_VIEW_BASE(webView), hitTestResult.isScrollbar);
#endif

    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->mouseTargetHitTestResult
        && priv->mouseTargetModifiers == modifiers
        && webkitHitTestResultCompare(priv->mouseTargetHitTestResult.get(), hitTestResult))
        return;

    priv->mouseTargetModifiers = modifiers;
    priv->mouseTargetHitTestResult = adoptGRef(webkitHitTestResultCreate(hitTestResult));
    g_signal_emit(webView, signals[MOUSE_TARGET_CHANGED], 0, priv->mouseTargetHitTestResult.get(), toPlatformModifiers(modifiers));
}

void webkitWebViewHandleDownloadRequest(WebKitWebView* webView, DownloadProxy* downloadProxy)
{
    ASSERT(downloadProxy);
    GRefPtr<WebKitDownload> download = webkitWebContextGetOrCreateDownload(downloadProxy);
    webkitDownloadSetWebView(download.get(), webView);
}

#if PLATFORM(GTK)
void webkitWebViewPrintFrame(WebKitWebView* webView, WebFrameProxy* frame)
{
    auto printOperation = adoptGRef(webkit_print_operation_new(webView));
    webkitPrintOperationSetPrintMode(printOperation.get(), PrintInfo::PrintModeSync);
    gboolean returnValue;
    g_signal_emit(webView, signals[PRINT], 0, printOperation.get(), &returnValue);
    if (returnValue)
        return;

    WebKitPrintOperationResponse response = webkitPrintOperationRunDialogForFrame(printOperation.get(), 0, frame);
    if (response == WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL)
        return;
    g_signal_connect(printOperation.leakRef(), "finished", G_CALLBACK(g_object_unref), 0);
}
#endif

WebKitWebResourceLoadManager* webkitWebViewGetWebResourceLoadManager(WebKitWebView* webView)
{
    return webView->priv->resourceLoadManager.get();
}

void webkitWebViewResourceLoadStarted(WebKitWebView* webView, WebKitWebResource* resource, ResourceRequest&& request)
{
    if (webkitWebResourceIsMainResource(resource))
        webView->priv->mainResource = resource;
    GRefPtr<WebKitURIRequest> uriRequest = adoptGRef(webkitURIRequestCreateForResourceRequest(request));
    g_signal_emit(webView, signals[RESOURCE_LOAD_STARTED], 0, resource, uriRequest.get());
}

void webkitWebViewEnterFullScreen(WebKitWebView* webView)
{
#if ENABLE(FULLSCREEN_API)
    gboolean returnValue;
    g_signal_emit(webView, signals[ENTER_FULLSCREEN], 0, &returnValue);
#if PLATFORM(GTK)
    if (!returnValue)
        webkitWebViewBaseEnterFullScreen(WEBKIT_WEB_VIEW_BASE(webView));
#endif
#endif
}

void webkitWebViewExitFullScreen(WebKitWebView* webView)
{
#if ENABLE(FULLSCREEN_API)
    gboolean returnValue;
    g_signal_emit(webView, signals[LEAVE_FULLSCREEN], 0, &returnValue);
#if PLATFORM(GTK)
    if (!returnValue)
        webkitWebViewBaseExitFullScreen(WEBKIT_WEB_VIEW_BASE(webView));
#endif
#endif
}

void webkitWebViewRunFileChooserRequest(WebKitWebView* webView, WebKitFileChooserRequest* request)
{
    gboolean returnValue;
    g_signal_emit(webView, signals[RUN_FILE_CHOOSER], 0, request, &returnValue);
}

#if PLATFORM(GTK)
void webkitWebViewPopulateContextMenu(WebKitWebView* webView, const Vector<WebContextMenuItemData>& proposedMenu, const WebHitTestResultData& hitTestResultData, GVariant* userData)
{
    WebKitWebViewBase* webViewBase = WEBKIT_WEB_VIEW_BASE(webView);
    WebContextMenuProxyGtk* contextMenuProxy = webkitWebViewBaseGetActiveContextMenuProxy(webViewBase);
    ASSERT(contextMenuProxy);

    GRefPtr<WebKitContextMenu> contextMenu = adoptGRef(webkitContextMenuCreate(proposedMenu));
    if (userData)
        webkit_context_menu_set_user_data(WEBKIT_CONTEXT_MENU(contextMenu.get()), userData);
    webkitContextMenuSetEvent(contextMenu.get(), webkitWebViewBaseTakeContextMenuEvent(webViewBase));

    GRefPtr<WebKitHitTestResult> hitTestResult = adoptGRef(webkitHitTestResultCreate(hitTestResultData));
    gboolean returnValue;
    g_signal_emit(webView, signals[CONTEXT_MENU], 0, contextMenu.get(),
#if !USE(GTK4)
        webkit_context_menu_get_event(contextMenu.get()),
#endif
        hitTestResult.get(), &returnValue);
    if (returnValue)
        return;

    Vector<WebContextMenuItemGlib> contextMenuItems;
    webkitContextMenuPopulate(contextMenu.get(), contextMenuItems);
    contextMenuProxy->populate(contextMenuItems);

    g_signal_connect(contextMenuProxy->gtkWidget(), WebContextMenuProxyGtk::widgetDismissedSignal, G_CALLBACK(+[](GtkWidget*, WebKitWebView* webView) {
        g_signal_emit(webView, signals[CONTEXT_MENU_DISMISSED], 0, nullptr);
    }), webView);

    // Clear the menu to make sure it's useless after signal emission.
    webkit_context_menu_remove_all(contextMenu.get());
}
#elif PLATFORM(WPE)
void webkitWebViewPopulateContextMenu(WebKitWebView* webView, const Vector<WebContextMenuItemData>& proposedMenu, const WebHitTestResultData& hitTestResultData, GVariant* userData)
{
    GRefPtr<WebKitContextMenu> contextMenu = adoptGRef(webkitContextMenuCreate(proposedMenu));
    if (userData)
        webkit_context_menu_set_user_data(WEBKIT_CONTEXT_MENU(contextMenu.get()), userData);
    GRefPtr<WebKitHitTestResult> hitTestResult = adoptGRef(webkitHitTestResultCreate(hitTestResultData));
    gboolean returnValue;
    g_signal_emit(webView, signals[CONTEXT_MENU], 0, contextMenu.get(),
#if !ENABLE(2022_GLIB_API)
        nullptr,
#endif
        hitTestResult.get(), &returnValue);
}
#endif

void webkitWebViewSubmitFormRequest(WebKitWebView* webView, WebKitFormSubmissionRequest* request)
{
    g_signal_emit(webView, signals[SUBMIT_FORM], 0, request);
}

void webkitWebViewHandleAuthenticationChallenge(WebKitWebView* webView, AuthenticationChallengeProxy* authenticationChallenge)
{
    auto* websiteDataManager = webkit_web_view_get_website_data_manager(webView);
    webView->priv->authenticationRequest = adoptGRef(webkitAuthenticationRequestCreate(authenticationChallenge,
        webView->priv->isEphemeral, webkit_website_data_manager_get_persistent_credential_storage_enabled(websiteDataManager)));
    gboolean returnValue;
    g_signal_emit(webView, signals[AUTHENTICATE], 0, webView->priv->authenticationRequest.get(), &returnValue);
}

void webkitWebViewInsecureContentDetected(WebKitWebView* webView, WebKitInsecureContentEvent type)
{
    g_signal_emit(webView, signals[INSECURE_CONTENT_DETECTED], 0, type);
}

bool webkitWebViewEmitShowNotification(WebKitWebView* webView, WebKitNotification* webNotification)
{
    gboolean handled;
    g_signal_emit(webView, signals[SHOW_NOTIFICATION], 0, webNotification, &handled);
    return handled;
}

#if PLATFORM(GTK)
bool webkitWebViewEmitRunColorChooser(WebKitWebView* webView, WebKitColorChooserRequest* request)
{
    gboolean handled;
    g_signal_emit(webView, signals[RUN_COLOR_CHOOSER], 0, request, &handled);
    return handled;
}
#endif

void webkitWebViewSelectionDidChange(WebKitWebView* webView)
{
    if (!webView->priv->editorState)
        return;

    webkitEditorStateChanged(webView->priv->editorState.get(), getPage(webView).editorState());
}

void webkitWebViewRequestInstallMissingMediaPlugins(WebKitWebView* webView, InstallMissingMediaPluginsPermissionRequest& request)
{
#if ENABLE(VIDEO) && !USE(GSTREAMER_FULL)
    GRefPtr<WebKitInstallMissingMediaPluginsPermissionRequest> installMediaPluginsPermissionRequest = adoptGRef(webkitInstallMissingMediaPluginsPermissionRequestCreate(request));
    webkitWebViewMakePermissionRequest(webView, WEBKIT_PERMISSION_REQUEST(installMediaPluginsPermissionRequest.get()));
#else
    ASSERT_NOT_REACHED();
#endif
}

WebKitWebsiteDataManager* webkitWebViewGetWebsiteDataManager(WebKitWebView* webView)
{
    return webView->priv->websiteDataManager.get();
}

#if PLATFORM(GTK)
bool webkitWebViewShowOptionMenu(WebKitWebView* webView, const IntRect& rect, WebKitOptionMenu* menu)
{
    GdkRectangle menuRect = rect;
    gboolean handled;
    g_signal_emit(webView, signals[SHOW_OPTION_MENU], 0, menu,
#if !USE(GTK4)
        webkit_option_menu_get_event(menu),
#endif
        &menuRect, &handled);
    return handled;
}
#endif

void webkitWebViewDidChangePageID(WebKitWebView* webView)
{
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_PAGE_ID]);
}

void webkitWebViewDidReceiveUserMessage(WebKitWebView* webView, UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    // Sink the floating ref.
    GRefPtr<WebKitUserMessage> userMessage = webkitUserMessageCreate(WTFMove(message), WTFMove(completionHandler));
    gboolean returnValue;
    g_signal_emit(webView, signals[USER_MESSAGE_RECEIVED], 0, userMessage.get(), &returnValue);
}

#if ENABLE(POINTER_LOCK)
void webkitWebViewRequestPointerLock(WebKitWebView* webView)
{
#if PLATFORM(GTK)
    webkitWebViewBaseRequestPointerLock(WEBKIT_WEB_VIEW_BASE(webView));
#endif
}

void webkitWebViewDenyPointerLockRequest(WebKitWebView* webView)
{
    getPage(webView).didDenyPointerLock();
}

void webkitWebViewDidLosePointerLock(WebKitWebView* webView)
{
#if PLATFORM(GTK)
    webkitWebViewBaseDidLosePointerLock(WEBKIT_WEB_VIEW_BASE(webView));
#endif
}
#endif

static void webkitWebViewSynthesizeCompositionKeyPress(WebKitWebView* webView, const String& text, std::optional<Vector<CompositionUnderline>>&& underlines, std::optional<EditingRange>&& selectionRange)
{
#if PLATFORM(GTK)
    webkitWebViewBaseSynthesizeCompositionKeyPress(WEBKIT_WEB_VIEW_BASE(webView), text, WTFMove(underlines), WTFMove(selectionRange));
#elif PLATFORM(WPE)
    webView->priv->view->synthesizeCompositionKeyPress(text, WTFMove(underlines), WTFMove(selectionRange));
#endif
}

void webkitWebViewSetComposition(WebKitWebView* webView, const String& text, const Vector<CompositionUnderline>& underlines, EditingRange&& selectionRange)
{
    webkitWebViewSynthesizeCompositionKeyPress(webView, text, underlines, WTFMove(selectionRange));
}

void webkitWebViewConfirmComposition(WebKitWebView* webView, const String& text)
{
    webkitWebViewSynthesizeCompositionKeyPress(webView, text, std::nullopt, std::nullopt);
}

void webkitWebViewCancelComposition(WebKitWebView* webView, const String& text)
{
    getPage(webView).cancelComposition(text);
}

void webkitWebViewDeleteSurrounding(WebKitWebView* webView, int offset, unsigned characterCount)
{
    getPage(webView).deleteSurrounding(offset, characterCount);
}

#if PLATFORM(WPE)
bool webkitWebViewShowOptionMenu(WebKitWebView* webView, const IntRect& rect, WebKitOptionMenu* menu)
{
    WebKitRectangle rectangle { rect.x(), rect.y(), rect.width(), rect.height() };

    gboolean handled;
    g_signal_emit(webView, signals[SHOW_OPTION_MENU], 0, menu, &rectangle, &handled);
    return handled;
}
#endif

void webkitWebViewPermissionStateQuery(WebKitWebView* webView, WebKitPermissionStateQuery* query)
{
    gboolean result;
    g_signal_emit(webView, signals[QUERY_PERMISSION_STATE], 0, query, &result);
}

#if PLATFORM(WPE)
/**
 * webkit_web_view_get_backend:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitWebViewBackend of @web_view
 *
 * Returns: (transfer none): the #WebKitWebViewBackend of @web_view
 *
 * Since: 2.20
 */
WebKitWebViewBackend* webkit_web_view_get_backend(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    return webView->priv->backend.get();
}
#endif

/**
 * webkit_web_view_get_context:
 * @web_view: a #WebKitWebView
 *
 * Gets the web context of @web_view.
 *
 * Returns: (transfer none): the #WebKitWebContext of the view
 */
WebKitWebContext* webkit_web_view_get_context(WebKitWebView *webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->context.get();
}

/**
 * webkit_web_view_get_user_content_manager:
 * @web_view: a #WebKitWebView
 *
 * Gets the user content manager associated to @web_view.
 *
 * Returns: (transfer none): the #WebKitUserContentManager associated with the view
 *
 * Since: 2.6
 */
WebKitUserContentManager* webkit_web_view_get_user_content_manager(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    return webView->priv->userContentManager.get();
}

/**
 * webkit_web_view_is_ephemeral:
 * @web_view: a #WebKitWebView
 *
 * Get whether a #WebKitWebView is ephemeral.
 *
 * To create an ephemeral #WebKitWebView you need to
 * use g_object_new() and pass is-ephemeral property with %TRUE value. See
 * #WebKitWebView:is-ephemeral for more details.
 * If @web_view was created with a ephemeral #WebKitWebView:related-view or an
 * ephemeral #WebKitWebView:web-context it will also be ephemeral.
 *
 * Returns: %TRUE if @web_view is ephemeral or %FALSE otherwise.
 *
 * Since: 2.16
 */
gboolean webkit_web_view_is_ephemeral(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return webView->priv->isEphemeral;
}

/**
 * webkit_web_view_is_controlled_by_automation:
 * @web_view: a #WebKitWebView
 *
 * Get whether a #WebKitWebView was created with #WebKitWebView:is-controlled-by-automation
 * property enabled.
 *
 * Only #WebKitWebView<!-- -->s controlled by automation can be used in an
 * automation session.
 *
 * Returns: %TRUE if @web_view is controlled by automation, or %FALSE otherwise.
 *
 * Since: 2.18
 */
gboolean webkit_web_view_is_controlled_by_automation(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return webView->priv->isControlledByAutomation;
}

/**
 * webkit_web_view_get_automation_presentation_type:
 * @web_view: a #WebKitWebView
 *
 * Get the presentation type of #WebKitWebView when created for automation.
 *
 * Returns: a #WebKitAutomationBrowsingContextPresentation.
 *
 * Since: 2.28
 */
WebKitAutomationBrowsingContextPresentation webkit_web_view_get_automation_presentation_type(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), WEBKIT_AUTOMATION_BROWSING_CONTEXT_PRESENTATION_WINDOW);

    return webView->priv->automationPresentationType;
}

/**
 * webkit_web_view_get_website_data_manager:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitWebsiteDataManager associated to @web_view.
 *
 * If @web_view is not ephemeral,
 * the returned #WebKitWebsiteDataManager will be the same as the #WebKitWebsiteDataManager
 * of @web_view's #WebKitWebContext.
 *
 * Returns: (transfer none): a #WebKitWebsiteDataManager
 *
 * Since: 2.16
 */
WebKitWebsiteDataManager* webkit_web_view_get_website_data_manager(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    if (webView->priv->websiteDataManager)
        return webView->priv->websiteDataManager.get();

    return webkit_web_context_get_website_data_manager(webView->priv->context.get());
}

/**
 * webkit_web_view_try_close:
 * @web_view: a #WebKitWebView
 *
 * Tries to close the @web_view.
 *
 * This will fire the onbeforeunload event
 * to ask the user for confirmation to close the page. If there isn't an
 * onbeforeunload event handler or the user confirms to close the page,
 * the #WebKitWebView::close signal is emitted, otherwise nothing happens.
 *
 * Since: 2.12
 */
void webkit_web_view_try_close(WebKitWebView *webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    if (getPage(webView).tryClose())
        webkitWebViewClosePage(webView);
}

/**
 * webkit_web_view_load_uri:
 * @web_view: a #WebKitWebView
 * @uri: an URI string
 *
 * Requests loading of the specified URI string.
 *
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_uri(WebKitWebView* webView, const gchar* uri)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(uri);

    getPage(webView).loadRequest(URL({ }, String::fromUTF8(uri)));
}

/**
 * webkit_web_view_load_html:
 * @web_view: a #WebKitWebView
 * @content: The HTML string to load
 * @base_uri: (allow-none): The base URI for relative locations or %NULL
 *
 * Load the given @content string with the specified @base_uri.
 *
 * If @base_uri is not %NULL, relative URLs in the @content will be
 * resolved against @base_uri and absolute local paths must be children of the @base_uri.
 * For security reasons absolute local paths that are not children of @base_uri
 * will cause the web process to terminate.
 * If you need to include URLs in @content that are local paths in a different
 * directory than @base_uri you can build a data URI for them. When @base_uri is %NULL,
 * it defaults to "about:blank". The mime type of the document will be "text/html".
 * You can monitor the load operation by connecting to #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_html(WebKitWebView* webView, const gchar* content, const gchar* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);

    getPage(webView).loadData({ reinterpret_cast<const uint8_t*>(content), content ? strlen(content) : 0 }, "text/html"_s, "UTF-8"_s, String::fromUTF8(baseURI));
}

/**
 * webkit_web_view_load_alternate_html:
 * @web_view: a #WebKitWebView
 * @content: the new content to display as the main page of the @web_view
 * @content_uri: the URI for the alternate page content
 * @base_uri: (allow-none): the base URI for relative locations or %NULL
 *
 * Load the given @content string for the URI @content_uri.
 *
 * This allows clients to display page-loading errors in the #WebKitWebView itself.
 * When this method is called from #WebKitWebView::load-failed signal to show an
 * error page, then the back-forward list is maintained appropriately.
 * For everything else this method works the same way as webkit_web_view_load_html().
 */
void webkit_web_view_load_alternate_html(WebKitWebView* webView, const gchar* content, const gchar* contentURI, const gchar* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(content);
    g_return_if_fail(contentURI);

    getPage(webView).loadAlternateHTML(WebCore::DataSegment::create(Vector<uint8_t>(reinterpret_cast<const uint8_t*>(content), content ? strlen(content) : 0)), "UTF-8"_s, URL { String::fromUTF8(baseURI) }, URL { String::fromUTF8(contentURI) });
}

/**
 * webkit_web_view_load_plain_text:
 * @web_view: a #WebKitWebView
 * @plain_text: The plain text to load
 *
 * Load the specified @plain_text string into @web_view.
 *
 * The mime type of document will be "text/plain". You can monitor the load
 * operation by connecting to #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_plain_text(WebKitWebView* webView, const gchar* plainText)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(plainText);

    getPage(webView).loadData({ reinterpret_cast<const uint8_t*>(plainText), plainText ? strlen(plainText) : 0 }, "text/plain"_s, "UTF-8"_s, aboutBlankURL().string());
}

/**
 * webkit_web_view_load_bytes:
 * @web_view: a #WebKitWebView
 * @bytes: input data to load
 * @mime_type: (allow-none): the MIME type of @bytes, or %NULL
 * @encoding: (allow-none): the character encoding of @bytes, or %NULL
 * @base_uri: (allow-none): the base URI for relative locations or %NULL
 *
 * Load the specified @bytes into @web_view using the given @mime_type and @encoding.
 *
 * When @mime_type is %NULL, it defaults to "text/html".
 * When @encoding is %NULL, it defaults to "UTF-8".
 * When @base_uri is %NULL, it defaults to "about:blank".
 * You can monitor the load operation by connecting to #WebKitWebView::load-changed signal.
 *
 * Since: 2.6
 */
void webkit_web_view_load_bytes(WebKitWebView* webView, GBytes* bytes, const char* mimeType, const char* encoding, const char* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(bytes);

    gsize bytesDataSize;
    gconstpointer bytesData = g_bytes_get_data(bytes, &bytesDataSize);
    g_return_if_fail(bytesDataSize);

    getPage(webView).loadData({ reinterpret_cast<const uint8_t*>(bytesData), bytesDataSize }, mimeType ? String::fromUTF8(mimeType) : String::fromUTF8("text/html"),
        encoding ? String::fromUTF8(encoding) : String::fromUTF8("UTF-8"), String::fromUTF8(baseURI));
}

/**
 * webkit_web_view_load_request:
 * @web_view: a #WebKitWebView
 * @request: a #WebKitURIRequest to load
 *
 * Requests loading of the specified #WebKitURIRequest.
 *
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_load_request(WebKitWebView* webView, WebKitURIRequest* request)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_URI_REQUEST(request));

    ResourceRequest resourceRequest;
    webkitURIRequestGetResourceRequest(request, resourceRequest);
    getPage(webView).loadRequest(WTFMove(resourceRequest));
}

/**
 * webkit_web_view_get_page_id:
 * @web_view: a #WebKitWebView
 *
 * Get the identifier of the #WebKitWebPage corresponding to
 * the #WebKitWebView
 *
 * Returns: the page ID of @web_view.
 */
guint64 webkit_web_view_get_page_id(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return getPage(webView).webPageID().toUInt64();
}

/**
 * webkit_web_view_get_title:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:title property.
 *
 * You can connect to notify::title signal of @web_view to
 * be notified when the title has been received.
 *
 * Returns: The main frame document title of @web_view.
 */
const gchar* webkit_web_view_get_title(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->title.data();
}

/**
 * webkit_web_view_reload:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view.
 *
 * See also webkit_web_view_reload_bypass_cache().
 */
void webkit_web_view_reload(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView).reload({ });
}

/**
 * webkit_web_view_reload_bypass_cache:
 * @web_view: a #WebKitWebView
 *
 * Reloads the current contents of @web_view without
 * using any cached data.
 */
void webkit_web_view_reload_bypass_cache(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView).reload(ReloadOption::FromOrigin);
}

/**
 * webkit_web_view_stop_loading:
 * @web_view: a #WebKitWebView
 *
 * Stops any ongoing loading operation in @web_view.
 *
 * This method does nothing if no content is being loaded.
 * If there is a loading operation in progress, it will be cancelled and
 * #WebKitWebView::load-failed signal will be emitted with
 * %WEBKIT_NETWORK_ERROR_CANCELLED error.
 */
void webkit_web_view_stop_loading(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView).stopLoading();
}

/**
 * webkit_web_view_is_loading:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:is-loading property.
 *
 * You can monitor when a #WebKitWebView is loading a page by connecting to
 * notify::is-loading signal of @web_view. This is useful when you are
 * interesting in knowing when the view is loading something but not in the
 * details about the status of the load operation, for example to start a spinner
 * when the view is loading a page and stop it when it finishes.
 *
 * Returns: %TRUE if @web_view is loading a page or %FALSE otherwise.
 */
gboolean webkit_web_view_is_loading(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return webView->priv->isLoading;
}

/**
 * webkit_web_view_is_playing_audio:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:is-playing-audio property.
 *
 * You can monitor when a page in a #WebKitWebView is playing audio by
 * connecting to the notify::is-playing-audio signal of @web_view. This
 * is useful when the application wants to provide visual feedback when a
 * page is producing sound.
 *
 * Returns: %TRUE if a page in @web_view is playing audio or %FALSE otherwise.
 *
 * Since: 2.8
 */
gboolean webkit_web_view_is_playing_audio(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return getPage(webView).isPlayingAudio();
}

/**
 * webkit_web_view_set_is_muted:
 * @web_view: a #WebKitWebView
 * @muted: mute flag
 *
 * Sets the mute state of @web_view.
 *
 * Since: 2.30
 */
void webkit_web_view_set_is_muted(WebKitWebView* webView, gboolean muted)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_get_is_muted (webView) == muted)
        return;

    WebCore::MediaProducer::MutedStateFlags audioMutedFlag;
    if (muted)
        audioMutedFlag.add(WebCore::MediaProducer::MutedState::AudioIsMuted);
    getPage(webView).setMuted(audioMutedFlag);
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_IS_MUTED]);
}

/**
 * webkit_web_view_get_is_muted:
 * @web_view: a #WebKitWebView
 *
 * Gets the mute state of @web_view.
 *
 * Returns: %TRUE if @web_view audio is muted or %FALSE is audio is not muted.
 *
 * Since: 2.30
 */
gboolean webkit_web_view_get_is_muted(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return getPage(webView).isAudioMuted();
}

/**
 * webkit_web_view_go_back:
 * @web_view: a #WebKitWebView
 *
 * Loads the previous history item.
 *
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_back(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView).goBack();
}

/**
 * webkit_web_view_can_go_back:
 * @web_view: a #WebKitWebView
 *
 * Determines whether @web_view has a previous history item.
 *
 * Returns: %TRUE if able to move back or %FALSE otherwise.
 */
gboolean webkit_web_view_can_go_back(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return !!getPage(webView).backForwardList().backItem();
}

/**
 * webkit_web_view_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Loads the next history item.
 *
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_forward(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView).goForward();
}

/**
 * webkit_web_view_can_go_forward:
 * @web_view: a #WebKitWebView
 *
 * Determines whether @web_view has a next history item.
 *
 * Returns: %TRUE if able to move forward or %FALSE otherwise.
 */
gboolean webkit_web_view_can_go_forward(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return !!getPage(webView).backForwardList().forwardItem();
}

/**
 * webkit_web_view_get_uri:
 * @web_view: a #WebKitWebView
 *
 * Returns the current active URI of @web_view.
 *
 * The active URI might change during
 * a load operation:
 *
 * <orderedlist>
 * <listitem><para>
 *   When nothing has been loaded yet on @web_view the active URI is %NULL.
 * </para></listitem>
 * <listitem><para>
 *   When a new load operation starts the active URI is the requested URI:
 *   <itemizedlist>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_uri(),
 *     the requested URI is the given one.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_html(),
 *     the requested URI is "about:blank".
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_load_alternate_html(),
 *     the requested URI is content URI provided.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by webkit_web_view_go_back() or
 *     webkit_web_view_go_forward(), the requested URI is the original URI
 *     of the previous/next item in the #WebKitBackForwardList of @web_view.
 *   </para></listitem>
 *   <listitem><para>
 *     If the load operation was started by
 *     webkit_web_view_go_to_back_forward_list_item(), the requested URI
 *     is the opriginal URI of the given #WebKitBackForwardListItem.
 *   </para></listitem>
 *   </itemizedlist>
 * </para></listitem>
 * <listitem><para>
 *   If there is a server redirection during the load operation,
 *   the active URI is the redirected URI. When the signal
 *   #WebKitWebView::load-changed is emitted with %WEBKIT_LOAD_REDIRECTED
 *   event, the active URI is already updated to the redirected URI.
 * </para></listitem>
 * <listitem><para>
 *   When the signal #WebKitWebView::load-changed is emitted
 *   with %WEBKIT_LOAD_COMMITTED event, the active URI is the final
 *   one and it will not change unless a new load operation is started
 *   or a navigation action within the same page is performed.
 * </para></listitem>
 * </orderedlist>
 *
 * You can monitor the active URI by connecting to the notify::uri
 * signal of @web_view.
 *
 * Returns: the current active URI of @web_view or %NULL
 *    if nothing has been loaded yet.
 */
const gchar* webkit_web_view_get_uri(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->activeURI.data();
}

#if PLATFORM(GTK)
/**
 * webkit_web_view_get_favicon:
 * @web_view: a #WebKitWebView
 *
 * Returns favicon currently associated to @web_view.
 *
 * Returns favicon currently associated to @web_view, if any. You can
 * connect to notify::favicon signal of @web_view to be notified when
 * the favicon is available.
 *
 * Returns: (transfer none): a pointer to a #cairo_surface_t with the
 *    favicon or %NULL if there's no icon associated with @web_view.
 */
cairo_surface_t* webkit_web_view_get_favicon(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    if (webView->priv->activeURI.isNull())
        return 0;

    return webView->priv->favicon.get();
}
#endif

/**
 * webkit_web_view_get_custom_charset:
 * @web_view: a #WebKitWebView
 *
 * Returns the current custom character encoding name of @web_view.
 *
 * Returns: the current custom character encoding name or %NULL if no
 *    custom character encoding has been set.
 */
const gchar* webkit_web_view_get_custom_charset(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    String customTextEncoding = getPage(webView).customTextEncodingName();
    if (customTextEncoding.isEmpty())
        return 0;

    webView->priv->customTextEncoding = customTextEncoding.utf8();
    return webView->priv->customTextEncoding.data();
}

/**
 * webkit_web_view_set_custom_charset:
 * @web_view: a #WebKitWebView
 * @charset: (allow-none): a character encoding name or %NULL
 *
 * Sets the current custom character encoding override of @web_view.
 *
 * The custom character encoding will override any text encoding detected via HTTP headers or
 * META tags. Calling this method will stop any current load operation and reload the
 * current page. Setting the custom character encoding to %NULL removes the character
 * encoding override.
 */
void webkit_web_view_set_custom_charset(WebKitWebView* webView, const gchar* charset)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView).setCustomTextEncodingName(String::fromUTF8(charset));
}

/**
 * webkit_web_view_get_estimated_load_progress:
 * @web_view: a #WebKitWebView
 *
 * Gets the value of the #WebKitWebView:estimated-load-progress property.
 *
 * You can monitor the estimated progress of a load operation by
 * connecting to the notify::estimated-load-progress signal of @web_view.
 *
 * Returns: an estimate of the of the percent complete for a document
 *     load as a range from 0.0 to 1.0.
 */
gdouble webkit_web_view_get_estimated_load_progress(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    return getPage(webView).pageLoadState().estimatedProgress();
}

/**
 * webkit_web_view_get_back_forward_list:
 * @web_view: a #WebKitWebView
 *
 * Obtains the #WebKitBackForwardList associated with the given #WebKitWebView.
 *
 * The #WebKitBackForwardList is owned by the #WebKitWebView.
 *
 * Returns: (transfer none): the #WebKitBackForwardList
 */
WebKitBackForwardList* webkit_web_view_get_back_forward_list(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->backForwardList.get();
}

/**
 * webkit_web_view_go_to_back_forward_list_item:
 * @web_view: a #WebKitWebView
 * @list_item: a #WebKitBackForwardListItem
 *
 * Loads the specific history item @list_item.
 *
 * You can monitor the load operation by connecting to
 * #WebKitWebView::load-changed signal.
 */
void webkit_web_view_go_to_back_forward_list_item(WebKitWebView* webView, WebKitBackForwardListItem* listItem)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_BACK_FORWARD_LIST_ITEM(listItem));

    getPage(webView).goToBackForwardItem(*webkitBackForwardListItemGetItem(listItem));
}

/**
 * webkit_web_view_set_settings:
 * @web_view: a #WebKitWebView
 * @settings: a #WebKitSettings
 *
 * Sets the #WebKitSettings to be applied to @web_view.
 *
 * The
 * existing #WebKitSettings of @web_view will be replaced by
 * @settings. New settings are applied immediately on @web_view.
 * The same #WebKitSettings object can be shared
 * by multiple #WebKitWebView<!-- -->s.
 */
void webkit_web_view_set_settings(WebKitWebView* webView, WebKitSettings* settings)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_SETTINGS(settings));

    if (webView->priv->settings == settings)
        return;

    // The "settings" property is set on construction, and in that
    // case webkit_web_view_set_settings() will be called *before*
    // any settings have been assigned. In that case there are no
    // signal handlers to disconnect.
    if (webView->priv->settings)
        webkitWebViewDisconnectSettingsSignalHandlers(webView);

    webView->priv->settings = settings;
    webkitWebViewUpdateSettings(webView);
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_SETTINGS]);
}

/**
 * webkit_web_view_get_settings:
 * @web_view: a #WebKitWebView
 *
 * Gets the #WebKitSettings currently applied to @web_view.
 *
 * If no other #WebKitSettings have been explicitly applied to
 * @web_view with webkit_web_view_set_settings(), the default
 * #WebKitSettings will be returned. This method always returns
 * a valid #WebKitSettings object.
 * To modify any of the @web_view settings, you can either create
 * a new #WebKitSettings object with webkit_settings_new(), setting
 * the desired preferences, and then replace the existing @web_view
 * settings with webkit_web_view_set_settings() or get the existing
 * @web_view settings and update it directly. #WebKitSettings objects
 * can be shared by multiple #WebKitWebView<!-- -->s, so modifying
 * the settings of a #WebKitWebView would affect other
 * #WebKitWebView<!-- -->s using the same #WebKitSettings.
 *
 * Returns: (transfer none): the #WebKitSettings attached to @web_view
 */
WebKitSettings* webkit_web_view_get_settings(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);
    return webView->priv->settings.get();
}

/**
 * webkit_web_view_get_window_properties:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitWindowProperties object.
 *
 * Get the #WebKitWindowProperties object containing the properties
 * that the window containing @web_view should have.
 *
 * Returns: (transfer none): the #WebKitWindowProperties of @web_view
 */
WebKitWindowProperties* webkit_web_view_get_window_properties(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->windowProperties.get();
}

/**
 * webkit_web_view_set_zoom_level:
 * @web_view: a #WebKitWebView
 * @zoom_level: the zoom level
 *
 * Set the zoom level of @web_view.
 *
 * Set the zoom level of @web_view, i.e. the factor by which the
 * view contents are scaled with respect to their original size.
 */
void webkit_web_view_set_zoom_level(WebKitWebView* webView, gdouble zoomLevel)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (webkit_web_view_get_zoom_level(webView) == zoomLevel)
        return;

    auto& page = getPage(webView);
    if (webkit_settings_get_zoom_text_only(webView->priv->settings.get()))
        page.setTextZoomFactor(zoomLevel * webView->priv->textScaleFactor);
    else
        page.setPageZoomFactor(zoomLevel);
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_ZOOM_LEVEL]);
}

/**
 * webkit_web_view_get_zoom_level:
 * @web_view: a #WebKitWebView
 *
 * Set the zoom level of @web_view.
 *
 * Get the zoom level of @web_view, i.e. the factor by which the
 * view contents are scaled with respect to their original size.
 *
 * Returns: the current zoom level of @web_view
 */
gdouble webkit_web_view_get_zoom_level(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 1);

    auto& page = getPage(webView);
    gboolean zoomTextOnly = webkit_settings_get_zoom_text_only(webView->priv->settings.get());
    return zoomTextOnly ? page.textZoomFactor() / webView->priv->textScaleFactor : page.pageZoomFactor();
}

/**
 * webkit_web_view_can_execute_editing_command:
 * @web_view: a #WebKitWebView
 * @command: the command to check
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously check if it is possible to execute the given editing command.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_view_can_execute_editing_command_finish() to get the result of the operation.
 */
void webkit_web_view_can_execute_editing_command(WebKitWebView* webView, const char* command, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(command);

    GRefPtr<GTask> task = adoptGRef(g_task_new(webView, cancellable, callback, userData));
    getPage(webView).validateCommand(String::fromUTF8(command), [task = WTFMove(task)](bool isEnabled, int32_t) {
        g_task_return_boolean(task.get(), isEnabled);
    });
}

/**
 * webkit_web_view_can_execute_editing_command_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_can_execute_editing_command().
 *
 * Returns: %TRUE if the editing command can be executed or %FALSE otherwise
 */
gboolean webkit_web_view_can_execute_editing_command_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, webView), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * webkit_web_view_execute_editing_command:
 * @web_view: a #WebKitWebView
 * @command: the command to execute
 *
 * Request to execute the given @command for @web_view.
 *
 * You can use webkit_web_view_can_execute_editing_command() to check whether
 * it's possible to execute the command.
 */
void webkit_web_view_execute_editing_command(WebKitWebView* webView, const char* command)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(command);

    getPage(webView).executeEditCommand(String::fromUTF8(command));
}

/**
 * webkit_web_view_execute_editing_command_with_argument:
 * @web_view: a #WebKitWebView
 * @command: the command to execute
 * @argument: the command argument
 *
 * Request to execute the given @command with @argument for @web_view.
 *
 * You can use
 * webkit_web_view_can_execute_editing_command() to check whether
 * it's possible to execute the command.
 *
 * Since: 2.10
 */
void webkit_web_view_execute_editing_command_with_argument(WebKitWebView* webView, const char* command, const char* argument)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(command);
    g_return_if_fail(argument);

    getPage(webView).executeEditCommand(String::fromUTF8(command), String::fromUTF8(argument));
}

/**
 * webkit_web_view_get_find_controller:
 * @web_view: the #WebKitWebView
 *
 * Gets the #WebKitFindController.
 *
 * Gets the #WebKitFindController that will allow the caller to query
 * the #WebKitWebView for the text to look for.
 *
 * Returns: (transfer none): the #WebKitFindController associated to
 * this particular #WebKitWebView.
 */
WebKitFindController* webkit_web_view_get_find_controller(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    if (!webView->priv->findController)
        webView->priv->findController = adoptGRef(WEBKIT_FIND_CONTROLLER(g_object_new(WEBKIT_TYPE_FIND_CONTROLLER, "web-view", webView, NULL)));

    return webView->priv->findController.get();
}

#if PLATFORM(GTK)
/**
 * webkit_web_view_get_javascript_global_context: (skip)
 * @web_view: a #WebKitWebView
 *
 * Get the global JavaScript context.
 *
 * Get the global JavaScript context used by @web_view to deserialize the
 * result values of scripts executed with webkit_web_view_run_javascript().
 *
 * Returns: the <function>JSGlobalContextRef</function> used by @web_view to deserialize
 *    the result values of scripts.
 *
 * Deprecated: 2.22: Use jsc_value_get_context() instead.
 */
JSGlobalContextRef webkit_web_view_get_javascript_global_context(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    // We keep a reference to the js context in the view only when this method is called
    // for backwards compatibility.
    if (!webView->priv->jsContext)
        webView->priv->jsContext = API::SerializedScriptValue::sharedJSCContext();
    return jscContextGetJSContext(webView->priv->jsContext.get());
}
#endif

static void webkitWebViewRunJavaScriptCallback(API::SerializedScriptValue* wkSerializedScriptValue, const ExceptionDetails& exceptionDetails, GTask* task)
{
    if (g_task_return_error_if_cancelled(task))
        return;

    if (!wkSerializedScriptValue) {
        StringBuilder builder;
        if (!exceptionDetails.sourceURL.isEmpty()) {
            builder.append(exceptionDetails.sourceURL);
            if (exceptionDetails.lineNumber > 0)
                builder.append(':', exceptionDetails.lineNumber);
            if (exceptionDetails.columnNumber > 0)
                builder.append(':', exceptionDetails.columnNumber);
            builder.append(": ");
        }
        builder.append(exceptionDetails.message);
        g_task_return_new_error(task, WEBKIT_JAVASCRIPT_ERROR, WEBKIT_JAVASCRIPT_ERROR_SCRIPT_FAILED,
            "%s", builder.toString().utf8().data());
        return;
    }

    g_task_return_pointer(task, webkitJavascriptResultCreate(wkSerializedScriptValue->internalRepresentation()),
        reinterpret_cast<GDestroyNotify>(webkit_javascript_result_unref));
}

static void webkitWebViewRunJavaScriptWithParams(WebKitWebView* webView, const gchar* script, RunJavaScriptParameters&& params, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    GRefPtr<GTask> task = adoptGRef(g_task_new(webView, cancellable, callback, userData));

    getPage(webView).runJavaScriptInMainFrame(WTFMove(params), [task = WTFMove(task)] (auto&& result) {
        RefPtr<API::SerializedScriptValue> serializedScriptValue;
        ExceptionDetails exceptionDetails;
        if (result.has_value())
            serializedScriptValue = WTFMove(result.value());
        else
            exceptionDetails = WTFMove(result.error());
        webkitWebViewRunJavaScriptCallback(serializedScriptValue.get(), exceptionDetails, task.get());
    });
}

void webkitWebViewRunJavascriptWithoutForcedUserGestures(WebKitWebView* webView, const gchar* script, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(script);

    RunJavaScriptParameters params = { String::fromUTF8(script), URL { }, false, std::nullopt, false };
    webkitWebViewRunJavaScriptWithParams(webView, script, WTFMove(params), cancellable, callback, userData);
}

/**
 * webkit_web_view_run_javascript:
 * @web_view: a #WebKitWebView
 * @script: the script to run
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the script finished
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously run @script in the context of the current page in @web_view.
 *
 * If
 * WebKitSettings:enable-javascript is FALSE, this method will do nothing.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_view_run_javascript_finish() to get the result of the operation.
 */
void webkit_web_view_run_javascript(WebKitWebView* webView, const gchar* script, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(script);

    RunJavaScriptParameters params = { String::fromUTF8(script), URL { }, false, std::nullopt, true };
    webkitWebViewRunJavaScriptWithParams(webView, script, WTFMove(params), cancellable, callback, userData);
}

/**
 * webkit_web_view_run_javascript_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_run_javascript().
 *
 * This is an example of using webkit_web_view_run_javascript() with a script returning
 * a string:
 *
 * ```c
 * static void
 * web_view_javascript_finished (GObject      *object,
 *                               GAsyncResult *result,
 *                               gpointer      user_data)
 * {
 *     WebKitJavascriptResult *js_result;
 *     JSCValue               *value;
 *     GError                 *error = NULL;
 *
 *     js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (object), result, &error);
 *     if (!js_result) {
 *         g_warning ("Error running javascript: %s", error->message);
 *         g_error_free (error);
 *         return;
 *     }
 *
 *     value = webkit_javascript_result_get_js_value (js_result);
 *     if (jsc_value_is_string (value)) {
 *         gchar        *str_value = jsc_value_to_string (value);
 *         JSCException *exception = jsc_context_get_exception (jsc_value_get_context (value));
 *         if (exception)
 *             g_warning ("Error running javascript: %s", jsc_exception_get_message (exception));
 *         else
 *             g_print ("Script result: %s\n", str_value);
 *         g_free (str_value);
 *     } else {
 *         g_warning ("Error running javascript: unexpected return value");
 *     }
 *     webkit_javascript_result_unref (js_result);
 * }
 *
 * static void
 * web_view_get_link_url (WebKitWebView *web_view,
 *                        const gchar   *link_id)
 * {
 *     gchar *script = g_strdup_printf ("window.document.getElementById('%s').href;", link_id);
 *     webkit_web_view_run_javascript (web_view, script, NULL, web_view_javascript_finished, NULL);
 *     g_free (script);
 * }
 * ```
 *
 * Returns: (transfer full): a #WebKitJavascriptResult with the result of the last executed statement in @script
 *    or %NULL in case of error
 */
WebKitJavascriptResult* webkit_web_view_run_javascript_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, webView), nullptr);

    return static_cast<WebKitJavascriptResult*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_web_view_run_javascript_in_world:
 * @web_view: a #WebKitWebView
 * @script: the script to run
 * @world_name: the name of a #WebKitScriptWorld
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the script finished
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously run @script in the script world.
 *
 * Asynchronously run @script in the script world with name @world_name of the current page context in @web_view.
 * If WebKitSettings:enable-javascript is FALSE, this method will do nothing.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_view_run_javascript_in_world_finish() to get the result of the operation.
 *
 * Since: 2.22
 */
void webkit_web_view_run_javascript_in_world(WebKitWebView* webView, const gchar* script, const char* worldName, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(script);
    g_return_if_fail(worldName);

    GRefPtr<GTask> task = adoptGRef(g_task_new(webView, cancellable, callback, userData));
    auto world = API::ContentWorld::sharedWorldWithName(String::fromUTF8(worldName));
    getPage(webView).runJavaScriptInFrameInScriptWorld({ String::fromUTF8(script), URL { }, false, std::nullopt, true }, std::nullopt, world.get(), [task = WTFMove(task)] (auto&& result) {
        RefPtr<API::SerializedScriptValue> serializedScriptValue;
        ExceptionDetails exceptionDetails;
        if (result.has_value())
            serializedScriptValue = WTFMove(result.value());
        else
            exceptionDetails = WTFMove(result.error());
        webkitWebViewRunJavaScriptCallback(serializedScriptValue.get(), exceptionDetails, task.get());
    });
}

/*
 * webkit_web_view_run_async_javascript_function_in_world:
 * @web_view: a #WebKitWebView
 * @body: the JavaScript function body
 * @arguments: a #GVariant with format `{&sv}` storing the function arguments. Function argument values must be one of the following types, or contain only the following GVariant types: number, string, array, and dictionary.
 * @world_name (nullable): the name of a #WebKitScriptWorld, if no name (i.e. %NULL) is provided, the default world is used. Any value that is not %NULL is a distinct world.
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the script finished
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously run @body in the script world with name @world_name of the current page context in
 * @web_view. If WebKitSettings:enable-javascript is FALSE, this method will do nothing. This API
 * differs from webkit_web_view_run_javascript_in_world() in that the JavaScript function can return a
 * Promise and its result will be properly passed to the callback.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_view_run_javascript_in_world_finish() to get the result of the operation.
 *
 * For instance here is a dummy example that shows how to pass arguments to a JS function that
 * returns a Promise that resolves with the passed argument:
 *
 * ```c
 * static void
 * web_view_javascript_finished (GObject      *object,
 *                               GAsyncResult *result,
 *                               gpointer      user_data)
 * {
 *     WebKitJavascriptResult *js_result;
 *     JSCValue               *value;
 *     GError                 *error = NULL;
 *
 *     js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (object), result, &error);
 *     if (!js_result) {
 *         g_warning ("Error running javascript: %s", error->message);
 *         g_error_free (error);
 *         return;
 *     }
 *
 *     value = webkit_javascript_result_get_js_value (js_result);
 *     if (jsc_value_is_number (value)) {
 *         gint32        int_value = jsc_value_to_string (value);
 *         JSCException *exception = jsc_context_get_exception (jsc_value_get_context (value));
 *         if (exception)
 *             g_warning ("Error running javascript: %s", jsc_exception_get_message (exception));
 *         else
 *             g_print ("Script result: %d\n", int_value);
 *         g_free (str_value);
 *     } else {
 *         g_warning ("Error running javascript: unexpected return value");
 *     }
 *     webkit_javascript_result_unref (js_result);
 * }
 *
 * static void
 * web_view_evaluate_promise (WebKitWebView *web_view)
 * {
 *     GVariantDict dict;
 *     g_variant_dict_init (&dict, NULL);
 *     g_variant_dict_insert (&dict, "count", "u", 42);
 *     GVariant *args = g_variant_dict_end (&dict);
 *     const gchar *body = "return new Promise((resolve) => { resolve(count); });";
 *     webkit_web_view_run_async_javascript_function_in_world (web_view, body, arguments, NULL, NULL, web_view_javascript_finished, NULL);
 * }
 * ```
 *
 * Since: 2.38
 */
void webkit_web_view_run_async_javascript_function_in_world(WebKitWebView* webView, const gchar* body, GVariant* arguments, const char* worldName, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(body);

    auto task = adoptGRef(g_task_new(webView, cancellable, callback, userData));
    auto world = worldName ? API::ContentWorld::sharedWorldWithName(String::fromUTF8(worldName)) : Ref<API::ContentWorld> { API::ContentWorld::pageContentWorld() };
    bool hasInvalidArgument = false;
    auto argumentsMap = WebCore::ArgumentWireBytesMap { };

    if (arguments) {
        GVariantIter iter;
        g_variant_iter_init(&iter, arguments);
        const char* key;
        GVariant* value;
        while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
            if (!key)
                continue;
            auto serializedValue = API::SerializedScriptValue::createFromGVariant(value);
            if (!serializedValue) {
                hasInvalidArgument = true;
                break;
            }
            argumentsMap.set(String::fromUTF8(key), serializedValue->internalRepresentation().wireBytes());
        }
    }

    if (hasInvalidArgument) {
        ExceptionDetails exceptionDetails;
        exceptionDetails.message = "Function argument values must be one of the following types, or contain only the following GVariant types: number, string, array, and dictionary"_s;
        webkitWebViewRunJavaScriptCallback(nullptr, exceptionDetails, task.get());
        return;
    }

    getPage(webView).runJavaScriptInFrameInScriptWorld({ String::fromUTF8(body), URL { }, RunAsAsyncFunction::Yes, WTFMove(argumentsMap), ForceUserGesture::Yes }, std::nullopt, world.get(), [task = WTFMove(task)](auto&& result) {
        RefPtr<API::SerializedScriptValue> serializedScriptValue;
        ExceptionDetails exceptionDetails;
        if (result.has_value())
            serializedScriptValue = WTFMove(result.value());
        else
            exceptionDetails = WTFMove(result.error());
        webkitWebViewRunJavaScriptCallback(serializedScriptValue.get(), exceptionDetails, task.get());
    });
}

/**
 * webkit_web_view_run_javascript_in_world_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_run_javascript_in_world().
 *
 * Returns: (transfer full): a #WebKitJavascriptResult with the result of the last executed statement in @script
 *    or %NULL in case of error
 *
 * Since: 2.22
 */
WebKitJavascriptResult* webkit_web_view_run_javascript_in_world_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, webView), nullptr);

    return static_cast<WebKitJavascriptResult*>(g_task_propagate_pointer(G_TASK(result), error));
}

static void resourcesStreamReadCallback(GObject* object, GAsyncResult* result, gpointer userData)
{
    GRefPtr<GTask> task = adoptGRef(G_TASK(userData));

    GError* error = 0;
    g_output_stream_splice_finish(G_OUTPUT_STREAM(object), result, &error);
    if (error) {
        g_task_return_error(task.get(), error);
        return;
    }

    WebKitWebView* webView = WEBKIT_WEB_VIEW(g_task_get_source_object(task.get()));
    gpointer outputStreamData = g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(object));
    getPage(webView).runJavaScriptInMainFrame({ String::fromUTF8(reinterpret_cast<const gchar*>(outputStreamData)), URL { }, false, std::nullopt, true },
        [task] (auto&& result) {
            RefPtr<API::SerializedScriptValue> serializedScriptValue;
            ExceptionDetails exceptionDetails;
            if (result.has_value())
                serializedScriptValue = WTFMove(result.value());
            else
                exceptionDetails = WTFMove(result.error());
            webkitWebViewRunJavaScriptCallback(serializedScriptValue.get(), exceptionDetails, task.get());
        });
}

/**
 * webkit_web_view_run_javascript_from_gresource:
 * @web_view: a #WebKitWebView
 * @resource: the location of the resource to load
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the script finished
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously run the script from @resource.
 *
 * Asynchronously run the script from @resource in the context of the
 * current page in @web_view.
 *
 * When the operation is finished, @callback will be called. You can
 * then call webkit_web_view_run_javascript_from_gresource_finish() to get the result
 * of the operation.
 */
void webkit_web_view_run_javascript_from_gresource(WebKitWebView* webView, const gchar* resource, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(resource);

    GError* error = 0;
    GRefPtr<GInputStream> inputStream = adoptGRef(g_resources_open_stream(resource, G_RESOURCE_LOOKUP_FLAGS_NONE, &error));
    if (error) {
        g_task_report_error(webView, callback, userData, 0, error);
        return;
    }

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    GRefPtr<GOutputStream> outputStream = adoptGRef(g_memory_output_stream_new(0, 0, fastRealloc, fastFree));
    g_output_stream_splice_async(outputStream.get(), inputStream.get(),
        static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
        G_PRIORITY_DEFAULT, cancellable, resourcesStreamReadCallback, task);
}

/**
 * webkit_web_view_run_javascript_from_gresource_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_run_javascript_from_gresource().
 *
 * Check webkit_web_view_run_javascript_finish() for a usage example.
 *
 * Returns: (transfer full): a #WebKitJavascriptResult with the result of the last executed statement in @script
 *    or %NULL in case of error
 */
WebKitJavascriptResult* webkit_web_view_run_javascript_from_gresource_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(g_task_is_valid(result, webView), 0);

    return static_cast<WebKitJavascriptResult*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_web_view_get_main_resource:
 * @web_view: a #WebKitWebView
 *
 * Return the main resource of @web_view.
 *
 * Returns: (transfer none): the main #WebKitWebResource of the view
 *    or %NULL if nothing has been loaded.
 */
WebKitWebResource* webkit_web_view_get_main_resource(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    return webView->priv->mainResource.get();
}

#if PLATFORM(GTK)
/**
 * webkit_web_view_get_inspector:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitWebInspector associated to @web_view
 *
 * Returns: (transfer none): the #WebKitWebInspector of @web_view
 */
WebKitWebInspector* webkit_web_view_get_inspector(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);

    if (!webView->priv->inspector)
        webView->priv->inspector = adoptGRef(webkitWebInspectorCreate(getPage(webView).inspector()));

    return webView->priv->inspector.get();
}
#endif

/**
 * webkit_web_view_can_show_mime_type:
 * @web_view: a #WebKitWebView
 * @mime_type: a MIME type
 *
 * Whether or not a MIME type can be displayed in @web_view.
 *
 * Returns: %TRUE if the MIME type @mime_type can be displayed or %FALSE otherwise
 */
gboolean webkit_web_view_can_show_mime_type(WebKitWebView* webView, const char* mimeType)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(mimeType, FALSE);

    return getPage(webView).canShowMIMEType(String::fromUTF8(mimeType));
}

struct ViewSaveAsyncData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    RefPtr<API::Data> webData;
    GRefPtr<GFile> file;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(ViewSaveAsyncData)

static void fileReplaceContentsCallback(GObject* object, GAsyncResult* result, gpointer data)
{
    GRefPtr<GTask> task = adoptGRef(G_TASK(data));
    GError* error = 0;
    if (!g_file_replace_contents_finish(G_FILE(object), result, 0, &error)) {
        g_task_return_error(task.get(), error);
        return;
    }

    g_task_return_boolean(task.get(), TRUE);
}

static void getContentsAsMHTMLDataCallback(API::Data* wkData, GTask* taskPtr)
{
    auto task = adoptGRef(taskPtr);
    if (g_task_return_error_if_cancelled(task.get()))
        return;

    ViewSaveAsyncData* data = static_cast<ViewSaveAsyncData*>(g_task_get_task_data(task.get()));
    // We need to retain the data until the asyncronous process
    // initiated by the user has finished completely.
    data->webData = wkData;

    // If we are saving to a file we need to write the data on disk before finishing.
    if (g_task_get_source_tag(task.get()) == webkit_web_view_save_to_file) {
        ASSERT(G_IS_FILE(data->file.get()));
        GCancellable* cancellable = g_task_get_cancellable(task.get());
        g_file_replace_contents_async(data->file.get(), reinterpret_cast<const gchar*>(data->webData->bytes()), data->webData->size(),
            0, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, cancellable, fileReplaceContentsCallback, task.leakRef());
        return;
    }

    g_task_return_boolean(task.get(), TRUE);
}

/**
 * webkit_web_view_save:
 * @web_view: a #WebKitWebView
 * @save_mode: the #WebKitSaveMode specifying how the web page should be saved.
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously save the current web page.
 *
 * Asynchronously save the current web page associated to the
 * #WebKitWebView into a self-contained format using the mode
 * specified in @save_mode.
 *
 * When the operation is finished, @callback will be called. You can
 * then call webkit_web_view_save_finish() to get the result of the
 * operation.
 */
void webkit_web_view_save(WebKitWebView* webView, WebKitSaveMode saveMode, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    // We only support MHTML at the moment.
    g_return_if_fail(saveMode == WEBKIT_SAVE_MODE_MHTML);

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    g_task_set_source_tag(task, reinterpret_cast<gpointer>(webkit_web_view_save));
    g_task_set_task_data(task, createViewSaveAsyncData(), reinterpret_cast<GDestroyNotify>(destroyViewSaveAsyncData));
    getPage(webView).getContentsAsMHTMLData([task](API::Data* data) {
        getContentsAsMHTMLDataCallback(data, task);
    });
}

/**
 * webkit_web_view_save_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_save().
 *
 * Returns: (transfer full): a #GInputStream with the result of saving
 *    the current web page or %NULL in case of error.
 */
GInputStream* webkit_web_view_save_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(g_task_is_valid(result, webView), 0);

    GTask* task = G_TASK(result);
    if (!g_task_propagate_boolean(task, error))
        return 0;

    GInputStream* dataStream = g_memory_input_stream_new();
    ViewSaveAsyncData* data = static_cast<ViewSaveAsyncData*>(g_task_get_task_data(task));
    gsize length = data->webData->size();
    if (length)
        g_memory_input_stream_add_data(G_MEMORY_INPUT_STREAM(dataStream), fastMemDup(data->webData->bytes(), length), length, fastFree);

    return dataStream;
}

/**
 * webkit_web_view_save_to_file:
 * @web_view: a #WebKitWebView
 * @file: the #GFile where the current web page should be saved to.
 * @save_mode: the #WebKitSaveMode specifying how the web page should be saved.
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously save the current web page.
 *
 * Asynchronously save the current web page associated to the
 * #WebKitWebView into a self-contained format using the mode
 * specified in @save_mode and writing it to @file.
 *
 * When the operation is finished, @callback will be called. You can
 * then call webkit_web_view_save_to_file_finish() to get the result of the
 * operation.
 */
void webkit_web_view_save_to_file(WebKitWebView* webView, GFile* file, WebKitSaveMode saveMode, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(G_IS_FILE(file));

    // We only support MHTML at the moment.
    g_return_if_fail(saveMode == WEBKIT_SAVE_MODE_MHTML);

    GTask* task = g_task_new(webView, cancellable, callback, userData);
    g_task_set_source_tag(task, reinterpret_cast<gpointer>(webkit_web_view_save_to_file));
    ViewSaveAsyncData* data = createViewSaveAsyncData();
    data->file = file;
    g_task_set_task_data(task, data, reinterpret_cast<GDestroyNotify>(destroyViewSaveAsyncData));

    getPage(webView).getContentsAsMHTMLData([task](API::Data* data) {
        getContentsAsMHTMLDataCallback(data, task);
    });
}

/**
 * webkit_web_view_save_to_file_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_web_view_save_to_file().
 *
 * Returns: %TRUE if the web page was successfully saved to a file or %FALSE otherwise.
 */
gboolean webkit_web_view_save_to_file_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, webView), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * webkit_web_view_download_uri:
 * @web_view: a #WebKitWebView
 * @uri: the URI to download
 *
 * Requests downloading of the specified URI string for @web_view.
 *
 * Returns: (transfer full): a new #WebKitDownload representing
 *    the download operation.
 */
WebKitDownload* webkit_web_view_download_uri(WebKitWebView* webView, const char* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);
    g_return_val_if_fail(uri, nullptr);

    GRefPtr<WebKitDownload> download = webkitWebContextStartDownload(webView->priv->context.get(), uri, &getPage(webView));
    return download.leakRef();
}

/**
 * webkit_web_view_get_tls_info:
 * @web_view: a #WebKitWebView
 * @certificate: (out) (transfer none): return location for a #GTlsCertificate
 * @errors: (out): return location for a #GTlsCertificateFlags the verification status of @certificate
 *
 * Retrieves the #GTlsCertificate associated with the main resource of @web_view.
 *
 * Retrieves the #GTlsCertificate associated with the main resource of @web_view,
 * and the #GTlsCertificateFlags showing what problems, if any, have been found
 * with that certificate.
 * If the connection is not HTTPS, this function returns %FALSE.
 * This function should be called after a response has been received from the
 * server, so you can connect to #WebKitWebView::load-changed and call this function
 * when it's emitted with %WEBKIT_LOAD_COMMITTED event.
 *
 * Note that this function provides no information about the security of the web
 * page if the current #WebKitTLSErrorsPolicy is %WEBKIT_TLS_ERRORS_POLICY_IGNORE,
 * as subresources of the page may be controlled by an attacker. This function
 * may safely be used to determine the security status of the current page only
 * if the current #WebKitTLSErrorsPolicy is %WEBKIT_TLS_ERRORS_POLICY_FAIL, in
 * which case subresources that fail certificate verification will be blocked.
 *
 * Returns: %TRUE if the @web_view connection uses HTTPS and a response has been received
 *    from the server, or %FALSE otherwise.
 */
gboolean webkit_web_view_get_tls_info(WebKitWebView* webView, GTlsCertificate** certificate, GTlsCertificateFlags* errors)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    WebFrameProxy* mainFrame = getPage(webView).mainFrame();
    if (!mainFrame)
        return FALSE;

    const auto& certificateInfo = mainFrame->certificateInfo();
    if (certificate)
        *certificate = certificateInfo.certificate().get();
    if (errors)
        *errors = certificateInfo.tlsErrors();

    return !!certificateInfo.certificate();
}

#if PLATFORM(GTK)
/**
 * webkit_web_view_get_snapshot:
 * @web_view: a #WebKitWebView
 * @region: the #WebKitSnapshotRegion for this snapshot
 * @options: #WebKitSnapshotOptions for the snapshot
 * @cancellable: (allow-none): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data
 *
 * Asynchronously retrieves a snapshot of @web_view for @region.
 *
 * @options specifies how the snapshot should be rendered.
 *
 * When the operation is finished, @callback will be called. You must
 * call webkit_web_view_get_snapshot_finish() to get the result of the
 * operation.
 */
void webkit_web_view_get_snapshot(WebKitWebView* webView, WebKitSnapshotRegion region, WebKitSnapshotOptions options, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    SnapshotOptions snapshotOptions = 0;
    switch (region) {
    case WEBKIT_SNAPSHOT_REGION_VISIBLE:
        snapshotOptions |= SnapshotOptionsVisibleContentRect;
        break;
    case WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT:
        snapshotOptions |= SnapshotOptionsFullContentRect;
        break;
    }

    if (!(options & WEBKIT_SNAPSHOT_OPTIONS_INCLUDE_SELECTION_HIGHLIGHTING))
        snapshotOptions |= SnapshotOptionsExcludeSelectionHighlighting;
    if (options & WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND)
        snapshotOptions |= SnapshotOptionsTransparentBackground;

    GRefPtr<GTask> task = adoptGRef(g_task_new(webView, cancellable, callback, userData));
    getPage(webView).takeSnapshot({ }, { }, snapshotOptions, [task = WTFMove(task)](const ShareableBitmapHandle& handle) {
        if (!handle.isNull()) {
            if (auto bitmap = ShareableBitmap::create(handle, SharedMemory::Protection::ReadOnly)) {
                if (auto surface = bitmap->createCairoSurface()) {
                    g_task_return_pointer(task.get(), surface.leakRef(), reinterpret_cast<GDestroyNotify>(cairo_surface_destroy));
                    return;
                }
            }
        }
        g_task_return_new_error(task.get(), WEBKIT_SNAPSHOT_ERROR, WEBKIT_SNAPSHOT_ERROR_FAILED_TO_CREATE, _("There was an error creating the snapshot"));
    });
}

/**
 * webkit_web_view_get_snapshot_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finishes an asynchronous operation started with webkit_web_view_get_snapshot().
 *
 * Returns: (transfer full): a #cairo_surface_t with the retrieved snapshot or %NULL in error.
 */
cairo_surface_t* webkit_web_view_get_snapshot_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(g_task_is_valid(result, webView), 0);

    return static_cast<cairo_surface_t*>(g_task_propagate_pointer(G_TASK(result), error));
}
#endif

void webkitWebViewWebProcessTerminated(WebKitWebView* webView, WebKitWebProcessTerminationReason reason)
{
#if PLATFORM(GTK)
    if (reason == WEBKIT_WEB_PROCESS_CRASHED) {
        gboolean returnValue;
        g_signal_emit(webView, signals[WEB_PROCESS_CRASHED], 0, &returnValue);
    }
#endif
    g_signal_emit(webView, signals[WEB_PROCESS_TERMINATED], 0, reason);

    // Reset the state of the responsiveness property.
    webkitWebViewSetIsWebProcessResponsive(webView, true);
}

/**
 * webkit_web_view_is_editable:
 * @web_view: a #WebKitWebView
 *
 * Gets whether the user is allowed to edit the HTML document.
 *
 * When @web_view is not editable an element in the HTML document can only be edited if the
 * CONTENTEDITABLE attribute has been set on the element or one of its parent
 * elements. By default a #WebKitWebView is not editable.
 *
 * Returns: %TRUE if the user is allowed to edit the HTML document, or %FALSE otherwise.
 *
 * Since: 2.8
 */
gboolean webkit_web_view_is_editable(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return getPage(webView).isEditable();
}

/**
 * webkit_web_view_set_editable:
 * @web_view: a #WebKitWebView
 * @editable: a #gboolean indicating the editable state
 *
 * Sets whether the user is allowed to edit the HTML document.
 *
 * If @editable is %TRUE, @web_view allows the user to edit the HTML document. If
 * @editable is %FALSE, an element in @web_view's document can only be edited if the
 * CONTENTEDITABLE attribute has been set on the element or one of its parent
 * elements. By default a #WebKitWebView is not editable.
 *
 * Normally, a HTML document is not editable unless the elements within the
 * document are editable. This function provides a way to make the contents
 * of a #WebKitWebView editable without altering the document or DOM structure.
 *
 * Since: 2.8
 */
void webkit_web_view_set_editable(WebKitWebView* webView, gboolean editable)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    if (editable == getPage(webView).isEditable())
        return;

    getPage(webView).setEditable(editable);

    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_EDITABLE]);
}

/**
 * webkit_web_view_get_editor_state:
 * @web_view: a #WebKitWebView
 *
 * Gets the web editor state of @web_view.
 *
 * Returns: (transfer none): the #WebKitEditorState of the view
 *
 * Since: 2.10
 */
WebKitEditorState* webkit_web_view_get_editor_state(WebKitWebView *webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    if (!webView->priv->editorState)
        webView->priv->editorState = adoptGRef(webkitEditorStateCreate(getPage(webView)));

    return webView->priv->editorState.get();
}

/**
 * webkit_web_view_get_session_state:
 * @web_view: a #WebKitWebView
 *
 * Gets the current session state of @web_view
 *
 * Returns: (transfer full): a #WebKitWebViewSessionState
 *
 * Since: 2.12
 */
WebKitWebViewSessionState* webkit_web_view_get_session_state(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    SessionState sessionState = getPage(webView).sessionState(nullptr);
    return webkitWebViewSessionStateCreate(WTFMove(sessionState));
}

/**
 * webkit_web_view_restore_session_state:
 * @web_view: a #WebKitWebView
 * @state: a #WebKitWebViewSessionState
 *
 * Restore the @web_view session state from @state
 *
 * Since: 2.12
 */
void webkit_web_view_restore_session_state(WebKitWebView* webView, WebKitWebViewSessionState* state)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(state);

    getPage(webView).restoreFromSessionState(webkitWebViewSessionStateGetSessionState(state), false);
}

#if PLATFORM(WPE)
/**
 * webkit_web_view_add_frame_displayed_callback:
 * @web_view: a #WebKitWebView
 * @callback: a #WebKitFrameDisplayedCallback
 * @user_data: (closure): user data to pass to @callback
 * @destroy_notify: (nullable): destroy notifier for @user_data
 *
 * Add a callback to be called when the backend notifies that a frame has been displayed in @web_view.
 *
 * Returns: an identifier that should be passed to webkit_web_view_remove_frame_displayed_callback()
 *    to remove the callback.
 *
 * Since: 2.24
 */
unsigned webkit_web_view_add_frame_displayed_callback(WebKitWebView* webView, WebKitFrameDisplayedCallback callback, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), 0);
    g_return_val_if_fail(callback, 0);

    webView->priv->frameDisplayedCallbacks.append(FrameDisplayedCallback(callback, userData, destroyNotify));
    return webView->priv->frameDisplayedCallbacks.last().id;
}

/**
 * webkit_web_view_remove_frame_displayed_callback:
 * @web_view: a #WebKitWebView
 * @id: an identifier
 *
 * Removes a #WebKitFrameDisplayedCallback previously added to @web_view with
 * webkit_web_view_add_frame_displayed_callback().
 *
 * Since: 2.24
 */
void webkit_web_view_remove_frame_displayed_callback(WebKitWebView* webView, unsigned id)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(id);

    Function<bool(const FrameDisplayedCallback&)> matchFunction = [id](const auto& item) {
        return item.id == id;
    };

    if (webView->priv->inFrameDisplayed) {
        auto index = webView->priv->frameDisplayedCallbacks.findIf(matchFunction);
        if (index != notFound)
            webView->priv->frameDisplayedCallbacksToRemove.add(id);
    } else
        webView->priv->frameDisplayedCallbacks.removeFirstMatching(matchFunction);
}
#endif // PLATFORM(WPE)

/**
 * webkit_web_view_send_message_to_page:
 * @web_view: a #WebKitWebView
 * @message: a #WebKitUserMessage
 * @cancellable: (nullable): a #GCancellable or %NULL to ignore
 * @callback: (scope async): (nullable): A #GAsyncReadyCallback to call when the request is satisfied or %NULL
 * @user_data: (closure): the data to pass to callback function
 *
 * Send @message to the #WebKitWebPage corresponding to @web_view.
 *
 * If @message is floating, it's consumed.
 * If you don't expect any reply, or you simply want to ignore it, you can pass %NULL as @callback.
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_view_send_message_to_page_finish() to get the message reply.
 *
 * Since: 2.28
 */
void webkit_web_view_send_message_to_page(WebKitWebView* webView, WebKitUserMessage* message, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(WEBKIT_IS_USER_MESSAGE(message));

    // We sink the reference in case of being floating.
    GRefPtr<WebKitUserMessage> adoptedMessage = message;
    auto& page = getPage(webView);
    if (!callback) {
        page.ensureRunningProcess().send(Messages::WebPage::SendMessageToWebExtension(webkitUserMessageGetMessage(message)), page.webPageID().toUInt64());
        return;
    }

    GRefPtr<GTask> task = adoptGRef(g_task_new(webView, cancellable, callback, userData));
    CompletionHandler<void(UserMessage&&)> completionHandler = [task = WTFMove(task)](UserMessage&& replyMessage) {
        switch (replyMessage.type) {
        case UserMessage::Type::Null:
            g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled"));
            break;
        case UserMessage::Type::Message:
            g_task_return_pointer(task.get(), g_object_ref_sink(webkitUserMessageCreate(WTFMove(replyMessage))), static_cast<GDestroyNotify>(g_object_unref));
            break;
        case UserMessage::Type::Error:
            g_task_return_new_error(task.get(), WEBKIT_USER_MESSAGE_ERROR, replyMessage.errorCode, _("Message %s was not handled"), replyMessage.name.data());
            break;
        }
    };
    page.ensureRunningProcess().sendWithAsyncReply(Messages::WebPage::SendMessageToWebExtensionWithReply(webkitUserMessageGetMessage(message)),
        WTFMove(completionHandler), page.webPageID().toUInt64());
}

/**
 * webkit_web_view_send_message_to_page_finish:
 * @web_view: a #WebKitWebView
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignor
 *
 * Finish an asynchronous operation started with webkit_web_view_send_message_to_page().
 *
 * Returns: (transfer full): a #WebKitUserMessage with the reply or %NULL in case of error.
 *
 * Since: 2.28
 */
WebKitUserMessage* webkit_web_view_send_message_to_page_finish(WebKitWebView* webView, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, webView), nullptr);

    return WEBKIT_USER_MESSAGE(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_web_view_set_input_method_context:
 * @web_view: a #WebKitWebView
 * @context: (nullable): the #WebKitInputMethodContext to set, or %NULL
 *
 * Set the #WebKitInputMethodContext to be used by @web_view.
 *
 * Set the #WebKitInputMethodContext to be used by @web_view, or %NULL to not use any input method.
 * Note that the same #WebKitInputMethodContext can't be set on more than one #WebKitWebView at the same time.
 *
 * Since: 2.28
 */
void webkit_web_view_set_input_method_context(WebKitWebView* webView, WebKitInputMethodContext* context)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));
    g_return_if_fail(!context || WEBKIT_IS_INPUT_METHOD_CONTEXT(context));

    if (context) {
        if (auto* currentWebView = webkitInputMethodContextGetWebView(context)) {
            if (currentWebView != webView) {
                g_warning("Trying to set a WebKitInputMethodContext to a WebKitWebView, but the WebKitInputMethodContext "
                    "was already set to a different WebKitWebView. It's not possible to use a WebKitInputMethodContext "
                    "with more than one WebKitWebView at the same time.");
            }
            return;
        }
        webkitInputMethodContextSetWebView(context, webView);
    }
#if PLATFORM(GTK)
    webkitWebViewBaseSetInputMethodContext(WEBKIT_WEB_VIEW_BASE(webView), context);
#elif PLATFORM(WPE)
    webView->priv->view->setInputMethodContext(context);
#endif
}

/**
 * webkit_web_view_get_input_method_context:
 * @web_view: a #WebKitWebView
 *
 * Get the #WebKitInputMethodContext currently in use by @web_view.
 *
 * Get the #WebKitInputMethodContext currently in use by @web_view, or %NULL if no input method is being used.
 *
 * Returns: (nullable) (transfer none): a #WebKitInputMethodContext, or %NULL
 *
 * Since: 2.28
 */
WebKitInputMethodContext* webkit_web_view_get_input_method_context(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

#if PLATFORM(GTK)
    return webkitWebViewBaseGetInputMethodContext(WEBKIT_WEB_VIEW_BASE(webView));
#elif PLATFORM(WPE)
    return webView->priv->view->inputMethodContext();
#endif

}

/**
 * webkit_web_view_get_website_policies:
 * @web_view: a #WebKitWebView
 *
 * Gets the default website policies.
 *
 * Gets the default website policies set on construction in the
 * @web_view. These can be overridden on a per-origin basis via the
 * #WebKitWebView::decide-policy signal handler.
 *
 * See also webkit_policy_decision_use_with_policies().
 *
 * Returns: (transfer none): the default #WebKitWebsitePolicies
 *     associated with the view.
 *
 * Since: 2.30
 */
WebKitWebsitePolicies* webkit_web_view_get_website_policies(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    return webView->priv->websitePolicies.get();
}

void webkitWebViewSetIsWebProcessResponsive(WebKitWebView* webView, bool isResponsive)
{
    if (webView->priv->isWebProcessResponsive == isResponsive)
        return;

    webView->priv->isWebProcessResponsive = isResponsive;
    g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_IS_WEB_PROCESS_RESPONSIVE]);
}

/**
 * webkit_web_view_get_is_web_process_responsive:
 * @web_view: a #WebKitWebView
 *
 * Get whether the current web process of a #WebKitWebView is responsive.
 *
 * Returns: %TRUE if the web process attached to @web_view is responsive, or %FALSE otherwise.
 *
 * Since: 2.34
 */
gboolean webkit_web_view_get_is_web_process_responsive(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), FALSE);

    return webView->priv->isWebProcessResponsive;
}

/**
 * webkit_web_view_terminate_web_process:
 * @web_view: a #WebKitWebView
 *
 * Terminates the web process associated to @web_view.
 *
 * When the web process gets terminated
 * using this method, the #WebKitWebView::web-process-terminated signal is emitted with
 * %WEBKIT_WEB_PROCESS_TERMINATED_BY_API as the reason for termination.
 *
 * Since: 2.34
 */
void webkit_web_view_terminate_web_process(WebKitWebView* webView)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    auto& page = getPage(webView);

    Ref<WebKit::WebProcessProxy> protectedProcessProxy(page.process());
    protectedProcessProxy->requestTermination(WebKit::ProcessTerminationReason::RequestedByClient);

    if (auto* provisionalPageProxy = page.provisionalPageProxy()) {
        Ref<WebKit::WebProcessProxy> protectedProcessProxy(provisionalPageProxy->process());
        protectedProcessProxy->requestTermination(WebKit::ProcessTerminationReason::RequestedByClient);
    }
}

/**
 * webkit_web_view_set_cors_allowlist:
 * @web_view: a #WebKitWebView
 * @allowlist: (array zero-terminated=1) (element-type utf8) (transfer none) (nullable): an allowlist of URI patterns, or %NULL
 *
 * Sets the @allowlist for CORS.
 *
 * Sets the @allowlist for which
 * [Cross-Origin Resource Sharing](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS)
 * checks are disabled in @web_view. URI patterns must be of the form
 * `[protocol]://[host]/[path]`, each component may contain the wildcard
 * character (`*`) to represent zero or more other characters. All three
 * components are required and must not be omitted from the URI
 * patterns.
 *
 * Disabling CORS checks permits resources from other origins to load
 * allowlisted resources. It does not permit the allowlisted resources
 * to load resources from other origins.
 *
 * If this function is called multiple times, only the allowlist set by
 * the most recent call will be effective.
 *
 * Since: 2.34
 */
void webkit_web_view_set_cors_allowlist(WebKitWebView* webView, const gchar* const* allowList)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    Vector<String> allowListVector;
    if (allowList) {
        for (auto str = allowList; *str; ++str)
            allowListVector.append(String::fromUTF8(*str));
    }

    getPage(webView).setCORSDisablingPatterns(WTFMove(allowListVector));
}

static void webkitWebViewConfigureMediaCapture(WebKitWebView* webView, WebCore::MediaProducerMediaCaptureKind captureKind, WebKitMediaCaptureState captureState)
{
    auto& page = getPage(webView);
    auto mutedState = page.mutedStateFlags();

    switch (captureState) {
    case WEBKIT_MEDIA_CAPTURE_STATE_NONE:
        page.stopMediaCapture(captureKind, [webView, captureKind] {
            switch (captureKind) {
            case WebCore::MediaProducerMediaCaptureKind::Microphone:
                g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_MICROPHONE_CAPTURE_STATE]);
                break;
            case WebCore::MediaProducerMediaCaptureKind::Display:
                break;
            case WebCore::MediaProducerMediaCaptureKind::Camera:
                g_object_notify_by_pspec(G_OBJECT(webView), sObjProperties[PROP_CAMERA_CAPTURE_STATE]);
                break;
            case WebCore::MediaProducerMediaCaptureKind::SystemAudio:
            case WebCore::MediaProducerMediaCaptureKind::EveryKind:
                ASSERT_NOT_REACHED();
                return;
            }
        });
        break;
    case WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE:
        switch (captureKind) {
        case WebCore::MediaProducerMediaCaptureKind::Microphone:
            mutedState.remove(WebCore::MediaProducer::MutedState::AudioCaptureIsMuted);
            break;
        case WebCore::MediaProducerMediaCaptureKind::Camera:
            mutedState.remove(WebCore::MediaProducer::MutedState::VideoCaptureIsMuted);
            break;
        case WebCore::MediaProducerMediaCaptureKind::Display:
            mutedState.remove(WebCore::MediaProducer::MutedState::ScreenCaptureIsMuted);
            break;
        case WebCore::MediaProducerMediaCaptureKind::SystemAudio:
        case WebCore::MediaProducerMediaCaptureKind::EveryKind:
            ASSERT_NOT_REACHED();
            return;
        }
        page.setMuted(mutedState);
        break;
    case WEBKIT_MEDIA_CAPTURE_STATE_MUTED:
        switch (captureKind) {
        case WebCore::MediaProducerMediaCaptureKind::Microphone:
            mutedState.add(WebCore::MediaProducer::MutedState::AudioCaptureIsMuted);
            break;
        case WebCore::MediaProducerMediaCaptureKind::Camera:
            mutedState.add(WebCore::MediaProducer::MutedState::VideoCaptureIsMuted);
            break;
        case WebCore::MediaProducerMediaCaptureKind::Display:
            mutedState.add(WebCore::MediaProducer::MutedState::ScreenCaptureIsMuted);
            break;
        case WebCore::MediaProducerMediaCaptureKind::SystemAudio:
        case WebCore::MediaProducerMediaCaptureKind::EveryKind:
            ASSERT_NOT_REACHED();
            return;
        }
        page.setMuted(mutedState);
        break;
    }
}

/**
 * webkit_web_view_get_camera_capture_state:
 * @web_view: a #WebKitWebView
 *
 * Get the camera capture state of a #WebKitWebView.
 *
 * Returns: The #WebKitMediaCaptureState of the camera device. If #WebKitSettings:enable-mediastream
 * is %FALSE, this method will return %WEBKIT_MEDIA_CAPTURE_STATE_NONE.
 *
 * Since: 2.34
 */
WebKitMediaCaptureState webkit_web_view_get_camera_capture_state(WebKitWebView* webView)
{
    auto state = getPage(webView).reportedMediaState();
    if (state & WebCore::MediaProducer::MediaState::HasActiveVideoCaptureDevice)
        return WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE;
    if (state & WebCore::MediaProducer::MediaState::HasMutedVideoCaptureDevice)
        return WEBKIT_MEDIA_CAPTURE_STATE_MUTED;
    return WEBKIT_MEDIA_CAPTURE_STATE_NONE;
}

/**
 * webkit_web_view_set_camera_capture_state:
 * @web_view: a #WebKitWebView
 * @state: a #WebKitMediaCaptureState
 *
 * Set the camera capture state of a #WebKitWebView.
 *
 * If #WebKitSettings:enable-mediastream is %FALSE, this method will have no visible effect. Once the
 * state of the device has been set to %WEBKIT_MEDIA_CAPTURE_STATE_NONE it cannot be changed
 * anymore. The page can however request capture again using the mediaDevices API.
 *
 * Since: 2.34
 */
void webkit_web_view_set_camera_capture_state(WebKitWebView* webView, WebKitMediaCaptureState state)
{
    if (webkit_web_view_get_camera_capture_state(webView) == WEBKIT_MEDIA_CAPTURE_STATE_NONE)
        return;

    webkitWebViewConfigureMediaCapture(webView, WebCore::MediaProducerMediaCaptureKind::Camera, state);
}

/**
 * webkit_web_view_get_microphone_capture_state:
 * @web_view: a #WebKitWebView
 *
 * Get the microphone capture state of a #WebKitWebView.
 *
 * Returns: The #WebKitMediaCaptureState of the microphone device. If #WebKitSettings:enable-mediastream
 * is %FALSE, this method will return %WEBKIT_MEDIA_CAPTURE_STATE_NONE.
 *
 * Since: 2.34
 */
WebKitMediaCaptureState webkit_web_view_get_microphone_capture_state(WebKitWebView* webView)
{
    auto state = getPage(webView).reportedMediaState();
    if (state & WebCore::MediaProducer::MediaState::HasActiveAudioCaptureDevice)
        return WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE;
    if (state & WebCore::MediaProducer::MediaState::HasMutedAudioCaptureDevice)
        return WEBKIT_MEDIA_CAPTURE_STATE_MUTED;
    return WEBKIT_MEDIA_CAPTURE_STATE_NONE;
}

/**
 * webkit_web_view_set_microphone_capture_state:
 * @web_view: a #WebKitWebView
 * @state: a #WebKitMediaCaptureState
 *
 * Set the microphone capture state of a #WebKitWebView.
 *
 * If #WebKitSettings:enable-mediastream is %FALSE, this method will have no visible effect. Once the
 * state of the device has been set to %WEBKIT_MEDIA_CAPTURE_STATE_NONE it cannot be changed
 * anymore. The page can however request capture again using the mediaDevices API.
 *
 * Since: 2.34
 */
void webkit_web_view_set_microphone_capture_state(WebKitWebView* webView, WebKitMediaCaptureState state)
{
    if (webkit_web_view_get_microphone_capture_state(webView) == WEBKIT_MEDIA_CAPTURE_STATE_NONE)
        return;

    webkitWebViewConfigureMediaCapture(webView, WebCore::MediaProducerMediaCaptureKind::Microphone, state);
}

/**
 * webkit_web_view_get_display_capture_state:
 * @web_view: a #WebKitWebView
 *
 * Get the display capture state of a #WebKitWebView.
 *
 * Returns: The #WebKitMediaCaptureState of the display device. If #WebKitSettings:enable-mediastream
 * is %FALSE, this method will return %WEBKIT_MEDIA_CAPTURE_STATE_NONE.
 *
 * Since: 2.34
 */
WebKitMediaCaptureState webkit_web_view_get_display_capture_state(WebKitWebView* webView)
{
    auto state = getPage(webView).reportedMediaState();
    if (state & WebCore::MediaProducer::MediaState::HasActiveScreenCaptureDevice)
        return WEBKIT_MEDIA_CAPTURE_STATE_ACTIVE;
    if (state & WebCore::MediaProducer::MediaState::HasMutedScreenCaptureDevice)
        return WEBKIT_MEDIA_CAPTURE_STATE_MUTED;
    return WEBKIT_MEDIA_CAPTURE_STATE_NONE;
}

/**
 * webkit_web_view_set_display_capture_state:
 * @web_view: a #WebKitWebView
 * @state: a #WebKitMediaCaptureState
 *
 * Set the display capture state of a #WebKitWebView.
 *
 * If #WebKitSettings:enable-mediastream is %FALSE, this method will have no visible effect. Once the
 * state of the device has been set to %WEBKIT_MEDIA_CAPTURE_STATE_NONE it cannot be changed
 * anymore. The page can however request capture again using the mediaDevices API.
 *
 * Since: 2.34
 */
void webkit_web_view_set_display_capture_state(WebKitWebView* webView, WebKitMediaCaptureState state)
{
    if (webkit_web_view_get_display_capture_state(webView) == WEBKIT_MEDIA_CAPTURE_STATE_NONE)
        return;

    webkitWebViewConfigureMediaCapture(webView, WebCore::MediaProducerMediaCaptureKind::Display, state);
}

void webkitWebViewForceRepaintForTesting(WebKitWebView* webView, ForceRepaintCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_VIEW(webView));

    getPage(webView).forceRepaint([callback, userData]() {
        callback(userData);
    });
}

void webkitSetCachedProcessSuspensionDelayForTesting(double seconds)
{
    WebKit::WebsiteDataStore::setCachedProcessSuspensionDelayForTesting(Seconds(seconds));
}

/**
 * webkit_web_view_get_web_extension_mode:
 * @web_view: a #WebKitWebView
 *
 * Get the view's #WebKitWebExtensionMode.
 *
 * Returns: the #WebKitWebExtensionMode
 *
 * Since: 2.38
 */
WebKitWebExtensionMode webkit_web_view_get_web_extension_mode(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), WEBKIT_WEB_EXTENSION_MODE_NONE);

    return webView->priv->webExtensionMode;
}

/**
 * webkit_web_view_get_default_content_security_policy:
 * @web_view: a #WebKitWebView
 *
 * Gets the configured default Content-Security-Policy.
 *
 * Returns: (nullable): The default policy or %NULL
 *
 * Since: 2.38
 */
const gchar*
webkit_web_view_get_default_content_security_policy(WebKitWebView* webView)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_VIEW(webView), nullptr);

    if (webView->priv->defaultContentSecurityPolicy.isNull())
        return nullptr;

    return webView->priv->defaultContentSecurityPolicy.data();
}
