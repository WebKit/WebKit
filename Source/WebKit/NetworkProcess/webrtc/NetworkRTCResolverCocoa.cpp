/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "NetworkRTCResolverCocoa.h"

#if USE(LIBWEBRTC)

#include <wtf/Expected.h>
#include <wtf/cf/VectorCF.h>

namespace WebKit {

static void resolvedName(CFHostRef hostRef, CFHostInfoType typeInfo, const CFStreamError *error, void *info)
{
    ASSERT_UNUSED(typeInfo, !typeInfo);

    ASSERT(info);
    auto* resolver = static_cast<NetworkRTCResolverCocoa*>(info);

    if (error->domain) {
        // FIXME: Need to handle failure, but info is not provided in the callback.
        resolver->completed(makeUnexpected(WebCore::DNSError::Unknown));
        return;
    }

    Boolean result;
    CFArrayRef resolvedAddresses = CFHostGetAddressing(hostRef, &result);
    ASSERT_UNUSED(result, result);

    auto addresses = makeVector(resolvedAddresses, [] (CFDataRef data) -> std::optional<WebCore::IPAddress> {
        return WebCore::IPAddress::fromSockAddrIn6(*reinterpret_cast<const struct sockaddr_in6*>(CFDataGetBytePtr(data)));
    });
    if (addresses.isEmpty()) {
        resolver->completed(makeUnexpected(WebCore::DNSError::CannotResolve));
        return;
    }
    resolver->completed(WTFMove(addresses));
}

std::unique_ptr<NetworkRTCResolver> NetworkRTCResolver::create(LibWebRTCResolverIdentifier identifier, WebCore::DNSCompletionHandler&& completionHandler)
{
    return std::unique_ptr<NetworkRTCResolver>(new NetworkRTCResolverCocoa(identifier, WTFMove(completionHandler)));
}

NetworkRTCResolverCocoa::NetworkRTCResolverCocoa(LibWebRTCResolverIdentifier identifier, WebCore::DNSCompletionHandler&& completionHandler)
    : NetworkRTCResolver(identifier, WTFMove(completionHandler))
{
}

NetworkRTCResolverCocoa::~NetworkRTCResolverCocoa()
{
    CFHostUnscheduleFromRunLoop(m_host.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFHostSetClient(m_host.get(), nullptr, nullptr);
}

void NetworkRTCResolverCocoa::start(const String& address)
{
    m_host = adoptCF(CFHostCreateWithName(kCFAllocatorDefault, address.createCFString().get()));
    CFHostClientContext context = { 0, this, nullptr, nullptr, nullptr };
    CFHostSetClient(m_host.get(), resolvedName, &context);
    CFHostScheduleWithRunLoop(m_host.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    Boolean result = CFHostStartInfoResolution(m_host.get(), kCFHostAddresses, nullptr);
    ASSERT_UNUSED(result, result);
}

void NetworkRTCResolverCocoa::stop()
{
    CFHostCancelInfoResolution(m_host.get(), CFHostInfoType::kCFHostAddresses);
    if (auto completionHandler = WTFMove(m_completionHandler))
        completionHandler(makeUnexpected(WebCore::DNSError::Cancelled));
}

void NetworkRTCResolverCocoa::completed(WebCore::DNSAddressesOrError&& addressesOrError)
{
    if (auto completionHandler = WTFMove(m_completionHandler))
        completionHandler(WTFMove(addressesOrError));
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
