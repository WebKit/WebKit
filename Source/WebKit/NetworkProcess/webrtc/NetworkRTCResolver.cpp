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
#include "NetworkRTCResolver.h"

#if USE(LIBWEBRTC)

#include <wtf/Expected.h>

namespace WebKit {

// FIXME: Use the function after removing the NetworkRTCResolverCocoa.
#if !PLATFORM(COCOA)
std::unique_ptr<NetworkRTCResolver> NetworkRTCResolver::create(LibWebRTCResolverIdentifier identifier, WebCore::DNSCompletionHandler&& completionHandler)
{
    return std::unique_ptr<NetworkRTCResolver>(new NetworkRTCResolver(identifier, WTFMove(completionHandler)));
}
#endif

NetworkRTCResolver::NetworkRTCResolver(LibWebRTCResolverIdentifier identifier, WebCore::DNSCompletionHandler&& completionHandler)
    : m_identifier(identifier)
    , m_completionHandler(WTFMove(completionHandler))
{
}

NetworkRTCResolver::~NetworkRTCResolver()
{
    if (auto completionHandler = WTFMove(m_completionHandler))
        completionHandler(makeUnexpected(WebCore::DNSError::Unknown));
}

void NetworkRTCResolver::start(const String& address)
{
    WebCore::resolveDNS(address, m_identifier.toUInt64(), WTFMove(m_completionHandler));
}

void NetworkRTCResolver::stop()
{
    WebCore::stopResolveDNS(m_identifier.toUInt64());
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
