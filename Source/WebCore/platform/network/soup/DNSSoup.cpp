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
#include "DNS.h"
#include "DNSResolveQueue.h"

#if USE(SOUP)

#include "NetworkStorageSession.h"
#include "SoupNetworkSession.h"
#include <libsoup/soup.h>
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

void DNSResolveQueue::updateIsUsingProxy()
{
    GRefPtr<GProxyResolver> resolver;
    g_object_get(NetworkStorageSession::defaultStorageSession().getOrCreateSoupNetworkSession().soupSession(), "proxy-resolver", &resolver.outPtr(), nullptr);
    ASSERT(resolver);

    g_proxy_resolver_lookup_async(resolver.get(), "http://example.com/", nullptr, proxyResolvedForHttpUriCallback, &m_isUsingProxy);
    g_proxy_resolver_lookup_async(resolver.get(), "https://example.com/", nullptr, proxyResolvedForHttpsUriCallback, &m_isUsingProxy);
}

static void resolvedCallback(SoupAddress*, guint, void*)
{
    DNSResolveQueue::singleton().decrementRequestCount();
}

void DNSResolveQueue::platformResolve(const String& hostname)
{
    ASSERT(isMainThread());

    soup_session_prefetch_dns(NetworkStorageSession::defaultStorageSession().getOrCreateSoupNetworkSession().soupSession(), hostname.utf8().data(), nullptr, resolvedCallback, nullptr);
}

void prefetchDNS(const String& hostname)
{
    ASSERT(isMainThread());
    if (hostname.isEmpty())
        return;

    DNSResolveQueue::singleton().add(hostname);
}

}

#endif
