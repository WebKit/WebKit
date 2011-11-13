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
#include "ewk_settings.h"

#include "EWebKit.h"
#if ENABLE(SQL_DATABASE)
#include "DatabaseTracker.h"
#endif
#include "FrameView.h"
#include "IconDatabase.h"
#include "Image.h"
#include "IntSize.h"
#include "KURL.h"
#include "MemoryCache.h"
// FIXME: Why is there a directory in this include?
#include "appcache/ApplicationCacheStorage.h"
#include "ewk_private.h"

#include <Eina.h>
#include <eina_safety_checks.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

static const char* _ewk_cache_directory_path = 0;

static const char* _ewk_icon_database_path = 0;
#if ENABLE(SQL_DATABASE)
static const char* _ewk_default_web_database_path = 0;
static uint64_t _ewk_default_web_database_quota = 1 * 1024 * 1024;
#endif

static WTF::String _ewk_settings_webkit_platform_get()
{
    WTF::String uaPlatform;
#if PLATFORM(X11)
    uaPlatform = "X11";
#else
    uaPlatform = "Unknown";
#endif
    return uaPlatform;
}

static WTF::String _ewk_settings_webkit_os_version_get()
{
    WTF::String uaOsVersion;
    struct utsname name;

    if (uname(&name) != -1)
        uaOsVersion = makeString(name.sysname, ' ', name.machine);
    else
        uaOsVersion = "Unknown";

    return uaOsVersion;
}

uint64_t ewk_settings_web_database_default_quota_get()
{
#if ENABLE(SQL_DATABASE)
    return _ewk_default_web_database_quota;
#else
    return 0;
#endif
}

void ewk_settings_web_database_path_set(const char* path)
{
#if ENABLE(SQL_DATABASE)
    WebCore::DatabaseTracker::tracker().setDatabaseDirectoryPath(WTF::String::fromUTF8(path));
    eina_stringshare_replace(&_ewk_default_web_database_path, path);
#endif
}

const char* ewk_settings_web_database_path_get(void)
{
#if ENABLE(SQL_DATABASE)
    return _ewk_default_web_database_path;
#else
    return 0;
#endif
}

Eina_Bool ewk_settings_icon_database_path_set(const char* directory)
{
    WebCore::IconDatabase::delayDatabaseCleanup();

    if (directory) {
        if (WebCore::iconDatabase().isEnabled()) {
            ERR("IconDatabase is already open: %s", _ewk_icon_database_path);
            return false;
        }

        struct stat st;

        if (stat(directory, &st)) {
            ERR("could not stat(%s): %s", directory, strerror(errno));
            return false;
        }

        if (!S_ISDIR(st.st_mode)) {
            ERR("not a directory: %s", directory);
            return false;
        }

        if (access(directory, R_OK | W_OK)) {
            ERR("could not access directory '%s' for read and write: %s",
                directory, strerror(errno));
            return false;
        }

        WebCore::iconDatabase().setEnabled(true);
        WebCore::iconDatabase().open(WTF::String::fromUTF8(directory), WebCore::IconDatabase::defaultDatabaseFilename());

        eina_stringshare_replace(&_ewk_icon_database_path, directory);
    } else {
        WebCore::iconDatabase().setEnabled(false);
        WebCore::iconDatabase().close();

        eina_stringshare_del(_ewk_icon_database_path);
        _ewk_icon_database_path = 0;
    }
    return true;
}

const char* ewk_settings_icon_database_path_get(void)
{
    if (!WebCore::iconDatabase().isEnabled())
        return 0;
    if (!WebCore::iconDatabase().isOpen())
        return 0;

    return _ewk_icon_database_path;
}

Eina_Bool ewk_settings_icon_database_clear(void)
{
    if (!WebCore::iconDatabase().isEnabled())
        return false;
    if (!WebCore::iconDatabase().isOpen())
        return false;

    WebCore::iconDatabase().removeAllIcons();
    return true;
}

cairo_surface_t* ewk_settings_icon_database_icon_surface_get(const char* url)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, 0);

    WebCore::KURL kurl(WebCore::KURL(), WTF::String::fromUTF8(url));
    WebCore::Image* icon = WebCore::iconDatabase().synchronousIconForPageURL(kurl.string(), WebCore::IntSize(16, 16));

    if (!icon) {
        ERR("no icon for url %s", url);
        return 0;
    }

    return icon->nativeImageForCurrentFrame();
}

Evas_Object* ewk_settings_icon_database_icon_object_add(const char* url, Evas* canvas)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);

    WebCore::KURL kurl(WebCore::KURL(), WTF::String::fromUTF8(url));
    WebCore::Image* icon = WebCore::iconDatabase().synchronousIconForPageURL(kurl.string(), WebCore::IntSize(16, 16));
    cairo_surface_t* surface;

    if (!icon) {
        ERR("no icon for url %s", url);
        return 0;
    }

    surface = icon->nativeImageForCurrentFrame();
    return ewk_util_image_from_cairo_surface_add(canvas, surface);
}

Eina_Bool ewk_settings_cache_enable_get(void)
{
    WebCore::MemoryCache* cache = WebCore::memoryCache();
    return !cache->disabled();
}

void ewk_settings_cache_enable_set(Eina_Bool set)
{
    WebCore::MemoryCache* cache = WebCore::memoryCache();
    set = !set;
    if (cache->disabled() != set)
        cache->setDisabled(set);
}

void ewk_settings_cache_capacity_set(unsigned capacity)
{
    WebCore::MemoryCache* cache = WebCore::memoryCache();
    cache->setCapacities(0, capacity, capacity);
}

void ewk_settings_repaint_throttling_set(double deferredRepaintDelay, double initialDeferredRepaintDelayDuringLoading, double maxDeferredRepaintDelayDuringLoading, double deferredRepaintDelayIncrementDuringLoading)
{
    WebCore::FrameView::setRepaintThrottlingDeferredRepaintDelay(deferredRepaintDelay);
    WebCore::FrameView::setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(initialDeferredRepaintDelayDuringLoading);
    WebCore::FrameView::setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(maxDeferredRepaintDelayDuringLoading);
    WebCore::FrameView::setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(deferredRepaintDelayIncrementDuringLoading);
}

/**
 * @internal
 *
 * Gets the default user agent string.
 *
 * @return a pointer to an eina_stringshare containing the user agent string
 */
const char* ewk_settings_default_user_agent_get(void)
{
    WTF::String uaVersion = makeString(String::number(WEBKIT_USER_AGENT_MAJOR_VERSION), '.', String::number(WEBKIT_USER_AGENT_MINOR_VERSION), '+');
    WTF::String staticUa = makeString("Mozilla/5.0 (", _ewk_settings_webkit_platform_get(), "; ", _ewk_settings_webkit_os_version_get(), ") AppleWebKit/", uaVersion) + makeString(" (KHTML, like Gecko) Version/5.0 Safari/", uaVersion);

    return eina_stringshare_add(staticUa.utf8().data());
}

Eina_Bool ewk_settings_cache_directory_path_set(const char* path)
{
    if (!path)
        return false;

    WebCore::cacheStorage().setCacheDirectory(WTF::String::fromUTF8(path));
    eina_stringshare_replace(&_ewk_cache_directory_path, path);

    return true;
}

const char* ewk_settings_cache_directory_path_get()
{
    return _ewk_cache_directory_path;
}

double ewk_settings_default_timer_interval_get(void)
{
    return WebCore::Settings::defaultMinDOMTimerInterval();
}
