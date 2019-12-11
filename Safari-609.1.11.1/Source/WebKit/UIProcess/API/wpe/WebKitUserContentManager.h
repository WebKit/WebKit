/*
 * Copyright (C) 2014 Igalia S.L.
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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitUserContentManager_h
#define WebKitUserContentManager_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>
#include <wpe/WebKitUserContent.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_USER_CONTENT_MANAGER            (webkit_user_content_manager_get_type())
#define WEBKIT_USER_CONTENT_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_USER_CONTENT_MANAGER, WebKitUserContentManager))
#define WEBKIT_IS_USER_CONTENT_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_USER_CONTENT_MANAGER))
#define WEBKIT_USER_CONTENT_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_USER_CONTENT_MANAGER, WebKitUserContentManagerClass))
#define WEBKIT_IS_USER_CONTENT_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_USER_CONTENT_MANAGER))
#define WEBKIT_USER_CONTENT_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_USER_CONTENT_MANAGER, WebKitUserContentManagerClass))

typedef struct _WebKitUserContentManager        WebKitUserContentManager;
typedef struct _WebKitUserContentManagerClass   WebKitUserContentManagerClass;
typedef struct _WebKitUserContentManagerPrivate WebKitUserContentManagerPrivate;


struct _WebKitUserContentManager {
    GObject parent;

    /*< private >*/
    WebKitUserContentManagerPrivate *priv;
};

struct _WebKitUserContentManagerClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_user_content_manager_get_type                                   (void);

WEBKIT_API WebKitUserContentManager *
webkit_user_content_manager_new                                        (void);

WEBKIT_API void
webkit_user_content_manager_add_style_sheet                            (WebKitUserContentManager *manager,
                                                                        WebKitUserStyleSheet     *stylesheet);
WEBKIT_API void
webkit_user_content_manager_remove_all_style_sheets                    (WebKitUserContentManager *manager);

WEBKIT_API gboolean
webkit_user_content_manager_register_script_message_handler            (WebKitUserContentManager *manager,
                                                                        const gchar              *name);
WEBKIT_API void
webkit_user_content_manager_unregister_script_message_handler          (WebKitUserContentManager *manager,
                                                                        const gchar              *name);

WEBKIT_API gboolean
webkit_user_content_manager_register_script_message_handler_in_world   (WebKitUserContentManager *manager,
                                                                        const gchar              *name,
                                                                        const gchar              *world_name);
WEBKIT_API void
webkit_user_content_manager_unregister_script_message_handler_in_world (WebKitUserContentManager *manager,
                                                                        const gchar              *name,
                                                                        const gchar              *world_name);

WEBKIT_API void
webkit_user_content_manager_add_script                                 (WebKitUserContentManager *manager,
                                                                        WebKitUserScript         *script);

WEBKIT_API void
webkit_user_content_manager_remove_all_scripts                         (WebKitUserContentManager *manager);

WEBKIT_API void
webkit_user_content_manager_add_filter                                 (WebKitUserContentManager *manager,
                                                                        WebKitUserContentFilter  *filter);

WEBKIT_API void
webkit_user_content_manager_remove_filter                              (WebKitUserContentManager *manager,
                                                                        WebKitUserContentFilter  *filter);

WEBKIT_API void
webkit_user_content_manager_remove_filter_by_id                        (WebKitUserContentManager *manager,
                                                                        const char               *filter_id);

WEBKIT_API void
webkit_user_content_manager_remove_all_filters                         (WebKitUserContentManager *manager);

G_END_DECLS

#endif
