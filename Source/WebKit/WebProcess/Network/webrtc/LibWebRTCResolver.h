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

#pragma once

#if USE(LIBWEBRTC)

#include "LibWebRTCDnsResolverFactory.h"
#include "LibWebRTCResolverIdentifier.h"
#include <WebCore/LibWebRTCMacros.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Identified.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace IPC {
class Connection;
}

namespace WebKit {
class LibWebRTCSocketFactory;

class LibWebRTCResolver final : public LibWebRTCDnsResolverFactory::Resolver, private webrtc::AsyncDnsResolverResult, public CanMakeCheckedPtr<LibWebRTCResolver>, public Identified<LibWebRTCResolverIdentifier> {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCResolver);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LibWebRTCResolver);
public:
    LibWebRTCResolver() = default;
    ~LibWebRTCResolver();

    void start(const rtc::SocketAddress&, Function<void()>&&) final;

private:
    friend class WebRTCResolver;

    // webrtc::AsyncDnsResolverInterface
    const webrtc::AsyncDnsResolverResult& result() const final;

    // webrtc::AsyncDnsResolverResult
    bool GetResolvedAddress(int family, rtc::SocketAddress*) const final;
    int GetError() const { return m_error; }

    void setError(int);
    void setResolvedAddress(Vector<rtc::IPAddress>&&);

    static void sendOnMainThread(Function<void(IPC::Connection&)>&&);

    Vector<rtc::IPAddress> m_addresses;
    rtc::SocketAddress m_addressToResolve;
    Function<void()> m_callback;
    int m_error { 0 };
    uint16_t m_port { 0 };
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
