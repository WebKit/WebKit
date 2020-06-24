/*
 * Copyright (C) 2013 Samsung Electronics Inc. All rights reserved.
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

#ifndef WebKitAuthenticationRequest_h
#define WebKitAuthenticationRequest_h

#include <gtk/gtk.h>
#include <webkit2/WebKitCredential.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_AUTHENTICATION_REQUEST            (webkit_authentication_request_get_type())
#define WEBKIT_AUTHENTICATION_REQUEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_AUTHENTICATION_REQUEST, WebKitAuthenticationRequest))
#define WEBKIT_AUTHENTICATION_REQUEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_AUTHENTICATION_REQUEST, WebKitAuthenticationRequestClass))
#define WEBKIT_IS_AUTHENTICATION_REQUEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_AUTHENTICATION_REQUEST))
#define WEBKIT_IS_AUTHENTICATION_REQUEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_AUTHENTICATION_REQUEST))
#define WEBKIT_AUTHENTICATION_REQUEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_AUTHENTICATION_REQUEST, WebKitAuthenticationRequestClass))

typedef struct _WebKitAuthenticationRequest        WebKitAuthenticationRequest;
typedef struct _WebKitAuthenticationRequestClass   WebKitAuthenticationRequestClass;
typedef struct _WebKitAuthenticationRequestPrivate WebKitAuthenticationRequestPrivate;

struct _WebKitAuthenticationRequest {
    GObject parent;

    /*< private >*/
    WebKitAuthenticationRequestPrivate *priv;
};

struct _WebKitAuthenticationRequestClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

/**
 * WebKitAuthenticationScheme:
 * @WEBKIT_AUTHENTICATION_SCHEME_DEFAULT: The default authentication scheme of WebKit.
 * @WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC: Basic authentication scheme as defined in RFC 2617.
 * @WEBKIT_AUTHENTICATION_SCHEME_HTTP_DIGEST: Digest authentication scheme as defined in RFC 2617.
 * @WEBKIT_AUTHENTICATION_SCHEME_HTML_FORM: HTML Form authentication.
 * @WEBKIT_AUTHENTICATION_SCHEME_NTLM: NTLM Microsoft proprietary authentication scheme.
 * @WEBKIT_AUTHENTICATION_SCHEME_NEGOTIATE: Negotiate (or SPNEGO) authentication scheme as defined in RFC 4559.
 * @WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED: Client Certificate Authentication (see RFC 2246).
 * @WEBKIT_AUTHENTICATION_SCHEME_SERVER_TRUST_EVALUATION_REQUESTED: Server Trust Authentication.
 * @WEBKIT_AUTHENTICATION_SCHEME_UNKNOWN: Authentication scheme unknown.
 *
 * Enum values representing the authentication scheme.
 *
 * Since: 2.2
 */
typedef enum {
    WEBKIT_AUTHENTICATION_SCHEME_DEFAULT = 1,
    WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC = 2,
    WEBKIT_AUTHENTICATION_SCHEME_HTTP_DIGEST = 3,
    WEBKIT_AUTHENTICATION_SCHEME_HTML_FORM = 4,
    WEBKIT_AUTHENTICATION_SCHEME_NTLM = 5,
    WEBKIT_AUTHENTICATION_SCHEME_NEGOTIATE = 6,
    WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED = 7,
    WEBKIT_AUTHENTICATION_SCHEME_SERVER_TRUST_EVALUATION_REQUESTED = 8,
    WEBKIT_AUTHENTICATION_SCHEME_UNKNOWN = 100,
} WebKitAuthenticationScheme;


WEBKIT_API GType
webkit_authentication_request_get_type                (void);

WEBKIT_API gboolean
webkit_authentication_request_can_save_credentials    (WebKitAuthenticationRequest *request);

WEBKIT_API void
webkit_authentication_request_set_can_save_credentials(WebKitAuthenticationRequest *request,
                                                       gboolean                     enabled);

WEBKIT_API WebKitCredential *
webkit_authentication_request_get_proposed_credential (WebKitAuthenticationRequest *request);

WEBKIT_API void
webkit_authentication_request_set_proposed_credential (WebKitAuthenticationRequest *request,
                                                       WebKitCredential            *credential);

WEBKIT_API const gchar *
webkit_authentication_request_get_host                (WebKitAuthenticationRequest *request);

WEBKIT_API guint
webkit_authentication_request_get_port                (WebKitAuthenticationRequest *request);

WEBKIT_API const gchar *
webkit_authentication_request_get_realm               (WebKitAuthenticationRequest *request);

WEBKIT_API WebKitAuthenticationScheme
webkit_authentication_request_get_scheme              (WebKitAuthenticationRequest *request);

WEBKIT_API gboolean
webkit_authentication_request_is_for_proxy            (WebKitAuthenticationRequest *request);

WEBKIT_API gboolean
webkit_authentication_request_is_retry                (WebKitAuthenticationRequest *request);

WEBKIT_API void
webkit_authentication_request_authenticate            (WebKitAuthenticationRequest *request,
                                                       WebKitCredential            *credential);

WEBKIT_API void
webkit_authentication_request_cancel                  (WebKitAuthenticationRequest *request);

G_END_DECLS

#endif
