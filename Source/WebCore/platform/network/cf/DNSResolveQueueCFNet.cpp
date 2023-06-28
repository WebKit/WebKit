/*
 * Copyright (C) 2008 Collin Jackson  <collinj@webkit.org>
 * Copyright (C) 2009-2023 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Igalia S.L.
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
#include "DNSResolveQueueCFNet.h"

#include "Timer.h"
#include <dns_sd.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/text/StringHash.h>

#if PLATFORM(IOS_FAMILY)
#include <CFNetwork/CFNetwork.h>
#endif

namespace WebCore {

DNSResolveQueueCFNet::DNSResolveQueueCFNet() = default;

DNSResolveQueueCFNet::~DNSResolveQueueCFNet() = default;

void DNSResolveQueueCFNet::updateIsUsingProxy()
{
    RetainPtr<CFDictionaryRef> proxySettings = adoptCF(CFNetworkCopySystemProxySettings());
    if (!proxySettings) {
        m_isUsingProxy = false;
        return;
    }

    RetainPtr<CFURLRef> httpCFURL = URL({ }, "http://example.com/"_s).createCFURL();
    RetainPtr<CFURLRef> httpsCFURL = URL({ }, "https://example.com/"_s).createCFURL();

    RetainPtr<CFArrayRef> httpProxyArray = adoptCF(CFNetworkCopyProxiesForURL(httpCFURL.get(), proxySettings.get()));
    RetainPtr<CFArrayRef> httpsProxyArray = adoptCF(CFNetworkCopyProxiesForURL(httpsCFURL.get(), proxySettings.get()));

    CFIndex httpProxyCount = CFArrayGetCount(httpProxyArray.get());
    CFIndex httpsProxyCount = CFArrayGetCount(httpsProxyArray.get());
    if (httpProxyCount == 1 && CFEqual(CFDictionaryGetValue(static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(httpProxyArray.get(), 0)), kCFProxyTypeKey), kCFProxyTypeNone))
        httpProxyCount = 0;
    if (httpsProxyCount == 1 && CFEqual(CFDictionaryGetValue(static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(httpsProxyArray.get(), 0)), kCFProxyTypeKey), kCFProxyTypeNone))
        httpsProxyCount = 0;

    m_isUsingProxy = httpProxyCount || httpsProxyCount;
}

class DNSResolveQueueCFNet::CompletionHandlerWrapper : public RefCounted<CompletionHandlerWrapper> {
public:
    static Ref<CompletionHandlerWrapper> create(DNSCompletionHandler&& completionHandler, std::optional<uint64_t> identifier) { return adoptRef(*new CompletionHandlerWrapper(WTFMove(completionHandler), identifier)); }

    void complete(std::optional<DNSError> error)
    {
        if (!m_completionHandler)
            return;

        if (error)
            m_completionHandler(makeUnexpected(*error));
        else if (m_addresses.isEmpty()) {
            ASSERT(receivedIPv4AndIPv6());
            m_completionHandler(makeUnexpected(WebCore::DNSError::CannotResolve));
        } else {
            ASSERT(receivedIPv4AndIPv6());
            m_completionHandler(std::exchange(m_addresses, { }));
        }

        // This is currently necessary to prevent unbounded growth of m_pendingRequests.
        // FIXME: NetworkRTCProvider::CreateResolver should use sendWithAsyncReply, and there's
        // no need to have the ability to stop a DNS lookup. Then we don't need m_pendingRequests.
        if (m_identifier)
            stopResolveDNS(*m_identifier);
    }

    void addIPAddress(IPAddress&& address)
    {
        m_hasReceivedIPv4 |= address.isIPv4();
        m_hasReceivedIPv6 |= address.isIPv6();
        if (!address.containsOnlyZeros())
            m_addresses.append(WTFMove(address));
    }

    bool receivedIPv4AndIPv6() const { return m_hasReceivedIPv4 && m_hasReceivedIPv6; }

private:
    CompletionHandlerWrapper(DNSCompletionHandler&& completionHandler, std::optional<uint64_t> identifier)
        : m_completionHandler(WTFMove(completionHandler))
        , m_identifier(identifier) { }

    DNSCompletionHandler m_completionHandler;
    Vector<IPAddress> m_addresses;
    std::optional<uint64_t> m_identifier;
    bool m_hasReceivedIPv4 { false };
    bool m_hasReceivedIPv6 { false };
};

static std::optional<IPAddress> extractIPAddress(const struct sockaddr* address)
{
    if (!address) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
    if (address->sa_family == AF_INET)
        return IPAddress { reinterpret_cast<const struct sockaddr_in*>(address)->sin_addr };
    if (address->sa_family == AF_INET6)
        return IPAddress { reinterpret_cast<const struct sockaddr_in6*>(address)->sin6_addr };
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static void dnsLookupCallback(DNSServiceRef serviceRef, DNSServiceFlags flags, uint32_t, DNSServiceErrorType errorCode, const char*, const struct sockaddr* address, uint32_t, void* wrapperPointer)
{
    ASSERT(isMainThread());
    ASSERT(wrapperPointer);
    auto wrapper = adoptRef(*static_cast<DNSResolveQueueCFNet::CompletionHandlerWrapper*>(wrapperPointer));
    ASSERT(wrapper->refCount());

    struct DNSServiceDeallocator {
        void operator()(DNSServiceRef service) const { DNSServiceRefDeallocate(service); }
    };
    auto service = std::unique_ptr<_DNSServiceRef_t, DNSServiceDeallocator>(serviceRef);

    if (errorCode != kDNSServiceErr_NoError && errorCode != kDNSServiceErr_NoSuchRecord)
        return wrapper->complete(WebCore::DNSError::Unknown);

    if (auto ipAddress = extractIPAddress(address))
        wrapper->addIPAddress(WTFMove(*ipAddress));

    if (flags & kDNSServiceFlagsMoreComing || !wrapper->receivedIPv4AndIPv6()) {
        // These will be adopted again by a future callback.
        UNUSED_VARIABLE(wrapper.leakRef());
        service.release();
        return;
    }
    wrapper->complete(std::nullopt);
}

void DNSResolveQueueCFNet::performDNSLookup(const String& hostname, Ref<CompletionHandlerWrapper>&& wrapper)
{
    ASSERT(isMainThread());
    DNSServiceRef service { nullptr };
    auto& leakedWrapper = wrapper.leakRef();
    DNSServiceErrorType result = DNSServiceGetAddrInfo(&service, kDNSServiceFlagsReturnIntermediates, kDNSServiceInterfaceIndexAny, kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6, hostname.utf8().data(), dnsLookupCallback, &leakedWrapper);
    if (result != kDNSServiceErr_NoError) {
        ASSERT(!service);
        return adoptRef(leakedWrapper)->complete(DNSError::CannotResolve);
    }
    result = DNSServiceSetDispatchQueue(service, dispatch_get_main_queue());
    ASSERT(result == kDNSServiceErr_NoError);
}

void DNSResolveQueueCFNet::platformResolve(const String& hostname)
{
    performDNSLookup(hostname, CompletionHandlerWrapper::create([](auto) {
        DNSResolveQueue::singleton().decrementRequestCount();
    }, std::nullopt));
}

void DNSResolveQueueCFNet::resolve(const String& hostname, uint64_t identifier, DNSCompletionHandler&& completionHandler)
{
    auto wrapper = CompletionHandlerWrapper::create(WTFMove(completionHandler), identifier);
    performDNSLookup(hostname, wrapper.copyRef());
    m_pendingRequests.add(identifier, WTFMove(wrapper));
}

void DNSResolveQueueCFNet::stopResolve(uint64_t identifier)
{
    auto wrapper = m_pendingRequests.take(identifier);
    if (!wrapper)
        return;
    wrapper->complete(DNSError::Cancelled);
}

}
