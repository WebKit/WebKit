/*
 * Copyright (C) 2019 Igalia S.L.
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

#pragma once

#if defined(HAVE_ACCESSIBILITY) && HAVE_ACCESSIBILITY

#include <atk/atk.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_ACCESSIBLE_APPLICATION            (webkit_accessible_application_get_type())
#define WEBKIT_ACCESSIBLE_APPLICATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_ACCESSIBLE_APPLICATION, WebKitAccessibleApplication))
#define WEBKIT_IS_ACCESSIBLE_APPLICATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_ACCESSIBLE_APPLICATION))
#define WEBKIT_ACCESSIBLE_APPLICATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_ACCESSIBLE_APPLICATION, WebKitAccessibleApplicationClass))
#define WEBKIT_IS_ACCESSIBLE_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_ACCESSIBLE_APPLICATION))
#define WEBKIT_ACCESSIBLE_APPLICATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_ACCESSIBLE_APPLICATION, WebKitAccessibleApplicationClass))

typedef struct _WebKitAccessibleApplication        WebKitAccessibleApplication;
typedef struct _WebKitAccessibleApplicationClass   WebKitAccessibleApplicationClass;
typedef struct _WebKitAccessibleApplicationPrivate WebKitAccessibleApplicationPrivate;

struct _WebKitAccessibleApplication {
    AtkObject parent;

    WebKitAccessibleApplicationPrivate *priv;
};

struct _WebKitAccessibleApplicationClass {
    AtkObjectClass parent;
};

GType webkit_accessible_application_get_type(void);

WebKitAccessibleApplication* webkitAccessibleApplicationNew(void);
void webkitAccessibleApplicationSetChild(WebKitAccessibleApplication*, AtkObject*);

G_END_DECLS

#endif // HAVE(ACCESSIBILITY)
