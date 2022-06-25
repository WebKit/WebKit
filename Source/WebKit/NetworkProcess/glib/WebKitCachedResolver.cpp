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

#include "config.h"
#include "WebKitCachedResolver.h"

#include "DNSCache.h"
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

typedef struct {
    GRefPtr<GResolver> wrappedResolver;
    DNSCache cache;
} WebKitCachedResolverPrivate;

struct _WebKitCachedResolver {
    GResolver parentInstance;

    WebKitCachedResolverPrivate* priv;
};

struct _WebKitCachedResolverClass {
    GResolverClass parentClass;
};

WEBKIT_DEFINE_TYPE(WebKitCachedResolver, webkit_cached_resolver, G_TYPE_RESOLVER)

static GList* addressListVectorToGList(const Vector<GRefPtr<GInetAddress>>& addressList)
{
    GList* returnValue = nullptr;
    for (const auto& address : addressList)
        returnValue = g_list_prepend(returnValue, g_object_ref(address.get()));
    return g_list_reverse(returnValue);
}

static Vector<GRefPtr<GInetAddress>> addressListGListToVector(GList* addressList)
{
    Vector<GRefPtr<GInetAddress>> returnValue;
    for (GList* it = addressList; it && it->data; it = g_list_next(it))
        returnValue.append(G_INET_ADDRESS(it->data));
    return returnValue;
}

struct LookupAsyncData {
    CString hostname;
#if GLIB_CHECK_VERSION(2, 59, 0)
    DNSCache::Type dnsCacheType { DNSCache::Type::Default };
#endif
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(LookupAsyncData)

static GList* webkitCachedResolverLookupByName(GResolver* resolver, const char* hostname, GCancellable* cancellable, GError** error)
{
    auto* priv = WEBKIT_CACHED_RESOLVER(resolver)->priv;
    auto addressList = priv->cache.lookup(hostname);
    if (addressList)
        return addressListVectorToGList(addressList.value());

    auto* returnValue = g_resolver_lookup_by_name(priv->wrappedResolver.get(), hostname, cancellable, error);
    if (returnValue)
        priv->cache.update(hostname, addressListGListToVector(returnValue));
    return returnValue;
}

static void webkitCachedResolverLookupByNameAsync(GResolver* resolver, const char* hostname, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    GRefPtr<GTask> task = adoptGRef(g_task_new(resolver, cancellable, callback, userData));
    auto* priv = WEBKIT_CACHED_RESOLVER(resolver)->priv;
    auto addressList = priv->cache.lookup(hostname);
    if (addressList) {
        g_task_return_pointer(task.get(), addressListVectorToGList(addressList.value()), reinterpret_cast<GDestroyNotify>(g_resolver_free_addresses));
        return;
    }

    auto* asyncData = createLookupAsyncData();
    asyncData->hostname = hostname;
    g_task_set_task_data(task.get(), asyncData, reinterpret_cast<GDestroyNotify>(destroyLookupAsyncData));
    g_resolver_lookup_by_name_async(priv->wrappedResolver.get(), hostname, cancellable, [](GObject* resolver, GAsyncResult* result, gpointer userData) {
        GRefPtr<GTask> task = adoptGRef(G_TASK(userData));
        GUniqueOutPtr<GError> error;
        if (auto* addressList = g_resolver_lookup_by_name_finish(G_RESOLVER(resolver), result, &error.outPtr())) {
            auto* priv = WEBKIT_CACHED_RESOLVER(g_task_get_source_object(task.get()))->priv;
            auto* asyncData = static_cast<LookupAsyncData*>(g_task_get_task_data(task.get()));
            priv->cache.update(asyncData->hostname, addressListGListToVector(addressList));
            g_task_return_pointer(task.get(), addressList, reinterpret_cast<GDestroyNotify>(g_resolver_free_addresses));
        } else
            g_task_return_error(task.get(), error.release());
    }, task.leakRef());
}

static GList* webkitCachedResolverLookupByNameFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(g_task_is_valid(result, resolver), nullptr);

    return static_cast<GList*>(g_task_propagate_pointer(G_TASK(result), error));
}

#if GLIB_CHECK_VERSION(2, 59, 0)
static inline DNSCache::Type dnsCacheType(GResolverNameLookupFlags flags)
{
    // A cache is kept for each type of response to avoid the overcomplication of combining or filtering results.
    if (flags & G_RESOLVER_NAME_LOOKUP_FLAGS_IPV4_ONLY)
        return DNSCache::Type::IPv4Only;

    if (flags & G_RESOLVER_NAME_LOOKUP_FLAGS_IPV6_ONLY)
        return DNSCache::Type::IPv6Only;

    return DNSCache::Type::Default;
}

static GList* webkitCachedResolverLookupByNameWithFlags(GResolver* resolver, const char* hostname, GResolverNameLookupFlags flags, GCancellable* cancellable, GError** error)
{
    auto* priv = WEBKIT_CACHED_RESOLVER(resolver)->priv;
    auto cacheType = dnsCacheType(flags);
    auto addressList = priv->cache.lookup(hostname, cacheType);
    if (addressList)
        return addressListVectorToGList(addressList.value());

    auto* returnValue = g_resolver_lookup_by_name_with_flags(priv->wrappedResolver.get(), hostname, flags, cancellable, error);
    if (returnValue)
        priv->cache.update(hostname, addressListGListToVector(returnValue), cacheType);
    return returnValue;
}

static void webkitCachedResolverLookupByNameWithFlagsAsync(GResolver* resolver, const gchar* hostname, GResolverNameLookupFlags flags, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    GRefPtr<GTask> task = adoptGRef(g_task_new(resolver, cancellable, callback, userData));
    auto* priv = WEBKIT_CACHED_RESOLVER(resolver)->priv;
    auto cacheType = dnsCacheType(flags);
    auto addressList = priv->cache.lookup(hostname, cacheType);
    if (addressList) {
        g_task_return_pointer(task.get(), addressListVectorToGList(addressList.value()), reinterpret_cast<GDestroyNotify>(g_resolver_free_addresses));
        return;
    }

    auto* asyncData = createLookupAsyncData();
    asyncData->hostname = hostname;
    asyncData->dnsCacheType = cacheType;
    g_task_set_task_data(task.get(), asyncData, reinterpret_cast<GDestroyNotify>(destroyLookupAsyncData));
    g_resolver_lookup_by_name_with_flags_async(priv->wrappedResolver.get(), hostname, flags, cancellable, [](GObject* resolver, GAsyncResult* result, gpointer userData) {
        GRefPtr<GTask> task = adoptGRef(G_TASK(userData));
        GUniqueOutPtr<GError> error;
        if (auto* addressList = g_resolver_lookup_by_name_with_flags_finish(G_RESOLVER(resolver), result, &error.outPtr())) {
            auto* priv = WEBKIT_CACHED_RESOLVER(g_task_get_source_object(task.get()))->priv;
            auto* asyncData = static_cast<LookupAsyncData*>(g_task_get_task_data(task.get()));
            priv->cache.update(asyncData->hostname, addressListGListToVector(addressList), asyncData->dnsCacheType);
            g_task_return_pointer(task.get(), addressList, reinterpret_cast<GDestroyNotify>(g_resolver_free_addresses));
        } else
            g_task_return_error(task.get(), error.release());
    }, task.leakRef());
}

static GList* webkitCachedResolverLookupByNameWithFlagsFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(g_task_is_valid(result, resolver), nullptr);

    return static_cast<GList*>(g_task_propagate_pointer(G_TASK(result), error));
}
#endif // GLIB_CHECK_VERSION(2, 59, 0)

static char* webkitCachedResolverLookupByAddress(GResolver* resolver, GInetAddress* address, GCancellable* cancellable, GError** error)
{
    return g_resolver_lookup_by_address(WEBKIT_CACHED_RESOLVER(resolver)->priv->wrappedResolver.get(), address, cancellable, error);
}

static void webkitCachedResolverLookupByAddressAsync(GResolver* resolver, GInetAddress* address, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_resolver_lookup_by_address_async(WEBKIT_CACHED_RESOLVER(resolver)->priv->wrappedResolver.get(), address, cancellable, callback, userData);
}

static char* webkitCachedResolverLookupByAddressFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    return g_resolver_lookup_by_address_finish(WEBKIT_CACHED_RESOLVER(resolver)->priv->wrappedResolver.get(), result, error);
}

static GList* webkitCachedResolverLookupRecords(GResolver* resolver, const char* rrname, GResolverRecordType recordType, GCancellable* cancellable, GError** error)
{
    return g_resolver_lookup_records(WEBKIT_CACHED_RESOLVER(resolver)->priv->wrappedResolver.get(), rrname, recordType, cancellable, error);
}

static void webkitCachedResolverLookupRecordsAsync(GResolver* resolver, const char* rrname, GResolverRecordType recordType, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_resolver_lookup_records_async(WEBKIT_CACHED_RESOLVER(resolver)->priv->wrappedResolver.get(), rrname, recordType, cancellable, callback, userData);
}

static GList* webkitCachedResolverLookupRecordsFinish(GResolver* resolver, GAsyncResult* result, GError** error)
{
    return g_resolver_lookup_records_finish(WEBKIT_CACHED_RESOLVER(resolver)->priv->wrappedResolver.get(), result, error);
}

static void webkitCachedResolverReload(GResolver* resolver)
{
    WEBKIT_CACHED_RESOLVER(resolver)->priv->cache.clear();
}

static void webkit_cached_resolver_class_init(WebKitCachedResolverClass* klass)
{
    GResolverClass* resolverClass = G_RESOLVER_CLASS(klass);
    resolverClass->lookup_by_name = webkitCachedResolverLookupByName;
    resolverClass->lookup_by_name_async = webkitCachedResolverLookupByNameAsync;
    resolverClass->lookup_by_name_finish = webkitCachedResolverLookupByNameFinish;
#if GLIB_CHECK_VERSION(2, 59, 0)
    resolverClass->lookup_by_name_with_flags = webkitCachedResolverLookupByNameWithFlags;
    resolverClass->lookup_by_name_with_flags_async = webkitCachedResolverLookupByNameWithFlagsAsync;
    resolverClass->lookup_by_name_with_flags_finish = webkitCachedResolverLookupByNameWithFlagsFinish;
#endif
    resolverClass->lookup_by_address = webkitCachedResolverLookupByAddress;
    resolverClass->lookup_by_address_async = webkitCachedResolverLookupByAddressAsync;
    resolverClass->lookup_by_address_finish = webkitCachedResolverLookupByAddressFinish;
    resolverClass->lookup_records = webkitCachedResolverLookupRecords;
    resolverClass->lookup_records_async = webkitCachedResolverLookupRecordsAsync;
    resolverClass->lookup_records_finish = webkitCachedResolverLookupRecordsFinish;
    resolverClass->reload = webkitCachedResolverReload;
}

GResolver* webkitCachedResolverNew(GRefPtr<GResolver>&& wrappedResolver)
{
    g_return_val_if_fail(wrappedResolver, nullptr);

    auto* resolver = WEBKIT_CACHED_RESOLVER(g_object_new(WEBKIT_TYPE_CACHED_RESOLVER, nullptr));
    resolver->priv->wrappedResolver = WTFMove(wrappedResolver);
    return G_RESOLVER(resolver);
}
