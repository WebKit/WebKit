/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * Copyright (C) 2008, 2010 Collabora Ltd.
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
#include "webkitglobals.h"

#include "ApplicationCacheStorage.h"
#include "Chrome.h"
#include "FrameNetworkingContextGtk.h"
#include "GOwnPtr.h"
#include "GRefPtr.h"
#include "IconDatabase.h"
#include "Logging.h"
#include "MemoryCache.h"
#include "Page.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "TextEncodingRegistry.h"
#include "Pasteboard.h"
#include "PasteboardHelperGtk.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceResponse.h"
#include "webkitapplicationcache.h"
#include "webkitglobalsprivate.h"
#include "webkiticondatabase.h"
#include "webkitsoupauthdialog.h"
#include "webkitspellchecker.h"
#include "webkitspellcheckerenchant.h"
#include "webkitwebdatabase.h"
#include "webkitwebplugindatabaseprivate.h"
#include <libintl.h>
#include <runtime/InitializeThreading.h>
#include <stdlib.h>
#include <wtf/MainThread.h>

static WebKitCacheModel cacheModel = WEBKIT_CACHE_MODEL_DEFAULT;

using namespace WebCore;

/**
 * SECTION:webkit
 * @short_description: Global functions controlling WebKit
 *
 * WebKit manages many resources which are not related to specific
 * views. These functions relate to cross-view limits, such as cache
 * sizes, database quotas, and the HTTP session management.
 */

/**
 * webkit_get_default_session:
 *
 * Retrieves the default #SoupSession used by all web views.
 * Note that the session features are added by WebKit on demand,
 * so if you insert your own #SoupCookieJar before any network
 * traffic occurs, WebKit will use it instead of the default.
 *
 * Return value: (transfer none): the default #SoupSession
 *
 * Since: 1.1.1
 */
SoupSession* webkit_get_default_session ()
{
    webkitInit();
    return ResourceHandle::defaultSession();
}

/**
 * webkit_set_cache_model:
 * @cache_model: a #WebKitCacheModel
 *
 * Specifies a usage model for WebViews, which WebKit will use to
 * determine its caching behavior. All web views follow the cache
 * model. This cache model determines the RAM and disk space to use
 * for caching previously viewed content .
 *
 * Research indicates that users tend to browse within clusters of
 * documents that hold resources in common, and to revisit previously
 * visited documents. WebKit and the frameworks below it include
 * built-in caches that take advantage of these patterns,
 * substantially improving document load speed in browsing
 * situations. The WebKit cache model controls the behaviors of all of
 * these caches, including various WebCore caches.
 *
 * Browsers can improve document load speed substantially by
 * specifying WEBKIT_CACHE_MODEL_WEB_BROWSER. Applications without a
 * browsing interface can reduce memory usage substantially by
 * specifying WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER. Default value is
 * WEBKIT_CACHE_MODEL_WEB_BROWSER.
 *
 * Since: 1.1.18
 */
void webkit_set_cache_model(WebKitCacheModel model)
{
    webkitInit();

    if (cacheModel == model)
        return;

    // FIXME: Add disk cache handling when soup has the API
    guint cacheTotalCapacity;
    guint cacheMinDeadCapacity;
    guint cacheMaxDeadCapacity;
    gdouble deadDecodedDataDeletionInterval;
    guint pageCacheCapacity;

    // FIXME: The Mac port calculates these values based on the amount of physical memory that's
    // installed on the system. Currently these values match the Mac port for users with more than
    // 512 MB and less than 1024 MB of physical memory.
    switch (model) {
    case WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER:
        pageCacheCapacity = 0;
        cacheTotalCapacity = 0; // FIXME: The Mac port actually sets this to larger than 0.
        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;
        deadDecodedDataDeletionInterval = 0;
        break;
    case WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER:
        pageCacheCapacity = 2;
        cacheTotalCapacity = 16 * 1024 * 1024;
        cacheMinDeadCapacity = cacheTotalCapacity / 8;
        cacheMaxDeadCapacity = cacheTotalCapacity / 4;
        deadDecodedDataDeletionInterval = 0;
        break;
    case WEBKIT_CACHE_MODEL_WEB_BROWSER:
        // Page cache capacity (in pages). Comment from Mac port:
        // (Research indicates that value / page drops substantially after 3 pages.)
        pageCacheCapacity = 3;
        cacheTotalCapacity = 32 * 1024 * 1024;
        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;
        deadDecodedDataDeletionInterval = 60;
        break;
    default:
        g_return_if_reached();
    }

    memoryCache()->setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    memoryCache()->setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
    pageCache()->setCapacity(pageCacheCapacity);
    cacheModel = model;
}

/**
 * webkit_get_cache_model:
 *
 * Returns the current cache model. For more information about this
 * value check the documentation of the function
 * webkit_set_cache_model().
 *
 * Return value: the current #WebKitCacheModel
 *
 * Since: 1.1.18
 */
WebKitCacheModel webkit_get_cache_model()
{
    webkitInit();
    return cacheModel;
}

/**
 * webkit_get_web_plugin_database:
 *
 * Returns the current #WebKitWebPluginDatabase with information about
 * all the plugins WebKit knows about in this instance.
 *
 * Return value: (transfer none): the current #WebKitWebPluginDatabase
 *
 * Since: 1.3.8
 */
WebKitWebPluginDatabase* webkit_get_web_plugin_database()
{
    static WebKitWebPluginDatabase* database = 0;

    webkitInit();

    if (!database)
        database = webkit_web_plugin_database_new();

    return database;
}


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

/**
 * webkit_get_icon_database:
 *
 * Returns the #WebKitIconDatabase providing access to website icons.
 *
 * Return value: (transfer none): the current #WebKitIconDatabase
 *
 * Since: 1.3.13
 */
WebKitIconDatabase* webkit_get_icon_database()
{
    webkitInit();

    static WebKitIconDatabase* database = 0;
    if (!database)
        database = WEBKIT_ICON_DATABASE(g_object_new(WEBKIT_TYPE_ICON_DATABASE, NULL));

    return database;
}

static GRefPtr<WebKitSpellChecker> textChecker = 0;

static void webkitExit()
{
    g_object_unref(webkit_get_default_session());
    textChecker = 0;
}

/**
 * webkit_get_text_checker:
 *
 * Returns: the #WebKitSpellChecker used by WebKit, or %NULL if spell
 * checking is not enabled
 *
 * Since: 1.5.1
 **/
GObject* webkit_get_text_checker()
{
    webkitInit();

#if ENABLE(SPELLCHECK)
    if (!textChecker)
        textChecker = adoptGRef(WEBKIT_SPELL_CHECKER(g_object_new(WEBKIT_TYPE_SPELL_CHECKER_ENCHANT, NULL)));
#endif

    return G_OBJECT(textChecker.get());
}

/**
 * webkit_set_text_checker:
 * @checker: a #WebKitSpellChecker or %NULL
 *
 * Sets @checker as the spell checker to be used by WebKit. The API
 * accepts GObject since in the future we might accept objects
 * implementing multiple interfaces (for example, spell checking and
 * grammar checking).
 *
 * Since: 1.5.1
 **/
void webkit_set_text_checker(GObject* checker)
{
    g_return_if_fail(!checker || WEBKIT_IS_SPELL_CHECKER(checker));

    webkitInit();

    // We need to do this because we need the cast, and casting NULL
    // is not kosher.
    textChecker = checker ? WEBKIT_SPELL_CHECKER(checker) : 0;
}

void webkitInit()
{
    static bool isInitialized = false;
    if (isInitialized)
        return;
    isInitialized = true;

    bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

    JSC::initializeThreading();
    WTF::initializeMainThread();

    WebCore::initializeLoggingChannelsIfNecessary();

    // We make sure the text codecs have been initialized, because
    // that may only be done by the main thread.
    atomicCanonicalTextEncodingName("UTF-8");

    GOwnPtr<gchar> databaseDirectory(g_build_filename(g_get_user_data_dir(), "webkit", "databases", NULL));
    webkit_set_web_database_directory_path(databaseDirectory.get());

    GOwnPtr<gchar> cacheDirectory(g_build_filename(g_get_user_cache_dir(), "webkitgtk", "applications", NULL));
    WebCore::cacheStorage().setCacheDirectory(cacheDirectory.get());

    PageGroup::setShouldTrackVisitedLinks(true);

    GOwnPtr<gchar> iconDatabasePath(g_build_filename(g_get_user_data_dir(), "webkit", "icondatabase", NULL));
    webkit_icon_database_set_path(webkit_get_icon_database(), iconDatabasePath.get());

    SoupSession* session = webkit_get_default_session();

    SoupSessionFeature* authDialog = static_cast<SoupSessionFeature*>(g_object_new(WEBKIT_TYPE_SOUP_AUTH_DIALOG, NULL));
    g_signal_connect(authDialog, "current-toplevel", G_CALLBACK(currentToplevelCallback), NULL);
    soup_session_add_feature(session, authDialog);
    g_object_unref(authDialog);

    SoupSessionFeature* sniffer = static_cast<SoupSessionFeature*>(g_object_new(SOUP_TYPE_CONTENT_SNIFFER, NULL));
    soup_session_add_feature(session, sniffer);
    g_object_unref(sniffer);

    soup_session_add_feature_by_type(session, SOUP_TYPE_CONTENT_DECODER);

    atexit(webkitExit);
}
