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

#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_CACHED_RESOLVER            (webkit_cached_resolver_get_type())
#define WEBKIT_CACHED_RESOLVER(object)         (G_TYPE_CHECK_INSTANCE_CAST((object), WEBKIT_TYPE_CACHED_RESOLVER, WebKitCachedResolver))
#define WEBKIT_IS_CACHED_RESOLVER(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), WEBKIT_TYPE_CACHED_RESOLVER))
#define WEBKIT_CACHED_RESOLVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_CACHED_RESOLVER, WebKitCachedResolverClass))
#define WEBKIT_IS_CACHED_RESOLVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_CACHED_RESOLVER))
#define WEBKIT_CACHED_RESOLVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), WEBKIT_TYPE_CACHED_RESOLVER, WebKitCachedResolverClass))

typedef struct _WebKitCachedResolver WebKitCachedResolver;
typedef struct _WebKitCachedResolverClass WebKitCachedResolverClass;

GType webkit_cached_resolver_get_type(void);

GResolver* webkitCachedResolverNew(GRefPtr<GResolver>&&);

G_END_DECLS
