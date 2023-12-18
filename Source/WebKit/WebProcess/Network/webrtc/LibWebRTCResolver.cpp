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
#include "LibWebRTCResolver.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCNetwork.h"
#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "NetworkRTCProviderMessages.h"
#include "WebProcess.h"
#include <wtf/MainThread.h>

namespace WebKit {

void LibWebRTCResolver::sendOnMainThread(Function<void(IPC::Connection&)>&& callback)
{
    callOnMainRunLoop([callback = WTFMove(callback)]() {
        Ref networkProcessConnection = WebProcess::singleton().ensureNetworkProcessConnection().connection();
        callback(networkProcessConnection);
    });
}

LibWebRTCResolver::~LibWebRTCResolver()
{
    WebProcess::singleton().libWebRTCNetwork().socketFactory().removeResolver(m_identifier);
    sendOnMainThread([identifier = m_identifier](IPC::Connection& connection) {
        connection.send(Messages::NetworkRTCProvider::StopResolver(identifier), 0);
    });
}

void LibWebRTCResolver::start(const rtc::SocketAddress& address, Function<void()>&& callback)
{
    ASSERT(!m_callback);

    m_callback = WTFMove(callback);
    m_addressToResolve = address;
    m_port = address.port();

    auto addressString = address.HostAsURIString();
    String name { addressString.data(), static_cast<unsigned>(addressString.length()) };

    if (name.endsWithIgnoringASCIICase(".local"_s) && !WTF::isVersion4UUID(StringView { name }.left(name.length() - 6))) {
        RELEASE_LOG_ERROR(WebRTC, "mDNS candidate is not a Version 4 UUID");
        setError(-1);
        return;
    }

    sendOnMainThread([identifier = m_identifier, name = WTFMove(name).isolatedCopy()](IPC::Connection& connection) {
        connection.send(Messages::NetworkRTCProvider::CreateResolver(identifier, name), 0);
    });
}

const webrtc::AsyncDnsResolverResult& LibWebRTCResolver::result() const
{
    return *this;
}

bool LibWebRTCResolver::GetResolvedAddress(int family, rtc::SocketAddress* address) const
{
    ASSERT(address);
    if (m_error || !m_addresses.size())
        return false;

    *address = m_addressToResolve;
    for (auto& ipAddress : m_addresses) {
        if (family == ipAddress.family()) {
            address->SetResolvedIP(ipAddress);
            address->SetPort(m_port);
            return true;
        }
    }
    return false;
}

void LibWebRTCResolver::setResolvedAddress(Vector<rtc::IPAddress>&& addresses)
{
    m_addresses = WTFMove(addresses);
    m_callback();
}

void LibWebRTCResolver::setError(int error)
{
    m_error = error;
    m_callback();
}

} // namespace WebKit

#endif // USE(LIBWEBRTC)
