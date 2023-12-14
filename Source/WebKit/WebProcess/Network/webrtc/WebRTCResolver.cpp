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
#include "WebRTCResolver.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCResolver.h"
#include "LibWebRTCSocketFactory.h"
#include <WebCore/LibWebRTCProvider.h>
#include <wtf/Function.h>

namespace WebKit {

WebRTCResolver::WebRTCResolver(LibWebRTCSocketFactory& socketFactory, LibWebRTCResolverIdentifier identifier)
    : m_socketFactory(socketFactory)
    , m_identifier(identifier)
{
}

void WebRTCResolver::setResolvedAddress(const Vector<RTCNetwork::IPAddress>& addresses)
{
    auto& factory = m_socketFactory;

    auto rtcAddresses = addresses.map([](auto& address) {
        return address.rtcAddress();
    });
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([&factory, identifier = m_identifier, rtcAddresses = WTFMove(rtcAddresses)] () mutable {
        if (auto resolver = factory.resolver(identifier))
            resolver->setResolvedAddress(WTFMove(rtcAddresses));
    });
}

void WebRTCResolver::resolvedAddressError(int error)
{
    auto& factory = m_socketFactory;
    WebCore::LibWebRTCProvider::callOnWebRTCNetworkThread([&factory, identifier = m_identifier, error]() {
        if (auto resolver = factory.resolver(identifier))
            resolver->setError(error);
    });
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
