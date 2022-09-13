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
#include "WebKitOverridingResolver.h"

#include <wtf/HashSet.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

typedef struct {
    GRefPtr<GResolver> wrappedResolver;
    HashSet<String> localhostAliases;

    GRefPtr<GInetAddress> ipv4LoopbackAddress;
    GRefPtr<GInetAddress> ipv6LoopbackAddress;
} WebKitOverridingResolverPrivate;

struct _WebKitOverridingResolver {
    GResolver parentInstance;

    WebKitOverridingResolverPrivate* priv;
};

struct _WebKitOverridingResolverClass {
    GResolverClass parentClass;
};

WEBKIT_DEFINE_TYPE(WebKitOverridingResolver, webkit_overriding_resolver, G_TYPE_RESOLVER)

static GList* createLoobackAddressList(WebKitOverridingResolver* resolver)
{
    GList* list = nullptr;
    list = g_list_append(list, g_object_ref(resolver->priv->ipv4LoopbackAddress.get()));
    list = g_list_append(list, g_object_ref(resolver->priv->ipv6LoopbackAddress.get()));
    return list;
}

static GList* webkitOverridingResolverLookupByName(GResolver* resolver, const char* hostname, GCancellable* cancellable, GError** error)
{
    auto* self = WEBKIT_OVERRIDING_RESOLVER(resolver);

    if (self->priv->localhostAliases.contains(String::fromUTF8(hostname)))
        return createLoobackAddressList(self);

    return g_resolver_lookup_by_name(self->priv->wrappedResolver.get(), hostname, cancellable, error);
}

static void webkitOverridingResolverLookupByNameAsync(GResolver* resolver, const char* hostname, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    auto* self = WEBKIT_OVERRIDING_RESOLVER(resolver);

    if (self->priv->localhostAliases.contains(String::fromUTF8(hostname))) {
        GRefPtr<GTask> task = adoptGRef(g_task_new(resolver, cancellable, callback, userData));
        g_task_return_pointer(task.get(), createLoobackAddressList(self), reinterpret_cast<GDestroyNotify>(g_resolver_free_addresses));
        return;
    }

    return g_resolver_lookup_by_name_async(self->priv->wrappedResolver.get(), hostname, cancellable, callback, userData);
}

static GList* webkitOverridingResolverLookupByNameFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(g_task_is_valid(result, resolver), nullptr);

    return static_cast<GList*>(g_task_propagate_pointer(G_TASK(result), error));
}

#if GLIB_CHECK_VERSION(2, 59, 0)
static GList* createLoobackAddressList(WebKitOverridingResolver* resolver, GResolverNameLookupFlags flags)
{
    GList* list = nullptr;
    if (!(flags & G_RESOLVER_NAME_LOOKUP_FLAGS_IPV6_ONLY))
        list = g_list_append(list, g_object_ref(resolver->priv->ipv4LoopbackAddress.get()));
    if (!(flags & G_RESOLVER_NAME_LOOKUP_FLAGS_IPV4_ONLY))
        list = g_list_append(list, g_object_ref(resolver->priv->ipv4LoopbackAddress.get()));
    return list;
}

static GList* webkitOverridingResolverLookupByNameWithFlags(GResolver* resolver, const char* hostname, GResolverNameLookupFlags flags, GCancellable* cancellable, GError** error)
{
    auto* self = WEBKIT_OVERRIDING_RESOLVER(resolver);

    if (self->priv->localhostAliases.contains(String::fromUTF8(hostname)))
        return createLoobackAddressList(self, flags);

    return g_resolver_lookup_by_name_with_flags(self->priv->wrappedResolver.get(), hostname, flags, cancellable, error);
}

static void webkitOverridingResolverLookupByNameWithFlagsAsync(GResolver* resolver, const gchar* hostname, GResolverNameLookupFlags flags, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    auto* self = WEBKIT_OVERRIDING_RESOLVER(resolver);

    if (self->priv->localhostAliases.contains(String::fromUTF8(hostname))) {
        GRefPtr<GTask> task = adoptGRef(g_task_new(resolver, cancellable, callback, userData));
        g_task_return_pointer(task.get(), createLoobackAddressList(self, flags), reinterpret_cast<GDestroyNotify>(g_resolver_free_addresses));
        return;
    }

    return g_resolver_lookup_by_name_with_flags_async(self->priv->wrappedResolver.get(), hostname, flags, cancellable, callback, userData);
}

static GList* webkitOverridingResolverLookupByNameWithFlagsFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(g_task_is_valid(result, resolver), nullptr);

    return static_cast<GList*>(g_task_propagate_pointer(G_TASK(result), error));
}
#endif // GLIB_CHECK_VERSION(2, 59, 0)

static char* webkitOverridingResolverLookupByAddress(GResolver* resolver, GInetAddress* address, GCancellable* cancellable, GError** error)
{
    return g_resolver_lookup_by_address(WEBKIT_OVERRIDING_RESOLVER(resolver)->priv->wrappedResolver.get(), address, cancellable, error);
}

static void webkitOverridingResolverLookupByAddressAsync(GResolver* resolver, GInetAddress* address, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_resolver_lookup_by_address_async(WEBKIT_OVERRIDING_RESOLVER(resolver)->priv->wrappedResolver.get(), address, cancellable, callback, userData);
}

static char* webkitOverridingResolverLookupByAddressFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    return g_resolver_lookup_by_address_finish(WEBKIT_OVERRIDING_RESOLVER(resolver)->priv->wrappedResolver.get(), result, error);
}

static GList* webkitOverridingResolverLookupRecords(GResolver* resolver, const char* rrname, GResolverRecordType recordType, GCancellable* cancellable, GError** error)
{
    return g_resolver_lookup_records(WEBKIT_OVERRIDING_RESOLVER(resolver)->priv->wrappedResolver.get(), rrname, recordType, cancellable, error);
}

static void webkitOverridingResolverLookupRecordsAsync(GResolver* resolver, const char* rrname, GResolverRecordType recordType, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_resolver_lookup_records_async(WEBKIT_OVERRIDING_RESOLVER(resolver)->priv->wrappedResolver.get(), rrname, recordType, cancellable, callback, userData);
}

static GList* webkitOverridingResolverLookupRecordsFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    return g_resolver_lookup_records_finish(WEBKIT_OVERRIDING_RESOLVER(resolver)->priv->wrappedResolver.get(), result, error);
}

static void webkit_overriding_resolver_class_init(WebKitOverridingResolverClass* klass)
{
    GResolverClass* resolverClass = G_RESOLVER_CLASS(klass);
    resolverClass->lookup_by_name = webkitOverridingResolverLookupByName;
    resolverClass->lookup_by_name_async = webkitOverridingResolverLookupByNameAsync;
    resolverClass->lookup_by_name_finish = webkitOverridingResolverLookupByNameFinish;
#if GLIB_CHECK_VERSION(2, 59, 0)
    resolverClass->lookup_by_name_with_flags = webkitOverridingResolverLookupByNameWithFlags;
    resolverClass->lookup_by_name_with_flags_async = webkitOverridingResolverLookupByNameWithFlagsAsync;
    resolverClass->lookup_by_name_with_flags_finish = webkitOverridingResolverLookupByNameWithFlagsFinish;
#endif
    resolverClass->lookup_by_address = webkitOverridingResolverLookupByAddress;
    resolverClass->lookup_by_address_async = webkitOverridingResolverLookupByAddressAsync;
    resolverClass->lookup_by_address_finish = webkitOverridingResolverLookupByAddressFinish;
    resolverClass->lookup_records = webkitOverridingResolverLookupRecords;
    resolverClass->lookup_records_async = webkitOverridingResolverLookupRecordsAsync;
    resolverClass->lookup_records_finish = webkitOverridingResolverLookupRecordsFinish;
}

GResolver* webkitOverridingResolverNew(GRefPtr<GResolver>&& wrappedResolver, const HashSet<String>& localhostAliases)
{
    g_return_val_if_fail(wrappedResolver, nullptr);

    auto* resolver = WEBKIT_OVERRIDING_RESOLVER(g_object_new(WEBKIT_TYPE_OVERRIDING_RESOLVER, nullptr));
    resolver->priv->ipv4LoopbackAddress = adoptGRef(g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4));
    resolver->priv->ipv6LoopbackAddress = adoptGRef(g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV6));
    resolver->priv->wrappedResolver = WTFMove(wrappedResolver);
    resolver->priv->localhostAliases = localhostAliases;
    return G_RESOLVER(resolver);
}
