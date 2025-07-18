/*
 * Copyright (C) 2008 Collin Jackson  <collinj@webkit.org>
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
#include <pal/spi/cf/CFNetworkSPI.h>
#include <pal/spi/cocoa/NetworkSPI.h>
#include <wtf/BlockPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashSet.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/cf/VectorCF.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#include <wtf/posix/SocketPOSIX.h>
#include <wtf/text/StringHash.h>

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

    void complete(DNSAddressesOrError&& result)
    {
        if (m_completionHandler)
            m_completionHandler(WTFMove(result));

        // This is currently necessary to prevent unbounded growth of m_pendingRequests.
        // FIXME: NetworkRTCProvider::CreateResolver should use sendWithAsyncReply, and there's
        // no need to have the ability to stop a DNS lookup. Then we don't need m_pendingRequests.
        if (m_identifier)
            stopResolveDNS(*m_identifier);
    }

private:
    CompletionHandlerWrapper(DNSCompletionHandler&& completionHandler, std::optional<uint64_t> identifier)
        : m_completionHandler(WTFMove(completionHandler))
        , m_identifier(identifier) { }

    DNSCompletionHandler m_completionHandler;
    std::optional<uint64_t> m_identifier;
};

static std::optional<IPAddress> extractIPAddress(const struct sockaddr* address)
{
    if (!address)
        return std::nullopt;
    if (auto* addressV4 = dynamicCastToIPV4SocketAddress(*address))
        return IPAddress { addressV4->sin_addr };
    if (auto* addressV6 = dynamicCastToIPV6SocketAddress(*address))
        return IPAddress { addressV6->sin6_addr };
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

static constexpr auto timeoutForDNSResolution = 60_s;

void DNSResolveQueueCFNet::performDNSLookup(const String& hostname, Ref<CompletionHandlerWrapper>&& completionHandler)
{
    RetainPtr hostEndpoint = adoptCF(nw_endpoint_create_host(hostname.utf8().data(), "0"));
    RetainPtr context = adoptCF(nw_context_create("WebKit DNS Lookup"));
    RetainPtr parameters = adoptCF(nw_parameters_create());

    auto bundleID = applicationBundleIdentifier();
    if (!bundleID.isEmpty())
        nw_parameters_set_source_application_by_bundle_id(parameters.get(), bundleID.ascii().data());

    nw_context_set_privacy_level(context.get(), nw_context_privacy_level_silent);
    nw_parameters_set_context(parameters.get(), context.get());
    RetainPtr pathEvaluator = adoptCF(nw_path_create_evaluator_for_endpoint(hostEndpoint.get(), parameters.get()));
    RetainPtr path = adoptCF(nw_path_evaluator_copy_path(pathEvaluator.get()));
    RetainPtr resolver = adoptCF(nw_resolver_create_with_path(path.get()));

    RELEASE_ASSERT_WITH_MESSAGE(isMainThread(), "Always create timer on the main thread.");
    auto timeoutTimer = makeUnique<Timer>([resolver, completionHandler]() mutable {
        completionHandler->complete(makeUnexpected(DNSError::Cancelled));
        nw_resolver_cancel(resolver.get()); // This will destroy the timer and this lambda.
    });
    timeoutTimer->startOneShot(timeoutForDNSResolution);

    nw_resolver_set_update_handler(resolver.get(), dispatch_get_main_queue(), makeBlockPtr([resolver = WTFMove(resolver), completionHandler = WTFMove(completionHandler), timeoutTimer = WTFMove(timeoutTimer)] (nw_resolver_status_t status, nw_array_t resolvedEndpoints) mutable {
        if (status != nw_resolver_status_complete)
            return;

#if PLATFORM(IOS_FAMILY)
        WebThreadLock();
#endif

        auto callCompletionHandler = [resolver = WTFMove(resolver), completionHandler = WTFMove(completionHandler), timeoutTimer = WTFMove(timeoutTimer)](DNSAddressesOrError&& result) mutable {
            timeoutTimer->stop();
            completionHandler->complete(WTFMove(result));
            // We need to call nw_resolver_cancel to release the reference taken by nw_resolver_set_update_handler on the resolver.
            nw_resolver_cancel(resolver.get());
        };

        if (!resolvedEndpoints)
            return callCompletionHandler(makeUnexpected(DNSError::CannotResolve));

        size_t count = nw_array_get_count(resolvedEndpoints);
        Vector<IPAddress> result;
        result.reserveInitialCapacity(count);
        for (size_t i = 0; i < count; i++) {
            nw_endpoint_t resolvedEndpoint = reinterpret_cast<nw_endpoint_t>(nw_array_get_object_at_index(resolvedEndpoints, i));
            if (auto address = extractIPAddress(nw_endpoint_get_address(resolvedEndpoint)))
                result.append(WTFMove(*address));
        }
        result.shrinkToFit();

        if (result.isEmpty())
            callCompletionHandler(makeUnexpected(DNSError::CannotResolve));
        else
            callCompletionHandler(WTFMove(result));
    }).get());
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
    wrapper->complete(makeUnexpected(DNSError::Cancelled));
}

}
