/*
 * Copyright (C) 2018 Igalia S.L.
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

#if !defined(__WEBKIT_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit.h> can be included directly."
#endif

#ifndef WebKitUserContentFilterStore_h
#define WebKitUserContentFilterStore_h

#include <gio/gio.h>
#include <wpe/WebKitDefines.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_USER_CONTENT_FILTER_STORE            (webkit_user_content_filter_store_get_type())
#define WEBKIT_USER_CONTENT_FILTER_STORE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_USER_CONTENT_FILTER_STORE, WebKitUserContentFilterStore))
#define WEBKIT_IS_USER_CONTENT_FILTER_STORE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_USER_CONTENT_FILTER_STORE))
#define WEBKIT_USER_CONTENT_FILTER_STORE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_USER_CONTENT_FILTER_STORE, WebKitUserContentFilterStoreClass))
#define WEBKIT_IS_USER_CONTENT_FILTER_STORE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_USER_CONTENT_FILTER_STORE))
#define WEBKIT_USER_CONTENT_FILTER_STORE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_USER_CONTENT_FILTER_STORE, WebKitUserContentFilterStoreClass))

typedef struct _WebKitUserContentFilterStore        WebKitUserContentFilterStore;
typedef struct _WebKitUserContentFilterStoreClass   WebKitUserContentFilterStoreClass;
typedef struct _WebKitUserContentFilterStorePrivate WebKitUserContentFilterStorePrivate;

typedef struct _WebKitUserContentFilter             WebKitUserContentFilter;

struct _WebKitUserContentFilterStore {
    GObject parent;

    /*< private >*/
    WebKitUserContentFilterStorePrivate *priv;
};

struct _WebKitUserContentFilterStoreClass {
    GObjectClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};


WEBKIT_API GType
webkit_user_content_filter_store_get_type                 (void);

WEBKIT_API WebKitUserContentFilterStore *
webkit_user_content_filter_store_new                      (const gchar                  *storage_path);

WEBKIT_API const gchar *
webkit_user_content_filter_store_get_path                 (WebKitUserContentFilterStore *store);

WEBKIT_API void
webkit_user_content_filter_store_save                     (WebKitUserContentFilterStore *store,
                                                           const gchar                  *identifier,
                                                           GBytes                       *source,
                                                           GCancellable                 *cancellable,
                                                           GAsyncReadyCallback           callback,
                                                           gpointer                      user_data);

WEBKIT_API WebKitUserContentFilter *
webkit_user_content_filter_store_save_finish              (WebKitUserContentFilterStore *store,
                                                           GAsyncResult                 *result,
                                                           GError                      **error);

WEBKIT_API void
webkit_user_content_filter_store_save_from_file           (WebKitUserContentFilterStore *store,
                                                           const gchar                  *identifier,
                                                           GFile                        *file,
                                                           GCancellable                 *cancellable,
                                                           GAsyncReadyCallback           callback,
                                                           gpointer                      user_data);

WEBKIT_API WebKitUserContentFilter *
webkit_user_content_filter_store_save_from_file_finish    (WebKitUserContentFilterStore *store,
                                                           GAsyncResult                 *result,
                                                           GError                      **error);

WEBKIT_API void
webkit_user_content_filter_store_remove                   (WebKitUserContentFilterStore *store,
                                                           const gchar                  *identifier,
                                                           GCancellable                 *cancellable,
                                                           GAsyncReadyCallback           callback,
                                                           gpointer                      user_data);

WEBKIT_API gboolean
webkit_user_content_filter_store_remove_finish            (WebKitUserContentFilterStore *store,
                                                           GAsyncResult                 *result,
                                                           GError                      **error);

WEBKIT_API void
webkit_user_content_filter_store_load                     (WebKitUserContentFilterStore *store,
                                                           const gchar                  *identifier,
                                                           GCancellable                 *cancellable,
                                                           GAsyncReadyCallback           callback,
                                                           gpointer                      user_data);

WEBKIT_API WebKitUserContentFilter *
webkit_user_content_filter_store_load_finish              (WebKitUserContentFilterStore *store,
                                                           GAsyncResult                 *result,
                                                           GError                      **error);

WEBKIT_API void
webkit_user_content_filter_store_fetch_identifiers        (WebKitUserContentFilterStore *store,
                                                           GCancellable                 *cancellable,
                                                           GAsyncReadyCallback           callback,
                                                           gpointer                      user_data);

WEBKIT_API gchar**
webkit_user_content_filter_store_fetch_identifiers_finish (WebKitUserContentFilterStore *store,
                                                           GAsyncResult                 *result);

G_END_DECLS

#endif /* !WebKitUserContentFilterStore_h */
