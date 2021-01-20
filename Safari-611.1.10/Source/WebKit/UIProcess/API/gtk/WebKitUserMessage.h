/*
 * Copyright (C) 2019 Igalia S.L.
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

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION) && !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitUserMessage_h
#define WebKitUserMessage_h

#include <gio/gio.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_USER_MESSAGE            (webkit_user_message_get_type())
#define WEBKIT_USER_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_USER_MESSAGE, WebKitUserMessage))
#define WEBKIT_IS_USER_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_USER_MESSAGE))
#define WEBKIT_USER_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_USER_MESSAGE, WebKitUserMessageClass))
#define WEBKIT_IS_USER_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_USER_MESSAGE))
#define WEBKIT_USER_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_USER_MESSAGE, WebKitUserMessageClass))

#define WEBKIT_USER_MESSAGE_ERROR webkit_user_message_error_quark ()

typedef struct _WebKitUserMessage        WebKitUserMessage;
typedef struct _WebKitUserMessageClass   WebKitUserMessageClass;
typedef struct _WebKitUserMessagePrivate WebKitUserMessagePrivate;

/**
 * WebKitUserMessageError:
 * @WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE: The message was not handled by the receiver.
 *
 * Enum values used to denote errors happening when sending user messages.
 *
 * Since: 2.28
 */
typedef enum {
    WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE
} WebKitUserMessageError;

struct _WebKitUserMessage {
    GInitiallyUnowned parent;

    WebKitUserMessagePrivate *priv;
};

struct _WebKitUserMessageClass {
    GInitiallyUnownedClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_user_message_get_type         (void);

WEBKIT_API GQuark
webkit_user_message_error_quark      (void);

WEBKIT_API WebKitUserMessage *
webkit_user_message_new              (const char        *name,
                                      GVariant          *parameters);

WEBKIT_API WebKitUserMessage *
webkit_user_message_new_with_fd_list (const char        *name,
                                      GVariant          *parameters,
                                      GUnixFDList       *fd_list);

WEBKIT_API const char *
webkit_user_message_get_name         (WebKitUserMessage *message);

WEBKIT_API GVariant *
webkit_user_message_get_parameters   (WebKitUserMessage *message);

WEBKIT_API GUnixFDList *
webkit_user_message_get_fd_list      (WebKitUserMessage *message);

WEBKIT_API void
webkit_user_message_send_reply       (WebKitUserMessage *message,
                                      WebKitUserMessage *reply);

G_END_DECLS

#endif
