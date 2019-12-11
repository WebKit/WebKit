/*
 * Copyright (C) 2014 Collabora Ltd.
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitNotification_h
#define WebKitNotification_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitForwardDeclarations.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_NOTIFICATION            (webkit_notification_get_type())
#define WEBKIT_NOTIFICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_NOTIFICATION, WebKitNotification))
#define WEBKIT_IS_NOTIFICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_NOTIFICATION))
#define WEBKIT_NOTIFICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_NOTIFICATION, WebKitNotificationClass))
#define WEBKIT_IS_NOTIFICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_NOTIFICATION))
#define WEBKIT_NOTIFICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_NOTIFICATION, WebKitNotificationClass))

typedef struct _WebKitNotification        WebKitNotification;
typedef struct _WebKitNotificationClass   WebKitNotificationClass;
typedef struct _WebKitNotificationPrivate WebKitNotificationPrivate;

struct _WebKitNotification {
    GObject parent;

    WebKitNotificationPrivate *priv;
};

struct _WebKitNotificationClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
    void (*_webkit_reserved4) (void);
    void (*_webkit_reserved5) (void);
};

WEBKIT_API GType
webkit_notification_get_type                 (void);

WEBKIT_API guint64
webkit_notification_get_id                   (WebKitNotification *notification);

WEBKIT_API const gchar *
webkit_notification_get_title                (WebKitNotification *notification);

WEBKIT_API const gchar *
webkit_notification_get_body                 (WebKitNotification *notification);

WEBKIT_API const gchar *
webkit_notification_get_tag                  (WebKitNotification *notification);

WEBKIT_API void
webkit_notification_close                    (WebKitNotification *notification);

WEBKIT_API void
webkit_notification_clicked                  (WebKitNotification *notification);

G_END_DECLS

#endif
