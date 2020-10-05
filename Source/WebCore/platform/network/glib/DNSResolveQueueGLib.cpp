/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2012, 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DNSResolveQueueGLib.h"

#if USE(GLIB)

#include <gio/gio.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

// Initially true to ensure prefetch stays disabled until we have proxy settings.
static bool isUsingHttpProxy = true;
static bool isUsingHttpsProxy = true;

static bool didResolveProxy(char** uris)
{
    // We have a list of possible proxies to use for the URI. If the first item in the list is
    // direct:// (the usual case), then the user prefers not to use a proxy. This is similar to
    // resolving hostnames: there could be many possibilities returned in order of preference, and
    // if we're trying to connect we should attempt each one in order, but here we are not trying
    // to connect, merely to decide whether a proxy "should" be used.
    return uris && *uris && strcmp(*uris, "direct://");
}

static void didResolveProxy(GProxyResolver* resolver, GAsyncResult* result, bool* isUsingProxyType, bool* isUsingProxy)
{
    GUniqueOutPtr<GError> error;
    GUniquePtr<char*> uris(g_proxy_resolver_lookup_finish(resolver, result, &error.outPtr()));
    if (error) {
        WTFLogAlways("Error determining system proxy settings: %s", error->message);
        return;
    }

    *isUsingProxyType = didResolveProxy(uris.get());
    *isUsingProxy = isUsingHttpProxy || isUsingHttpsProxy;
}

static void proxyResolvedForHttpUriCallback(GObject* source, GAsyncResult* result, void* userData)
{
    didResolveProxy(G_PROXY_RESOLVER(source), result, &isUsingHttpProxy, static_cast<bool*>(userData));
}

static void proxyResolvedForHttpsUriCallback(GObject* source, GAsyncResult* result, void* userData)
{
    didResolveProxy(G_PROXY_RESOLVER(source), result, &isUsingHttpsProxy, static_cast<bool*>(userData));
}

void DNSResolveQueueGLib::updateIsUsingProxy()
{
    GProxyResolver* resolver = g_proxy_resolver_get_default();
    g_proxy_resolver_lookup_async(resolver, "http://example.com/", nullptr, proxyResolvedForHttpUriCallback, &m_isUsingProxy);
    g_proxy_resolver_lookup_async(resolver, "https://example.com/", nullptr, proxyResolvedForHttpsUriCallback, &m_isUsingProxy);
}

void DNSResolveQueueGLib::platformResolve(const String& hostname)
{
    ASSERT(isMainThread());

    GRefPtr<GResolver> resolver = adoptGRef(g_resolver_get_default());
    g_resolver_lookup_by_name_async(resolver.get(), hostname.utf8().data(), nullptr, [](GObject* resolver, GAsyncResult* result, gpointer) {
        GList* addresses = g_resolver_lookup_by_name_finish(G_RESOLVER(resolver), result, nullptr);
        g_clear_pointer(&addresses, g_resolver_free_addresses);
        DNSResolveQueue::singleton().decrementRequestCount();
    }, nullptr);
}

void DNSResolveQueueGLib::resolve(const String& hostname, uint64_t identifier, DNSCompletionHandler&& completionHandler)
{
    ASSERT(isMainThread());

    GRefPtr<GResolver> resolver = adoptGRef(g_resolver_get_default());
    auto request = makeUnique<DNSResolveQueueGLib::Request>(identifier, WTFMove(completionHandler));
    GRefPtr<GCancellable> cancellable = adoptGRef(g_cancellable_new());
    g_resolver_lookup_by_name_async(resolver.get(), hostname.utf8().data(), cancellable.get(), [](GObject* resolver, GAsyncResult* result, gpointer userData) {
        std::unique_ptr<DNSResolveQueueGLib::Request> request(static_cast<DNSResolveQueueGLib::Request*>(userData));
        GUniqueOutPtr<GError> error;
        GList* addresses = g_resolver_lookup_by_name_finish(G_RESOLVER(resolver), result, &error.outPtr());
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            request->completionHandler(makeUnexpected(DNSError::Cancelled));
            return;
        }

        static_cast<DNSResolveQueueGLib&>(DNSResolveQueue::singleton()).m_requestCancellables.remove(request->identifier);
        if (error) {
            request->completionHandler(makeUnexpected(DNSError::CannotResolve));
            return;
        }

        Vector<WebCore::IPAddress> addressList;
        for (auto* iter = addresses; iter; iter = g_list_next(iter)) {
            auto* address = G_INET_ADDRESS(iter->data);
            switch (g_inet_address_get_family(address)) {
            case G_SOCKET_FAMILY_IPV4: {
                struct in_addr ipv4Address;
                memcpy(&ipv4Address, g_inet_address_to_bytes(address), g_inet_address_get_native_size(address));
                addressList.append(IPAddress(ipv4Address));
                break;
            }
            case G_SOCKET_FAMILY_IPV6: {
                struct in6_addr ipv6Address;
                memcpy(&ipv6Address, g_inet_address_to_bytes(address), g_inet_address_get_native_size(address));
                addressList.append(IPAddress(ipv6Address));
                break;
            }
            case G_SOCKET_FAMILY_INVALID:
            case G_SOCKET_FAMILY_UNIX:
                break;
            }
        }

        if (addressList.isEmpty()) {
            request->completionHandler(makeUnexpected(WebCore::DNSError::CannotResolve));
            return;
        }

        request->completionHandler(WTFMove(addressList));
    }, request.release());

    m_requestCancellables.add(identifier, WTFMove(cancellable));
}

void DNSResolveQueueGLib::stopResolve(uint64_t identifier)
{
    ASSERT(isMainThread());

    if (auto cancellable = m_requestCancellables.take(identifier))
        g_cancellable_cancel(cancellable.get());
}

} // namespace WebCore

#endif // USE(GLIB)
