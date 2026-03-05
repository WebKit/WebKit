/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include <WebCore/RTCDataChannelHandler.h>
#include <WebCore/RTCDataChannelHandlerClient.h>
#include <WebCore/RTCDataChannelIdentifier.h>
#include <WebCore/RTCDataChannelRemoteSourceConnection.h>
#include <WebCore/RTCError.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class RTCDataChannelRemoteSource : public RTCDataChannelHandlerClient, public RefCounted<RTCDataChannelRemoteSource> {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(RTCDataChannelRemoteSource, WEBCORE_EXPORT);
public:
    WEBCORE_EXPORT static Ref<RTCDataChannelRemoteSource> create(RTCDataChannelIdentifier localIdentifier, RTCDataChannelIdentifier remoteIdentifier, UniqueRef<RTCDataChannelHandler>&&, Ref<RTCDataChannelRemoteSourceConnection>&&);
    ~RTCDataChannelRemoteSource();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void sendStringData(const CString& text) { m_handler->sendStringData(text); }
    void sendRawData(std::span<const uint8_t> data) { m_handler->sendRawData(data); }
    void close() { m_handler->close(); }

private:
    WEBCORE_EXPORT RTCDataChannelRemoteSource(RTCDataChannelIdentifier localIdentifier, RTCDataChannelIdentifier remoteIdentifier, UniqueRef<RTCDataChannelHandler>&&, Ref<RTCDataChannelRemoteSourceConnection>&&);

    // RTCDataChannelHandlerClient
    void didChangeReadyState(RTCDataChannelState) final;
    void didReceiveStringData(const String&) final;
    void didReceiveRawData(std::span<const uint8_t>) final;
    void didDetectError(Ref<RTCError>&&) final;
    void bufferedAmountIsDecreasing(size_t) final;
    size_t NODELETE bufferedAmount() const final;
    void peerConnectionIsClosing() final;

    const RTCDataChannelIdentifier m_remoteIdentifier;
    bool m_isClosed { false };
    const UniqueRef<RTCDataChannelHandler> m_handler;
    const Ref<RTCDataChannelRemoteSourceConnection> m_connection;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
