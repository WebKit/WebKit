/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008 Collabora Ltd.
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
#include "webkitprivate.h"

#include "ApplicationCacheStorage.h"
#include "Chrome.h"
#include "ChromeClientGtk.h"
#include "Element.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGtk.h"
#include "FrameNetworkingContextGtk.h"
#include "GtkVersioning.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "IconDatabase.h"
#include "Logging.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "Pasteboard.h"
#include "PasteboardHelperGtk.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "TextEncodingRegistry.h"
#include "WebKitDOMBinding.h"
#include "webkitnetworkresponse.h"
#include "webkitsoupauthdialog.h"
#include "webkitversion.h"
#include <libintl.h>
#include <runtime/InitializeThreading.h>
#include <stdlib.h>
#include <wtf/Threading.h>

#if ENABLE(VIDEO)
#include "FullscreenVideoController.h"
#endif

#if ENABLE(DATABASE)
#include "DatabaseTracker.h"
#endif

using namespace WebCore;

namespace WebKit {

WebKitWebView* getViewFromFrame(WebKitWebFrame* frame)
{
    WebKitWebFramePrivate* priv = frame->priv;
    return priv->webView;
}

WebCore::Frame* core(WebKitWebFrame* frame)
{
    if (!frame)
        return 0;

    WebKitWebFramePrivate* priv = frame->priv;
    return priv ? priv->coreFrame : 0;
}

WebKitWebFrame* kit(WebCore::Frame* coreFrame)
{
    if (!coreFrame)
        return 0;

    ASSERT(coreFrame->loader());
    WebKit::FrameLoaderClient* client = static_cast<WebKit::FrameLoaderClient*>(coreFrame->loader()->client());
    return client ? client->webFrame() : 0;
}

WebCore::Page* core(WebKitWebView* webView)
{
    if (!webView)
        return 0;

    WebKitWebViewPrivate* priv = webView->priv;
    return priv ? priv->corePage : 0;
}

WebKitWebView* kit(WebCore::Page* corePage)
{
    if (!corePage)
        return 0;

    ASSERT(corePage->chrome());
    WebKit::ChromeClient* client = static_cast<WebKit::ChromeClient*>(corePage->chrome()->client());
    return client ? client->webView() : 0;
}

WebKitWebNavigationReason kit(WebCore::NavigationType type)
{
    return (WebKitWebNavigationReason)type;
}

WebCore::NavigationType core(WebKitWebNavigationReason type)
{
    return static_cast<WebCore::NavigationType>(type);
}

WebCore::ResourceRequest core(WebKitNetworkRequest* request)
{
    SoupMessage* soupMessage = webkit_network_request_get_message(request);
    if (soupMessage)
        return ResourceRequest(soupMessage);

    KURL url = KURL(KURL(), String::fromUTF8(webkit_network_request_get_uri(request)));
    return ResourceRequest(url);
}

WebCore::ResourceResponse core(WebKitNetworkResponse* response)
{
    SoupMessage* soupMessage = webkit_network_response_get_message(response);
    if (soupMessage)
        return ResourceResponse(soupMessage);

    return ResourceResponse();
}

WebCore::EditingBehaviorType core(WebKitEditingBehavior type)
{
    return (WebCore::EditingBehaviorType)type;
}

WebKitHitTestResult* kit(const WebCore::HitTestResult& result)
{
    guint context = WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT;
    GOwnPtr<char> linkURI(0);
    GOwnPtr<char> imageURI(0);
    GOwnPtr<char> mediaURI(0);
    WebKitDOMNode* node = 0;

    if (!result.absoluteLinkURL().isEmpty()) {
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK;
        linkURI.set(g_strdup(result.absoluteLinkURL().string().utf8().data()));
    }

    if (!result.absoluteImageURL().isEmpty()) {
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
        imageURI.set(g_strdup(result.absoluteImageURL().string().utf8().data()));
    }

    if (!result.absoluteMediaURL().isEmpty()) {
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA;
        mediaURI.set(g_strdup(result.absoluteMediaURL().string().utf8().data()));
    }

    if (result.isSelected())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION;

    if (result.isContentEditable())
        context |= WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;

    if (result.innerNonSharedNode())
        node = kit(result.innerNonSharedNode());

    return WEBKIT_HIT_TEST_RESULT(g_object_new(WEBKIT_TYPE_HIT_TEST_RESULT,
                                               "link-uri", linkURI.get(),
                                               "image-uri", imageURI.get(),
                                               "media-uri", mediaURI.get(),
                                               "context", context,
                                               "inner-node", node,
                                               NULL));
}

PasteboardHelperGtk* pasteboardHelperInstance()
{
    static PasteboardHelperGtk* helper = new PasteboardHelperGtk();
    return helper;
}

} /** end namespace WebKit */

static GtkWidget* currentToplevelCallback(WebKitSoupAuthDialog* feature, SoupMessage* message, gpointer userData)
{
    gpointer messageData = g_object_get_data(G_OBJECT(message), "resourceHandle");
    if (!messageData)
        return NULL;

    ResourceHandle* handle = static_cast<ResourceHandle*>(messageData);
    if (!handle)
        return NULL;

    ResourceHandleInternal* d = handle->getInternal();
    if (!d)
        return NULL;

    WebKit::FrameNetworkingContextGtk* context = static_cast<WebKit::FrameNetworkingContextGtk*>(d->m_context.get());
    if (!context)
        return NULL;

    if (!context->coreFrame())
        return NULL;

    GtkWidget* toplevel =  gtk_widget_get_toplevel(GTK_WIDGET(context->coreFrame()->page()->chrome()->platformPageClient()));
    if (gtk_widget_is_toplevel(toplevel))
        return toplevel;
    else
        return NULL;
}

static void closeIconDatabaseOnExit()
{
    iconDatabase()->close();
}

#ifdef HAVE_GSETTINGS
static bool isSchemaAvailable(const char* schemaID)
{
    const char* const* availableSchemas = g_settings_list_schemas();
    char* const* iter = const_cast<char* const*>(availableSchemas);

    while (*iter) {
        if (g_str_equal(schemaID, *iter))
            return true;
        iter++;
    }

    return false;
}

GSettings* inspectorGSettings()
{
    static GSettings* settings = 0;

    if (settings)
        return settings;

    const gchar* schemaID = "org.webkitgtk-"WEBKITGTK_API_VERSION_STRING".inspector";

    // Unfortunately GSettings will abort the process execution if the
    // schema is not installed, which is the case for when running
    // tests, or even the introspection dump at build time, so check
    // if we have the schema before trying to initialize it.
    if (!isSchemaAvailable(schemaID)) {
        g_warning("GSettings schema not found - settings will not be used or saved.");
        return 0;
    }

    settings = g_settings_new(schemaID);

    return settings;
}
#endif

void webkit_init()
{
    static bool isInitialized = false;
    if (isInitialized)
        return;
    isInitialized = true;

    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    JSC::initializeThreading();
    WTF::initializeMainThread();

    WebCore::InitializeLoggingChannelsIfNecessary();

    // We make sure the text codecs have been initialized, because
    // that may only be done by the main thread.
    atomicCanonicalTextEncodingName("UTF-8");

    // Page cache capacity (in pages). Comment from Mac port:
    // (Research indicates that value / page drops substantially after 3 pages.)
    // FIXME: Expose this with an API and/or calculate based on available resources
    webkit_set_cache_model(WEBKIT_CACHE_MODEL_WEB_BROWSER);

#if ENABLE(DATABASE)
    gchar* databaseDirectory = g_build_filename(g_get_user_data_dir(), "webkit", "databases", NULL);
    webkit_set_web_database_directory_path(databaseDirectory);

    // FIXME: It should be possible for client applications to override the default appcache location
    WebCore::cacheStorage().setCacheDirectory(databaseDirectory);
    g_free(databaseDirectory);
#endif

    PageGroup::setShouldTrackVisitedLinks(true);

    Pasteboard::generalPasteboard()->setHelper(WebKit::pasteboardHelperInstance());

    iconDatabase()->setEnabled(true);

    GOwnPtr<gchar> iconDatabasePath(g_build_filename(g_get_user_data_dir(), "webkit", "icondatabase", NULL));
    iconDatabase()->open(iconDatabasePath.get());

    atexit(closeIconDatabaseOnExit);

    SoupSession* session = webkit_get_default_session();

    SoupSessionFeature* authDialog = static_cast<SoupSessionFeature*>(g_object_new(WEBKIT_TYPE_SOUP_AUTH_DIALOG, NULL));
    g_signal_connect(authDialog, "current-toplevel", G_CALLBACK(currentToplevelCallback), NULL);
    soup_session_add_feature(session, authDialog);
    g_object_unref(authDialog);

    SoupSessionFeature* sniffer = static_cast<SoupSessionFeature*>(g_object_new(SOUP_TYPE_CONTENT_SNIFFER, NULL));
    soup_session_add_feature(session, sniffer);
    g_object_unref(sniffer);

    soup_session_add_feature_by_type(session, SOUP_TYPE_CONTENT_DECODER);
}

void webkit_white_list_access_from_origin(const gchar* sourceOrigin, const gchar* destinationProtocol, const gchar* destinationHost, bool allowDestinationSubdomains)
{
    SecurityOrigin::addOriginAccessWhitelistEntry(*SecurityOrigin::createFromString(sourceOrigin), destinationProtocol, destinationHost, allowDestinationSubdomains);
}

void webkit_reset_origin_access_white_lists()
{
    SecurityOrigin::resetOriginAccessWhitelists();
}


void webkitWebViewEnterFullscreen(WebKitWebView* webView, Node* node)
{
    if (!node->hasTagName(HTMLNames::videoTag))
        return;

#if ENABLE(VIDEO)
    HTMLMediaElement* videoElement = static_cast<HTMLMediaElement*>(node);
    WebKitWebViewPrivate* priv = webView->priv;

    // First exit Fullscreen for the old mediaElement.
    if (priv->fullscreenVideoController)
        priv->fullscreenVideoController->exitFullscreen();

    priv->fullscreenVideoController = new FullscreenVideoController;
    priv->fullscreenVideoController->setMediaElement(videoElement);
    priv->fullscreenVideoController->enterFullscreen();
#endif
}

void webkitWebViewExitFullscreen(WebKitWebView* webView)
{
#if ENABLE(VIDEO)
    WebKitWebViewPrivate* priv = webView->priv;
    if (priv->fullscreenVideoController)
        priv->fullscreenVideoController->exitFullscreen();
#endif
}

