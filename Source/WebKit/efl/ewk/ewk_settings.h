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

#ifndef ewk_settings_h
#define ewk_settings_h

#include <Eina.h>
#include <Evas.h>
#include <cairo.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file ewk_settings.h
 *
 * @brief General purpose settings, not tied to any view object.
 */

/**
 * Returns the default quota for Web Database databases. By default
 * this value is 1MB.
 *
 * @return the current default database quota in bytes
 */
EAPI uint64_t         ewk_settings_web_database_default_quota_get(void);

/**
 * Sets the current path to the directory WebKit will write Web
 * Database databases.
 *
 * @param path the new database directory path
 */
EAPI void             ewk_settings_web_database_path_set(const char *path);

/**
 * Returns directory path where web database is stored.
 *
 * This is guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use eina_stringshare_ref()
 * instead of eina_stringshare_add() or strdup().
 *
 * @return database path or @c 0 if none or web database is not supported
 */
EAPI const char      *ewk_settings_web_database_path_get(void);

/**
 * Sets directory where to store icon database, opening or closing database.
 *
 * Icon database must be opened only once. If you try to set a path when the icon
 * database is already open, this function returns @c EINA_FALSE.
 *
 * @param directory where to store icon database, must be
 *        write-able, if @c 0 is given, then database is closed
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE on errors
 */
EAPI Eina_Bool        ewk_settings_icon_database_path_set(const char *path);

/**
 * Returns directory path where icon database is stored.
 *
 * This is guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use eina_stringshare_ref()
 * instead of eina_stringshare_add() or strdup().
 *
 * @return database path or @c 0 if none is set or database is closed
 */
EAPI const char      *ewk_settings_icon_database_path_get(void);

/**
 * Removes all known icons from database.
 *
 * Database must be opened with ewk_settings_icon_database_path_set()
 * in order to work.
 *
 * @return @c EINA_TRUE on success or @c EINA_FALSE otherwise, like
 *         closed database.
 */
EAPI Eina_Bool        ewk_settings_icon_database_clear(void);

/**
 * Queries icon for given URL, returning associated cairo surface.
 *
 * @note In order to have this working, one must open icon database
 *       with ewk_settings_icon_database_path_set().
 *
 * @param url which url to query icon
 *
 * @return cairo surface if any, or @c 0 on failure
 */
EAPI cairo_surface_t *ewk_settings_icon_database_icon_surface_get(const char *url);

/**
 * Creates Evas_Object of type image representing the given URL.
 *
 * This is an utility function that creates an Evas_Object of type
 * image set to have fill always match object size
 * (evas_object_image_filled_add()), saving some code to use it from Evas.
 *
 * @note In order to have this working, one must open icon database
 *       with ewk_settings_icon_database_path_set().
 *
 * @param url which url to query icon
 * @param canvas evas instance where to add resulting object
 *
 * @return newly allocated Evas_Object instance or @c 0 on
 *         errors. Delete the object with evas_object_del().
 */
EAPI Evas_Object     *ewk_settings_icon_database_icon_object_add(const char *url, Evas *canvas);

/**
 * Sets cache directory.
 *
 * @param path where to store cache, must be write-able.
 *
 * @return @c EINA_TRUE on success, @c EINA_FALSE if path is NULL or offline
 *         web application is not supported.
 */
EAPI Eina_Bool        ewk_settings_cache_directory_path_set(const char *path);

/**
 * Return cache directory path.
 *
 * This is guaranteed to be eina_stringshare, so whenever possible
 * save yourself some cpu cycles and use eina_stringshare_ref()
 * instead of eina_stringshare_add() or strdup().
 *
 * @return cache directory path.
 */
EAPI const char      *ewk_settings_cache_directory_path_get(void);

/**
 * Gets status of the memory cache of WebCore.
 *
 * @return @c EINA_TRUE if the cache is enabled or @c EINA_FALSE if not
 */
EAPI Eina_Bool        ewk_settings_cache_enable_get(void);

/**
 * Enables/disables the memory cache of WebCore, possibly clearing it.
 *
 * Disabling the cache will remove all resources from the cache.
 * They may still live on if they are referenced by some Web page though.
 *
 * @param set @c EINA_TRUE to enable memory cache, @c EINA_FALSE to disable
 */
EAPI void             ewk_settings_cache_enable_set(Eina_Bool set);

/**
 * Sets capacity of memory cache of WebCore.
 *
 * WebCore sets default value of memory cache on 8192 * 1024 bytes.
 *
 * @param capacity the maximum number of bytes that the cache should consume overall
 */
EAPI void             ewk_settings_cache_capacity_set(unsigned capacity);

/**
 * Sets values for repaint throttling.
 *
 * It allows to slow down page loading and
 * should ensure displaying a content with many css/gif animations.
 *
 * These values can be used as a example for repaints throttling.
 * 0,     0,   0,    0    - default WebCore's values, these do not delay any repaints
 * 0.025, 0,   2.5,  0.5  - recommended values for dynamic content
 * 0.01,  0,   1,    0.2  - minimal level
 * 0.025, 1,   5,    0.5  - medium level
 * 0.1,   2,   10,   1    - heavy level
 *
 * @param deferred_repaint_delay a normal delay
 * @param initial_deferred_repaint_delay_during_loading negative value would mean that first few repaints happen without a delay
 * @param max_deferred_repaint_delay_during_loading the delay grows on each repaint to this maximum value
 * @param deferred_repaint_delay_increment_during_loading on each repaint the delay increses by this amount
 */
EAPI void             ewk_settings_repaint_throttling_set(double deferred_repaint_delay, double initial_deferred_repaint_delay_during_loading, double max_deferred_repaint_delay_during_loading, double deferred_repaint_delay_increment_during_loading);

#ifdef __cplusplus
}
#endif
#endif // ewk_settings_h
