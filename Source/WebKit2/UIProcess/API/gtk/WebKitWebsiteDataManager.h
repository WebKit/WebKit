/*
 * Copyright (C) 2015 Igalia S.L.
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

#ifndef WebKitWebsiteDataManager_h
#define WebKitWebsiteDataManager_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEBSITE_DATA_MANAGER            (webkit_website_data_manager_get_type())
#define WEBKIT_WEBSITE_DATA_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEBSITE_DATA_MANAGER, WebKitWebsiteDataManager))
#define WEBKIT_IS_WEBSITE_DATA_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEBSITE_DATA_MANAGER))
#define WEBKIT_WEBSITE_DATA_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEBSITE_DATA_MANAGER, WebKitWebsiteDataManagerClass))
#define WEBKIT_IS_WEBSITE_DATA_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEBSITE_DATA_MANAGER))
#define WEBKIT_WEBSITE_DATA_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEBSITE_DATA_MANAGER, WebKitWebsiteDataManagerClass))

typedef struct _WebKitWebsiteDataManager        WebKitWebsiteDataManager;
typedef struct _WebKitWebsiteDataManagerClass   WebKitWebsiteDataManagerClass;
typedef struct _WebKitWebsiteDataManagerPrivate WebKitWebsiteDataManagerPrivate;

struct _WebKitWebsiteDataManager {
    GObject parent;

    WebKitWebsiteDataManagerPrivate *priv;
};

struct _WebKitWebsiteDataManagerClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_website_data_manager_get_type                                (void);

WEBKIT_API WebKitWebsiteDataManager *
webkit_website_data_manager_new                                     (const gchar              *first_option_name,
                                                                     ...);
WEBKIT_API const gchar *
webkit_website_data_manager_get_base_data_directory                 (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_base_cache_directory                 (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_local_storage_directory             (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_disk_cache_directory                (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_offline_application_cache_directory (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_indexeddb_directory                 (WebKitWebsiteDataManager *manager);

WEBKIT_API const gchar *
webkit_website_data_manager_get_websql_directory                    (WebKitWebsiteDataManager *manager);

G_END_DECLS

#endif
