/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "config.h"
#include "WebKitWebViewBackend.h"

#include "WebKitWebViewBackendPrivate.h"

/**
 * SECTION: WebKitWebViewBackend
 * @Short_description: A web view backend
 * @Title: WebKitWebViewBackend
 * @See_also: #WebKitWebView.
 *
 * A WebKitWebViewBackend is a boxed type wrapping a WPE backend used to create a
 * #WebKitWebView. A WebKitWebViewBackend is created with webkit_web_view_backend_new()
 * and it should be passed to a WebKitWebView constructor that will take the ownership.
 *
 * Since: 2.20
 */

struct _WebKitWebViewBackend {
    _WebKitWebViewBackend(struct wpe_view_backend* backend, GDestroyNotify notifyCallback, gpointer notifyCallbackData)
        : backend(backend)
        , notifyCallback(notifyCallback)
        , notifyCallbackData(notifyCallbackData)
    {
        ASSERT(backend);
        ASSERT(notifyCallback);
        ASSERT(notifyCallbackData);
    }

    ~_WebKitWebViewBackend()
    {
        notifyCallback(notifyCallbackData);
    }

    struct wpe_view_backend* backend;
    GDestroyNotify notifyCallback;
    gpointer notifyCallbackData;
    int referenceCount { 1 };
};

static WebKitWebViewBackend* webkitWebViewBackendRef(WebKitWebViewBackend* viewBackend)
{
    ASSERT(viewBackend);
    g_atomic_int_inc(&viewBackend->referenceCount);
    return viewBackend;
}

G_DEFINE_BOXED_TYPE(WebKitWebViewBackend, webkit_web_view_backend, webkitWebViewBackendRef, webkitWebViewBackendUnref)

void webkitWebViewBackendUnref(WebKitWebViewBackend* viewBackend)
{
    ASSERT(viewBackend);
    if (g_atomic_int_dec_and_test(&viewBackend->referenceCount)) {
        viewBackend->~WebKitWebViewBackend();
        fastFree(viewBackend);
    }
}

/**
 * webkit_web_view_backend_new:
 * @backend: (transfer full): a #wpe_view_backend
 * @notify: (nullable): a #GDestroyNotify, or %NULL
 * @user_data: user data to pass to @notify
 *
 * Create a new #WebKitWebViewBackend for the given WPE @backend. You can pass a #GDestroyNotify
 * that will be called when the object is destroyed passing @user_data as the argument. If @notify
 * is %NULL, wpe_view_backend_destroy() will be used with @backend as argument.
 * The returned #WebKitWebViewBackend should never be freed by the user; it must be passed to a
 * #WebKitWebView constructor that will take the ownership.
 *
 * Returns: a newly created #WebKitWebViewBackend
 *
 * Since: 2.20
 */
WebKitWebViewBackend* webkit_web_view_backend_new(struct wpe_view_backend* backend, GDestroyNotify notify, gpointer userData)
{
    g_return_val_if_fail(backend, nullptr);

    auto* viewBackend = static_cast<WebKitWebViewBackend*>(fastMalloc(sizeof(WebKitWebViewBackend)));
    new (viewBackend) WebKitWebViewBackend(backend, notify ? notify : reinterpret_cast<GDestroyNotify>(wpe_view_backend_destroy), notify ? userData : backend);
    return viewBackend;
}

/**
 * webkit_web_view_backend_get_wpe_backend:
 * @view_backend: a #WebKitWebViewBackend
 *
 * Get the WPE backend of @view_backend
 *
 * Returns: (transfer none): the #wpe_view_backend
 *
 * Since: 2.20
 */
struct wpe_view_backend* webkit_web_view_backend_get_wpe_backend(WebKitWebViewBackend* viewBackend)
{
    g_return_val_if_fail(viewBackend, nullptr);
    return viewBackend->backend;
}

namespace WTF {

template <> WebKitWebViewBackend* refGPtr(WebKitWebViewBackend* ptr)
{
    if (ptr)
        webkitWebViewBackendRef(ptr);
    return ptr;
}

template <> void derefGPtr(WebKitWebViewBackend* ptr)
{
    if (ptr)
        webkitWebViewBackendUnref(ptr);
}

}
