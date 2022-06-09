/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitFaviconDatabase_h
#define WebKitFaviconDatabase_h

#include <cairo.h>
#include <gio/gio.h>
#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_FAVICON_DATABASE            (webkit_favicon_database_get_type())
#define WEBKIT_FAVICON_DATABASE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_FAVICON_DATABASE, WebKitFaviconDatabase))
#define WEBKIT_IS_FAVICON_DATABASE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_FAVICON_DATABASE))
#define WEBKIT_FAVICON_DATABASE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_FAVICON_DATABASE, WebKitFaviconDatabaseClass))
#define WEBKIT_IS_FAVICON_DATABASE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_FAVICON_DATABASE))
#define WEBKIT_FAVICON_DATABASE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_FAVICON_DATABASE, WebKitFaviconDatabaseClass))
#define WEBKIT_FAVICON_DATABASE_ERROR           (webkit_favicon_database_error_quark())

typedef struct _WebKitFaviconDatabase        WebKitFaviconDatabase;
typedef struct _WebKitFaviconDatabaseClass   WebKitFaviconDatabaseClass;
typedef struct _WebKitFaviconDatabasePrivate WebKitFaviconDatabasePrivate;

struct _WebKitFaviconDatabase {
    GObject parent;

    WebKitFaviconDatabasePrivate *priv;
};

struct _WebKitFaviconDatabaseClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

/**
 * WebKitFaviconDatabaseError:
 * @WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED: The #WebKitFaviconDatabase has not been initialized yet
 * @WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND: There is not an icon available for the requested URL
 * @WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN: There might be an icon for the requested URL, but its data is unknown at the moment
 *
 * Enum values used to denote the various errors related to the #WebKitFaviconDatabase.
 **/
typedef enum {
    WEBKIT_FAVICON_DATABASE_ERROR_NOT_INITIALIZED,
    WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_NOT_FOUND,
    WEBKIT_FAVICON_DATABASE_ERROR_FAVICON_UNKNOWN
} WebKitFaviconDatabaseError;

WEBKIT_API GQuark
webkit_favicon_database_error_quark        (void);

WEBKIT_API GType
webkit_favicon_database_get_type           (void);

WEBKIT_API void
webkit_favicon_database_get_favicon        (WebKitFaviconDatabase *database,
                                            const gchar           *page_uri,
                                            GCancellable          *cancellable,
                                            GAsyncReadyCallback    callback,
                                            gpointer               user_data);
WEBKIT_API cairo_surface_t *
webkit_favicon_database_get_favicon_finish (WebKitFaviconDatabase *database,
                                            GAsyncResult          *result,
                                            GError               **error);
WEBKIT_API gchar *
webkit_favicon_database_get_favicon_uri    (WebKitFaviconDatabase *database,
                                            const gchar           *page_uri);
WEBKIT_API void
webkit_favicon_database_clear              (WebKitFaviconDatabase *database);

G_END_DECLS

#endif
