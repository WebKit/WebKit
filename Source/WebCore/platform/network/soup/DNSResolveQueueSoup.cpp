/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2009, 2012 Igalia S.L.
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
#include "DNSResolveQueueSoup.h"

#if USE(SOUP)

#include "NetworkStorageSession.h"
#include "SoupNetworkSession.h"
#include <libsoup/soup.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
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

Function<NetworkStorageSession&()>& globalDefaultNetworkStorageSessionAccessor()
{
    static NeverDestroyed<Function<NetworkStorageSession&()>> accessor;
    return accessor.get();
}

void DNSResolveQueueSoup::setGlobalDefaultNetworkStorageSessionAccessor(Function<NetworkStorageSession&()>&& accessor)
{
    globalDefaultNetworkStorageSessionAccessor() = WTFMove(accessor);
}

void DNSResolveQueueSoup::updateIsUsingProxy()
{
    GRefPtr<GProxyResolver> resolver;
    g_object_get(globalDefaultNetworkStorageSessionAccessor()().soupNetworkSession().soupSession(), "proxy-resolver", &resolver.outPtr(), nullptr);
    ASSERT(resolver);

    g_proxy_resolver_lookup_async(resolver.get(), "http://example.com/", nullptr, proxyResolvedForHttpUriCallback, &m_isUsingProxy);
    g_proxy_resolver_lookup_async(resolver.get(), "https://example.com/", nullptr, proxyResolvedForHttpsUriCallback, &m_isUsingProxy);
}

static void resolvedCallback(SoupAddress*, guint, void*)
{
    DNSResolveQueue::singleton().decrementRequestCount();
}

static void resolvedWithObserverCallback(SoupAddress* address, guint status, void* data)
{
    ASSERT(data);
    auto* resolveQueue = static_cast<DNSResolveQueueSoup*>(data);

    uint64_t identifier = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(address), "identifier"));

    auto completionAndCancelHandlers = resolveQueue->takeCompletionAndCancelHandlers(identifier);

    if (!completionAndCancelHandlers)
        return;

    auto completionHandler = WTFMove(completionAndCancelHandlers.get()->first);

    if (status != SOUP_STATUS_OK) {
        DNSError error = DNSError::Unknown;

        switch (status) {
        case SOUP_STATUS_CANT_RESOLVE:
            error = DNSError::CannotResolve;
            break;
        case SOUP_STATUS_CANCELLED:
            error = DNSError::Cancelled;
            break;
        case SOUP_STATUS_OK:
        default:
            ASSERT_NOT_REACHED();
        };

        completionHandler(makeUnexpected(error));
        return;
    }

    if (!soup_address_is_resolved(address)) {
        completionHandler(makeUnexpected(DNSError::Unknown));
        return;
    }

    Vector<WebCore::IPAddress> addresses;
    addresses.reserveInitialCapacity(1);
    int len;
    auto* ipAddress = reinterpret_cast<const struct sockaddr_in*>(soup_address_get_sockaddr(address, &len));
    for (unsigned i = 0; i < sizeof(*ipAddress) / len; i++)
        addresses.uncheckedAppend(WebCore::IPAddress(ipAddress[i]));

    completionHandler(addresses);
}

std::unique_ptr<DNSResolveQueueSoup::CompletionAndCancelHandlers> DNSResolveQueueSoup::takeCompletionAndCancelHandlers(uint64_t identifier)
{
    ASSERT(isMainThread());

    auto completionAndCancelHandlers = m_completionAndCancelHandlers.take(identifier);

    if (!completionAndCancelHandlers)
        return nullptr;

    return WTFMove(completionAndCancelHandlers);
}

void DNSResolveQueueSoup::removeCancelAndCompletionHandler(uint64_t identifier)
{
    ASSERT(isMainThread());

    m_completionAndCancelHandlers.remove(identifier);
}

void DNSResolveQueueSoup::platformResolve(const String& hostname)
{
    ASSERT(isMainThread());

    soup_session_prefetch_dns(globalDefaultNetworkStorageSessionAccessor()().soupNetworkSession().soupSession(), hostname.utf8().data(), nullptr, resolvedCallback, nullptr);
}

void DNSResolveQueueSoup::resolve(const String& hostname, uint64_t identifier, DNSCompletionHandler&& completionHandler)
{
    ASSERT(isMainThread());

    auto address = adoptGRef(soup_address_new(hostname.utf8().data(), 0));
    auto cancellable = adoptGRef(g_cancellable_new());
    soup_address_resolve_async(address.get(), soup_session_get_async_context(WebCore::globalDefaultNetworkStorageSessionAccessor()().soupNetworkSession().soupSession()), cancellable.get(), resolvedWithObserverCallback, this);

    g_object_set_data(G_OBJECT(address.get()), "identifier", GUINT_TO_POINTER(identifier));

    m_completionAndCancelHandlers.add(identifier, std::make_unique<DNSResolveQueueSoup::CompletionAndCancelHandlers>(WTFMove(completionHandler), WTFMove(cancellable)));
}

void DNSResolveQueueSoup::stopResolve(uint64_t identifier)
{
    ASSERT(isMainThread());

    if (auto completionAndCancelHandler = m_completionAndCancelHandlers.take(identifier)) {
        g_cancellable_cancel(completionAndCancelHandler.get()->second.get());
        completionAndCancelHandler.get()->first(makeUnexpected(DNSError::Cancelled));
    }
}

}

#endif
