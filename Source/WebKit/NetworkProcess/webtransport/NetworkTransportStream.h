/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#include <span>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include <Network/Network.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {
struct WebTransportStreamIdentifierType;
using WebTransportStreamIdentifier = ObjectIdentifier<WebTransportStreamIdentifierType>;
}

namespace WebKit {
enum class NetworkTransportStreamType : uint8_t { Bidirectional, OutgoingUnidirectional, IncomingUnidirectional };

class NetworkTransportSession;

class NetworkTransportStream : public RefCounted<NetworkTransportStream>, public CanMakeWeakPtr<NetworkTransportStream> {
    WTF_MAKE_TZONE_ALLOCATED(NetworkTransportStream);
public:
    template<typename... Args> static Ref<NetworkTransportStream> create(Args&&... args) { return adoptRef(*new NetworkTransportStream(std::forward<Args>(args)...)); }

    WebCore::WebTransportStreamIdentifier identifier() const { return m_identifier; }

    void sendBytes(std::span<const uint8_t>, bool withFin);

protected:
#if PLATFORM(COCOA)
    NetworkTransportStream(NetworkTransportSession&, nw_connection_t, NetworkTransportStreamType);
#else
    NetworkTransportStream();
#endif

private:
    void receiveLoop();

    const WebCore::WebTransportStreamIdentifier m_identifier;
    WeakPtr<NetworkTransportSession> m_session;
#if PLATFORM(COCOA)
    const RetainPtr<nw_connection_t> m_connection;
#endif
    const NetworkTransportStreamType m_streamType;
};

}
