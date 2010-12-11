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
#include "TextEncodingRegistry.h"
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

WebCore::EditingBehaviorType core(WebKitEditingBehavior type)
{
    return (WebCore::EditingBehaviorType)type;
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
