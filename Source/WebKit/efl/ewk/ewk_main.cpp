/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2010 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "ewk_main.h"

#include "appcache/ApplicationCacheStorage.h"
#include "EWebKit.h"
#include "Logging.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "ewk_private.h"
#include "ewk_settings.h"
#include "runtime/InitializeThreading.h"
#include "wtf/Threading.h"

#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Eina.h>
#include <Evas.h>
#include <stdlib.h>
#include <sys/stat.h>

#if ENABLE(GLIB_SUPPORT)
#include <glib-object.h>
#include <glib.h>

#ifdef ENABLE_GTK_PLUGINS_SUPPORT
#include <gtk/gtk.h>
#endif

#endif

#ifdef WTF_USE_SOUP
// REMOVE-ME: see todo below
#include "ResourceHandle.h"
#include <libsoup/soup.h>
#endif

static int _ewk_init_count = 0;
int _ewk_log_dom = -1;

static Eina_Bool _ewk_init_body(void);

int ewk_init(void)
{
    if (_ewk_init_count)
        return ++_ewk_init_count;

    if (!eina_init())
        goto error_eina;

    _ewk_log_dom = eina_log_domain_register("ewebkit", EINA_COLOR_ORANGE);
    if (_ewk_log_dom < 0) {
        EINA_LOG_CRIT("could not register log domain 'ewebkit'");
        goto error_log_domain;
    }

    if (!evas_init()) {
        CRITICAL("could not init evas.");
        goto error_evas;
    }

    if (!ecore_init()) {
        CRITICAL("could not init ecore.");
        goto error_ecore;
    }

    if (!ecore_evas_init()) {
        CRITICAL("could not init ecore_evas.");
        goto error_ecore_evas;
    }

    if (!edje_init()) {
        CRITICAL("could not init edje.");
        goto error_edje;
    }

    _ewk_init_body();

    return ++_ewk_init_count;

error_edje:
    ecore_evas_shutdown();
error_ecore_evas:
    ecore_shutdown();
error_ecore:
    evas_shutdown();
error_evas:
    eina_log_domain_unregister(_ewk_log_dom);
    _ewk_log_dom = -1;
error_log_domain:
    eina_shutdown();
error_eina:
    return 0;
}

int ewk_shutdown(void)
{
    _ewk_init_count--;
    if (_ewk_init_count)
        return _ewk_init_count;

    ecore_evas_shutdown();
    ecore_shutdown();
    evas_shutdown();
    eina_log_domain_unregister(_ewk_log_dom);
    _ewk_log_dom = -1;
    eina_shutdown();

    return 0;
}

Eina_Bool _ewk_init_body(void)
{

#if ENABLE(GLIB_SUPPORT)
    g_type_init();

    if (!g_thread_supported())
        g_thread_init(0);

#ifdef ENABLE_GTK_PLUGINS_SUPPORT
    gdk_threads_init();
    if (!gtk_init_check(0, 0))
        WRN("Could not initialize GTK support.");
#endif

    if (!ecore_main_loop_glib_integrate())
        WRN("Ecore was not compiled with GLib support, some plugins will not "
            "work (ie: Adobe Flash)");
#endif

    JSC::initializeThreading();
    WTF::initializeMainThread();
    WebCore::InitializeLoggingChannelsIfNecessary();

    // Page cache capacity (in pages). Comment from Mac port:
    // (Research indicates that value / page drops substantially after 3 pages.)
    // FIXME: Expose this with an API and/or calculate based on available resources
    WebCore::pageCache()->setCapacity(3);
    WebCore::PageGroup::setShouldTrackVisitedLinks(true);

    // set default location of web database path and appcache dir
    const char* home = getenv("HOME");
    if (!home) // don't forget about the homeless
        home = "/tmp"; // this directory must always exist

    // anyway, check it first
    struct stat state;
    if (stat(home, &state) == -1) {
        // Exit now - otherwise you may have some crash later
        int errnowas = errno;
        CRITICAL("Can't access HOME dir (or /tmp) - no place to save databases: %s", strerror(errnowas));
        return EINA_FALSE;
    }

    WTF::String wkdir = WTF::String(home) + "/.webkit";
    ewk_settings_web_database_path_set(wkdir.utf8().data());
    ewk_settings_icon_database_path_set(wkdir.utf8().data());

    WebCore::cacheStorage().setCacheDirectory(wkdir);

    // TODO: this should move to WebCore, already reported to webkit-gtk folks:
#ifdef WTF_USE_SOUP
    if (1) {
        SoupSession* session = WebCore::ResourceHandle::defaultSession();
        soup_session_add_feature_by_type(session, SOUP_TYPE_CONTENT_SNIFFER);
        soup_session_add_feature_by_type(session, SOUP_TYPE_CONTENT_DECODER);
    }
#endif

    return EINA_TRUE;
}

