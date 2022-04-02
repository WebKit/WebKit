/*
 * Copyright (C) 2021 Zixing Liu
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

#ifndef WebKitURISchemeResponse_h
#define WebKitURISchemeResponse_h

#include <glib-object.h>
#include <libsoup/soup.h>
#include <webkit2/WebKitDefines.h>
#include <webkit2/WebKitForwardDeclarations.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_URI_SCHEME_RESPONSE            (webkit_uri_scheme_response_get_type())
#define WEBKIT_URI_SCHEME_RESPONSE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_URI_SCHEME_RESPONSE, WebKitURISchemeResponse))
#define WEBKIT_IS_URI_SCHEME_RESPONSE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_URI_SCHEME_RESPONSE))
#define WEBKIT_URI_SCHEME_RESPONSE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_URI_SCHEME_RESPONSE, WebKitURISchemeResponseClass))
#define WEBKIT_IS_URI_SCHEME_RESPONSE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_URI_SCHEME_RESPONSE))
#define WEBKIT_URI_SCHEME_RESPONSE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_URI_SCHEME_RESPONSE, WebKitURISchemeResponseClass))

typedef struct _WebKitURISchemeResponse        WebKitURISchemeResponse;
typedef struct _WebKitURISchemeResponseClass   WebKitURISchemeResponseClass;
typedef struct _WebKitURISchemeResponsePrivate WebKitURISchemeResponsePrivate;

struct _WebKitURISchemeResponse {
    GObject parent;

    /*< private >*/
    WebKitURISchemeResponsePrivate *priv;
};

struct _WebKitURISchemeResponseClass {
    GObjectClass parent_class;

    /*< private >*/
    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_uri_scheme_response_get_type           (void);

WEBKIT_API WebKitURISchemeResponse *
webkit_uri_scheme_response_new                (GInputStream            *input_stream,
                                               gint64                   stream_length);

WEBKIT_API void
webkit_uri_scheme_response_set_status         (WebKitURISchemeResponse *response,
                                               guint                    status_code,
                                               const gchar             *reason_phrase);

WEBKIT_API void
webkit_uri_scheme_response_set_content_type   (WebKitURISchemeResponse *response,
                                               const gchar             *content_type);

WEBKIT_API void
webkit_uri_scheme_response_set_http_headers   (WebKitURISchemeResponse *response,
                                               SoupMessageHeaders      *headers);

G_END_DECLS

#endif
