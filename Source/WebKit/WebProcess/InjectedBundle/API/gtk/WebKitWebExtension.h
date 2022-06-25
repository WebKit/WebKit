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

#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit-web-extension.h> can be included directly."
#endif

#ifndef WebKitWebExtension_h
#define WebKitWebExtension_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitUserMessage.h>
#include <webkit2/WebKitWebPage.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_EXTENSION            (webkit_web_extension_get_type())
#define WEBKIT_WEB_EXTENSION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_EXTENSION, WebKitWebExtension))
#define WEBKIT_IS_WEB_EXTENSION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_EXTENSION))
#define WEBKIT_WEB_EXTENSION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_EXTENSION, WebKitWebExtensionClass))
#define WEBKIT_IS_WEB_EXTENSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_EXTENSION))
#define WEBKIT_WEB_EXTENSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_EXTENSION, WebKitWebExtensionClass))

typedef struct _WebKitWebExtension        WebKitWebExtension;
typedef struct _WebKitWebExtensionClass   WebKitWebExtensionClass;
typedef struct _WebKitWebExtensionPrivate WebKitWebExtensionPrivate;

/**
 * WebKitWebExtensionInitializeFunction:
 * @extension: a #WebKitWebExtension
 *
 * Type definition for a function that will be called to initialize
 * the web extension when the web process starts.
 */
typedef void (* WebKitWebExtensionInitializeFunction) (WebKitWebExtension *extension);

/**
 * WebKitWebExtensionInitializeWithUserDataFunction:
 * @extension: a #WebKitWebExtension
 * @user_data: a #GVariant
 *
 * Type definition for a function that will be called to initialize
 * the web extensions when the web process starts, and which receives
 * as additional argument the user data set with
 * webkit_web_context_set_web_extensions_initialization_user_data().
 *
 * Since: 2.4
 */
typedef void (* WebKitWebExtensionInitializeWithUserDataFunction) (WebKitWebExtension *extension,
                                                                   const GVariant     *user_data);

struct _WebKitWebExtension {
    GObject parent;

    WebKitWebExtensionPrivate *priv;
};

struct _WebKitWebExtensionClass {
    GObjectClass parent_class;
};

WEBKIT_API GType
webkit_web_extension_get_type                       (void);

WEBKIT_API WebKitWebPage *
webkit_web_extension_get_page                       (WebKitWebExtension *extension,
                                                     guint64             page_id);

WEBKIT_API void
webkit_web_extension_send_message_to_context        (WebKitWebExtension *extension,
                                                     WebKitUserMessage  *message,
                                                     GCancellable       *cancellable,
                                                     GAsyncReadyCallback callback,
                                                     gpointer            user_data);

WEBKIT_API WebKitUserMessage *
webkit_web_extension_send_message_to_context_finish (WebKitWebExtension *extension,
                                                     GAsyncResult       *result,
                                                     GError            **error);

G_END_DECLS

#endif
