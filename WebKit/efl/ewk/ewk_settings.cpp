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
#include "IconDatabase.h"
#include "Image.h"
#include "IntSize.h"
#include "KURL.h"
#include "ewk_private.h"
#include <wtf/text/CString.h>

#include <eina_safety_checks.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static uint64_t _ewk_default_web_database_quota = 1 * 1024;

/**
 * Returns the default quota for Web Database databases. By default
 * this value is 1MB.
 *
 * @return the current default database quota in bytes
 **/
uint64_t ewk_settings_web_database_default_quota_get()
{
    return _ewk_default_web_database_quota;
}

/**
 * Sets directory where to store icon database, opening database.
 *
 * @param directory where to store icon database, must be
 *        write-able. If @c NULL is given, then database is closed.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on errors.
 */
Eina_Bool ewk_settings_icon_database_path_set(const char *directory)
{
    WebCore::iconDatabase()->delayDatabaseCleanup();

    if (directory) {
        struct stat st;

        if (stat(directory, &st)) {
            ERR("could not stat(%s): %s", directory, strerror(errno));
            return EINA_FALSE;
        }

        if (!S_ISDIR(st.st_mode)) {
            ERR("not a directory: %s", directory);
            return EINA_FALSE;
        }

        if (access(directory, R_OK | W_OK)) {
            ERR("could not access directory '%s' for read and write: %s",
                directory, strerror(errno));
            return EINA_FALSE;
        }

        WebCore::iconDatabase()->setEnabled(true);
        WebCore::iconDatabase()->open(WebCore::String::fromUTF8(directory));
    } else {
        WebCore::iconDatabase()->setEnabled(false);
        WebCore::iconDatabase()->close();
    }
    return EINA_TRUE;
}

/**
 * Return directory path where icon database is stored.
 *
 * @return newly allocated string with database path or @c NULL if
 *         none is set or database is closed. Note that return must be
 *         freed with free() as it's a strdup()ed copy of the string
 *         due reference counting.
 */
char* ewk_settings_icon_database_path_get(void)
{
    if (!WebCore::iconDatabase()->isEnabled())
        return 0;
    if (!WebCore::iconDatabase()->isOpen())
        return 0;

    WebCore::String path = WebCore::iconDatabase()->databasePath();
    if (path.isEmpty())
        return 0;
    return strdup(path.utf8().data());
}

/**
 * Remove all known icons from database.
 *
 * Database must be opened with ewk_settings_icon_database_path_set()
 * in order to work.
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise, like
 *         closed database.
 */
Eina_Bool ewk_settings_icon_database_clear(void)
{
    if (!WebCore::iconDatabase()->isEnabled())
        return EINA_FALSE;
    if (!WebCore::iconDatabase()->isOpen())
        return EINA_FALSE;

    WebCore::iconDatabase()->removeAllIcons();
    return EINA_TRUE;
}

/**
 * Query icon for given URL, returning associated cairo surface.
 *
 * @note in order to have this working, one must open icon database
 *       with ewk_settings_icon_database_path_set().
 *
 * @param url which url to query icon.
 *
 * @return cairo surface if any, or NULL on failure.
 */
cairo_surface_t* ewk_settings_icon_database_icon_surface_get(const char *url)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, 0);

    WebCore::KURL kurl(WebCore::KURL(), WebCore::String::fromUTF8(url));
    WebCore::Image *icon = WebCore::iconDatabase()->iconForPageURL(kurl.string(), WebCore::IntSize(16, 16));

    if (!icon) {
        ERR("no icon for url %s", url);
        return 0;
    }

    return icon->nativeImageForCurrentFrame();
}

/**
 * Create Evas_Object of type image representing the given URL.
 *
 * This is an utility function that creates an Evas_Object of type
 * image set to have fill always match object size
 * (evas_object_image_filled_add()), saving some code to use it from Evas.
 *
 * @note in order to have this working, one must open icon database
 *       with ewk_settings_icon_database_path_set().
 *
 * @param url which url to query icon.
 * @param canvas evas instance where to add resulting object.
 *
 * @return newly allocated Evas_Object instance or @c NULL on
 *         errors. Delete the object with evas_object_del().
 */
Evas_Object* ewk_settings_icon_database_icon_object_add(const char* url, Evas* canvas)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(url, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(canvas, 0);

    WebCore::KURL kurl(WebCore::KURL(), WebCore::String::fromUTF8(url));
    WebCore::Image* icon = WebCore::iconDatabase()->iconForPageURL(kurl.string(), WebCore::IntSize(16, 16));
    cairo_surface_t* surface;

    if (!icon) {
        ERR("no icon for url %s", url);
        return 0;
    }

    surface = icon->nativeImageForCurrentFrame();
    return ewk_util_image_from_cairo_surface_add(canvas, surface);
}
