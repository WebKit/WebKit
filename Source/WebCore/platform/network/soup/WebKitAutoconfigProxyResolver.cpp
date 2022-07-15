/*
 * Copyright (C) 2022 Igalia S.L.
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

#include "config.h"
#include "WebKitAutoconfigProxyResolver.h"

#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

struct _WebKitAutoconfigProxyResolverPrivate {
    GRefPtr<GDBusProxy> pacRunner;
    CString autoconfigURL;
};

static void webkitAutoconfigProxyResolverInterfaceInit(GProxyResolverInterface*);

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitAutoconfigProxyResolver, webkit_autoconfig_proxy_resolver, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE(G_TYPE_PROXY_RESOLVER, webkitAutoconfigProxyResolverInterfaceInit))

static void webkit_autoconfig_proxy_resolver_class_init(WebKitAutoconfigProxyResolverClass*)
{
}

GRefPtr<GProxyResolver> webkitAutoconfigProxyResolverNew(const CString& autoconfigURL)
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GDBusProxy> pacRunner = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
        static_cast<GDBusProxyFlags>(G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES | G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
        nullptr, "org.gtk.GLib.PACRunner", "/org/gtk/GLib/PACRunner", "org.gtk.GLib.PACRunner", nullptr, &error.outPtr()));
    if (!pacRunner) {
        g_warning("Could not start proxy autoconfiguration helper: %s\n", error->message);
        return nullptr;
    }

    auto* proxyResolver = WEBKIT_AUTOCONFIG_PROXY_RESOLVER(g_object_new(WEBKIT_TYPE_AUTOCONFIG_PROXY_RESOLVER, nullptr));
    proxyResolver->priv->pacRunner = WTFMove(pacRunner);
    proxyResolver->priv->autoconfigURL = autoconfigURL;

    return adoptGRef(G_PROXY_RESOLVER(proxyResolver));
}

static gchar** webkitAutoconfigProxyResolverLookup(GProxyResolver* proxyResolver, const char* uri, GCancellable* cancellable, GError** error)
{
    auto* priv = WEBKIT_AUTOCONFIG_PROXY_RESOLVER(proxyResolver)->priv;
    GRefPtr<GVariant> variant = adoptGRef(g_dbus_proxy_call_sync(priv->pacRunner.get(), "Lookup", g_variant_new("(ss)", priv->autoconfigURL.data(), uri),
        G_DBUS_CALL_FLAGS_NONE, -1, cancellable, error));
    if (!variant)
        return nullptr;

    gchar** proxies;
    g_variant_get(variant.get(), "(^as)", &proxies);
    return proxies;
}

static void webkitAutoconfigProxyResolverLookupAsync(GProxyResolver* proxyResolver, const char* uri, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    GTask* task = g_task_new(proxyResolver, cancellable, callback, userData);
    auto* priv = WEBKIT_AUTOCONFIG_PROXY_RESOLVER(proxyResolver)->priv;
    g_dbus_proxy_call(priv->pacRunner.get(), "Lookup", g_variant_new("(ss)", priv->autoconfigURL.data(), uri), G_DBUS_CALL_FLAGS_NONE, -1, cancellable,
        [](GObject* source, GAsyncResult* result, gpointer userData) {
            GRefPtr<GTask> task = adoptGRef(G_TASK(userData));
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> variant = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(source), result, &error.outPtr()));
            if (variant) {
                gchar** proxies;
                g_variant_get(variant.get(), "(^as)", &proxies);
                g_task_return_pointer(task.get(), proxies, reinterpret_cast<GDestroyNotify>(g_strfreev));
            } else
                g_task_return_error(task.get(), error.release());
        }, task);
}

static gchar** webkitAutoconfigProxyResolverLookupFinish(GProxyResolver*, GAsyncResult* result, GError** error)
{
    return static_cast<char**>(g_task_propagate_pointer(G_TASK(result), error));
}

static void webkitAutoconfigProxyResolverInterfaceInit(GProxyResolverInterface* interface)
{
    interface->lookup = webkitAutoconfigProxyResolverLookup;
    interface->lookup_async = webkitAutoconfigProxyResolverLookupAsync;
    interface->lookup_finish = webkitAutoconfigProxyResolverLookupFinish;
}
