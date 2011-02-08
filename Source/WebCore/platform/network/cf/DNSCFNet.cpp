/*
 * Copyright (C) 2008 Collin Jackson  <collinj@webkit.org>
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "KURL.h"
#include "Timer.h"
#include <wtf/HashSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(WIN)
#include "LoaderRunLoopCF.h"
#include <CFNetwork/CFNetwork.h>
#endif

#if defined(BUILDING_ON_LEOPARD)
#include <SystemConfiguration/SystemConfiguration.h>
#endif

#ifdef BUILDING_ON_TIGER
// This function is available on Tiger, but not declared in the CFRunLoop.h header on Tiger.
extern "C" CFRunLoopRef CFRunLoopGetMain();
#endif

namespace WebCore {

// When resolve queue is empty, we fire async resolution requests immediately (which is important if the prefetch is triggered by hovering).
// But during page parsing, we should coalesce identical requests to avoid stressing out CFHost.
const int namesToResolveImmediately = 4;

// Coalesce prefetch requests for this long before sending them out.
const double coalesceDelayInSeconds = 1.0;

// Sending many DNS requests at once can overwhelm some gateways. CFHost doesn't currently throttle for us, see <rdar://8105550>.
const int maxSimultaneousRequests = 8;

// For a page has links to many outside sites, it is likely that the system DNS resolver won't be able to cache them all anyway, and we don't want
// to negatively affect other applications' performance by pushing their cached entries out.
// If we end up with lots of names to prefetch, some will be dropped.
const int maxRequestsToQueue = 64;

// If there were queued names that couldn't be sent simultaneously, check the state of resolvers after this delay.
const double retryResolvingInSeconds = 0.1;

static bool proxyIsEnabledInSystemPreferences()
{
    // Don't do DNS prefetch if proxies are involved. For many proxy types, the user agent is never exposed
    // to the IP address during normal operation. Querying an internal DNS server may not help performance,
    // as it doesn't necessarily look up the actual external IP. Also, if DNS returns a fake internal address,
    // local caches may keep it even after re-connecting to another network.

#if !defined(BUILDING_ON_LEOPARD)
    RetainPtr<CFDictionaryRef> proxySettings(AdoptCF, CFNetworkCopySystemProxySettings());
#else
    RetainPtr<CFDictionaryRef> proxySettings(AdoptCF, SCDynamicStoreCopyProxies(0));
#endif
    if (!proxySettings)
        return false;

    static CFURLRef httpCFURL = KURL(ParsedURLString, "http://example.com/").createCFURL();
    static CFURLRef httpsCFURL = KURL(ParsedURLString, "https://example.com/").createCFURL();

    RetainPtr<CFArrayRef> httpProxyArray(AdoptCF, CFNetworkCopyProxiesForURL(httpCFURL, proxySettings.get()));
    RetainPtr<CFArrayRef> httpsProxyArray(AdoptCF, CFNetworkCopyProxiesForURL(httpsCFURL, proxySettings.get()));

    CFIndex httpProxyCount = CFArrayGetCount(httpProxyArray.get());
    CFIndex httpsProxyCount = CFArrayGetCount(httpsProxyArray.get());
    if (httpProxyCount == 1 && CFEqual(CFDictionaryGetValue(static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(httpProxyArray.get(), 0)), kCFProxyTypeKey), kCFProxyTypeNone))
        httpProxyCount = 0;
    if (httpsProxyCount == 1 && CFEqual(CFDictionaryGetValue(static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(httpsProxyArray.get(), 0)), kCFProxyTypeKey), kCFProxyTypeNone))
        httpsProxyCount = 0;

    return httpProxyCount || httpsProxyCount;
}

class DNSResolveQueue : public TimerBase {
public:
    static DNSResolveQueue& shared();
    void add(const String&);
    void decrementRequestCount();

private:
    DNSResolveQueue();

    void resolve(const String&);
    virtual void fired();
    HashSet<String> m_names;
    int m_requestsInFlight;
};

DNSResolveQueue::DNSResolveQueue()
    : m_requestsInFlight(0)
{
}

DNSResolveQueue& DNSResolveQueue::shared()
{
    DEFINE_STATIC_LOCAL(DNSResolveQueue, names, ());
    return names;
}

void DNSResolveQueue::add(const String& name)
{
    // If there are no names queued, and few enough are in flight, resolve immediately (the mouse may be over a link).
    if (!m_names.size()) {
        if (proxyIsEnabledInSystemPreferences())
            return;

        if (atomicIncrement(&m_requestsInFlight) <= namesToResolveImmediately) {
            resolve(name);
            return;
        }
        atomicDecrement(&m_requestsInFlight);
    }

    // It's better to not prefetch some names than to clog the queue.
    // Dropping the newest names, because on a single page, these are likely to be below oldest ones.
    if (m_names.size() < maxRequestsToQueue) {
        m_names.add(name);
        if (!isActive())
            startOneShot(coalesceDelayInSeconds);
    }
}

void DNSResolveQueue::decrementRequestCount()
{
    atomicDecrement(&m_requestsInFlight);
}

void DNSResolveQueue::fired()
{
    if (proxyIsEnabledInSystemPreferences()) {
        m_names.clear();
        return;
    }

    int requestsAllowed = maxSimultaneousRequests - m_requestsInFlight;

    for (; !m_names.isEmpty() && requestsAllowed > 0; --requestsAllowed) {
        atomicIncrement(&m_requestsInFlight);
        HashSet<String>::iterator currentName = m_names.begin();
        resolve(*currentName);
        m_names.remove(currentName);
    }

    if (!m_names.isEmpty())
        startOneShot(retryResolvingInSeconds);
}

static void clientCallback(CFHostRef theHost, CFHostInfoType, const CFStreamError*, void*)
{
    DNSResolveQueue::shared().decrementRequestCount(); // It's ok to call shared() from a secondary thread, the static variable has already been initialized by now.
    CFRelease(theHost);
}

void DNSResolveQueue::resolve(const String& hostname)
{
    ASSERT(isMainThread());

    RetainPtr<CFStringRef> hostnameCF(AdoptCF, hostname.createCFString());
    RetainPtr<CFHostRef> host(AdoptCF, CFHostCreateWithName(0, hostnameCF.get()));
    if (!host) {
        atomicDecrement(&m_requestsInFlight);
        return;
    }
    CFHostClientContext context = { 0, 0, 0, 0, 0 };
    Boolean result = CFHostSetClient(host.get(), clientCallback, &context);
    ASSERT_UNUSED(result, result);
#if !PLATFORM(WIN)
    CFHostScheduleWithRunLoop(host.get(), CFRunLoopGetMain(), kCFRunLoopCommonModes);
#else
    // On Windows, we run a separate thread with CFRunLoop, which is where clientCallback will be called.
    CFHostScheduleWithRunLoop(host.get(), loaderRunLoop(), kCFRunLoopDefaultMode);
#endif
    CFHostStartInfoResolution(host.get(), kCFHostAddresses, 0);
    host.releaseRef(); // The host will be released from clientCallback().
}

void prefetchDNS(const String& hostname)
{
    ASSERT(isMainThread());
    if (hostname.isEmpty())
        return;
    DNSResolveQueue::shared().add(hostname);
}

}
