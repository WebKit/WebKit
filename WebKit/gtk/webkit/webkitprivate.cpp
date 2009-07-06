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

#include "webkitsoupauthdialog.h"
#include "webkitprivate.h"
#include "ChromeClientGtk.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGtk.h"
#include <libintl.h>
#include "Logging.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "Pasteboard.h"
#include "PasteboardHelperGtk.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include <runtime/InitializeThreading.h>

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
    return (WebCore::NavigationType)type;
}

WebCore::ResourceRequest core(WebKitNetworkRequest* request)
{
    SoupMessage* soupMessage = webkit_network_request_get_message(request);
    if (soupMessage)
        return ResourceRequest(soupMessage);

    KURL url = KURL(KURL(), String::fromUTF8(webkit_network_request_get_uri(request)));
    return ResourceRequest(url);
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

    WebCore::Frame* frame = d->m_frame;
    if (!frame)
        return NULL;

    GtkWidget* toplevel =  gtk_widget_get_toplevel(GTK_WIDGET(frame->page()->chrome()->platformWindow()));
    if (GTK_WIDGET_TOPLEVEL(toplevel))
        return toplevel;
    else
        return NULL;
}

void webkit_init()
{
    static bool isInitialized = false;
    if (isInitialized)
        return;
    isInitialized = true;

    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    JSC::initializeThreading();
    WebCore::InitializeLoggingChannelsIfNecessary();

    // Page cache capacity (in pages). Comment from Mac port:
    // (Research indicates that value / page drops substantially after 3 pages.)
    // FIXME: Expose this with an API and/or calculate based on available resources
    WebCore::pageCache()->setCapacity(3);

#if ENABLE(DATABASE)
    // FIXME: It should be possible for client applications to override this default location
    gchar* databaseDirectory = g_build_filename(g_get_user_data_dir(), "webkit", "databases", NULL);
    WebCore::DatabaseTracker::tracker().setDatabaseDirectoryPath(databaseDirectory);
    g_free(databaseDirectory);
#endif

    PageGroup::setShouldTrackVisitedLinks(true);

    Pasteboard::generalPasteboard()->setHelper(new WebKit::PasteboardHelperGtk());

    SoupSession* session = webkit_get_default_session();

    SoupSessionFeature* authDialog = static_cast<SoupSessionFeature*>(g_object_new(WEBKIT_TYPE_SOUP_AUTH_DIALOG, NULL));
    g_signal_connect(authDialog, "current-toplevel", G_CALLBACK(currentToplevelCallback), NULL);
    soup_session_add_feature(session, authDialog);
    g_object_unref(authDialog);

    SoupSessionFeature* sniffer = static_cast<SoupSessionFeature*>(g_object_new(SOUP_TYPE_CONTENT_SNIFFER, NULL));
    soup_session_add_feature(session, sniffer);
    g_object_unref(sniffer);
}
