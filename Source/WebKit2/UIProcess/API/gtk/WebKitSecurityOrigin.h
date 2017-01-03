/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitSecurityOrigin_h
#define WebKitSecurityOrigin_h

#include <glib-object.h>
#include <webkit2/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_SECURITY_ORIGIN (webkit_security_origin_get_type())

typedef struct _WebKitSecurityOrigin WebKitSecurityOrigin;

WEBKIT_API GType
webkit_security_origin_get_type     (void);

WEBKIT_API WebKitSecurityOrigin *
webkit_security_origin_new          (const gchar          *protocol,
                                     const gchar          *host,
                                     guint16               port);

WEBKIT_API WebKitSecurityOrigin *
webkit_security_origin_new_for_uri  (const gchar          *uri);

WEBKIT_API WebKitSecurityOrigin *
webkit_security_origin_ref          (WebKitSecurityOrigin *origin);

WEBKIT_API void
webkit_security_origin_unref        (WebKitSecurityOrigin *origin);

WEBKIT_API const gchar *
webkit_security_origin_get_protocol (WebKitSecurityOrigin *origin);

WEBKIT_API const gchar *
webkit_security_origin_get_host     (WebKitSecurityOrigin *origin);

WEBKIT_API guint16
webkit_security_origin_get_port     (WebKitSecurityOrigin *origin);

WEBKIT_API gboolean
webkit_security_origin_is_opaque    (WebKitSecurityOrigin *origin);

WEBKIT_API gchar *
webkit_security_origin_to_string    (WebKitSecurityOrigin *origin);

G_END_DECLS

#endif /* WebKitSecurityOrigin_h */
