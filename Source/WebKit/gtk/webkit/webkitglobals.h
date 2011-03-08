/*
 * Copyright (C) 2007, 2008, 2009 Holger Hans Peter Freyther
 * Copyright (C) 2008 Jan Michael C. Alonzo
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2010 Igalia S.L.
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

#ifndef webkitglobals_h
#define webkitglobals_h

#include "webkitdefines.h"
#include <glib.h>
#include <libsoup/soup.h>

G_BEGIN_DECLS

/*
 * WebKitCacheModel:
 * @WEBKIT_CACHE_MODEL_DEFAULT: The default cache model. This is
 *   WEBKIT_CACHE_MODEL_WEB_BROWSER.
 * @WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER: Disable the cache completely, which
 *   substantially reduces memory usage. Useful for applications that only
 *   access a single local file, with no navigation to other pages. No remote
 *   resources will be cached.
 * @WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER: A cache model optimized for viewing
 *   a series of local files -- for example, a documentation viewer or a website
 *   designer. WebKit will cache a moderate number of resources.
 * @WEBKIT_CACHE_MODEL_WEB_BROWSER: Improve document load speed substantially
 *   by caching a very large number of resources and previously viewed content.
 *
 * Enum values used for determining the webview cache model.
 */
typedef enum {
    WEBKIT_CACHE_MODEL_DEFAULT,
    WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER,
    WEBKIT_CACHE_MODEL_WEB_BROWSER,
    WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER,
} WebKitCacheModel;

WEBKIT_API SoupSession*
webkit_get_default_session                      (void);

WEBKIT_API WebKitWebPluginDatabase *
webkit_get_web_plugin_database                  (void);

WEBKIT_API WebKitIconDatabase *
webkit_get_icon_database                        (void);

WEBKIT_API void
webkit_set_cache_model                          (WebKitCacheModel     cache_model);

WEBKIT_API WebKitCacheModel
webkit_get_cache_model                          (void);

G_END_DECLS

#endif
