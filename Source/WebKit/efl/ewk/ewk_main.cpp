/*
    Copyright (C) 2009-2010 ProFUSION embedded systems
    Copyright (C) 2009-2011 Samsung Electronics

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

#include "FileSystem.h"
#include "Logging.h"
#include "PageCache.h"
#include "PageGroup.h"
#include "ScriptController.h"
#include "Settings.h"
#include "ewk_logging.h"
#include "ewk_network.h"
#include "ewk_private.h"
#include "ewk_settings.h"
#include "runtime/InitializeThreading.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Eina.h>
#include <Evas.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wtf/Threading.h>

#if ENABLE(GLIB_SUPPORT)
#include <glib-object.h>
#include <glib.h>

#ifdef ENABLE_GTK_PLUGINS_SUPPORT
#include <gtk/gtk.h>
#endif

#endif

#if USE(SOUP)
// REMOVE-ME: see todo below
#include "ResourceHandle.h"
#include "ewk_auth_soup.h"
#include <libsoup/soup.h>
#endif

static int _ewkInitCount = 0;

/**
 * \var     _ewk_log_dom
 * @brief   the log domain identifier that is used with EINA's macros
 */
int _ewk_log_dom = -1;

static Eina_Bool _ewk_init_body(void);

int ewk_init(void)
{
    if (_ewkInitCount)
        return ++_ewkInitCount;

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

    if (!_ewk_init_body()) {
        CRITICAL("could not init body");
        goto error_edje;
    }

    return ++_ewkInitCount;

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
    _ewkInitCount--;
    if (_ewkInitCount)
        return _ewkInitCount;

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

    WebCore::ScriptController::initializeThreading();
    WebCore::initializeLoggingChannelsIfNecessary();
    WebCore::Settings::setDefaultMinDOMTimerInterval(0.004);

    // Page cache capacity (in pages). Comment from Mac port:
    // (Research indicates that value / page drops substantially after 3 pages.)
    // FIXME: Expose this with an API and/or calculate based on available resources
    WebCore::pageCache()->setCapacity(3);
    WebCore::PageGroup::setShouldTrackVisitedLinks(true);

    String home = WebCore::homeDirectoryPath();
    struct stat state;
    // check home directory first
    if (stat(home.utf8().data(), &state) == -1) {
        // Exit now - otherwise you may have some crash later
        int errnowas = errno;
        CRITICAL("Can't access HOME dir (or /tmp) - no place to save databases: %s", strerror(errnowas));
        return false;
    }

    WTF::String webkitDirectory = home + "/.webkit";
    if (WebCore::makeAllDirectories(webkitDirectory)) {
        ewk_settings_web_database_path_set(webkitDirectory.utf8().data());
        ewk_settings_application_cache_path_set(webkitDirectory.utf8().data());
    }

    ewk_network_tls_certificate_check_set(false);

    // TODO: this should move to WebCore, already reported to webkit-gtk folks:
#if USE(SOUP)
    if (1) {
        SoupSession* session = WebCore::ResourceHandle::defaultSession();
        soup_session_add_feature_by_type(session, SOUP_TYPE_CONTENT_SNIFFER);
        soup_session_add_feature_by_type(session, SOUP_TYPE_CONTENT_DECODER);

        SoupSessionFeature* auth_dialog = static_cast<SoupSessionFeature*>(g_object_new(EWK_TYPE_SOUP_AUTH_DIALOG, 0));
        soup_session_add_feature(session, auth_dialog);
    }
#endif

    return true;
}
