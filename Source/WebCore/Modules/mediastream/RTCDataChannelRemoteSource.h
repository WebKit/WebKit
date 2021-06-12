/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "RTCDataChannelHandler.h"
#include "RTCDataChannelHandlerClient.h"
#include "RTCDataChannelIdentifier.h"
#include "RTCDataChannelRemoteSourceConnection.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

class RTCDataChannelRemoteSource : public RTCDataChannelHandlerClient {
public:
    static Ref<RTCDataChannelRemoteSource> create(RTCDataChannelIdentifier identifier, UniqueRef<RTCDataChannelHandler>&& handler, Ref<RTCDataChannelRemoteSourceConnection>&& connection) { return adoptRef(*new RTCDataChannelRemoteSource(identifier, WTFMove(handler), WTFMove(connection))); }
    ~RTCDataChannelRemoteSource();

    void sendStringData(const CString& text) { m_handler->sendStringData(text); }
    void sendRawData(const uint8_t* data, size_t size) { m_handler->sendRawData(data, size); }
    void close() { m_handler->close(); }

private:
    WEBCORE_EXPORT RTCDataChannelRemoteSource(RTCDataChannelIdentifier, UniqueRef<RTCDataChannelHandler>&&, Ref<RTCDataChannelRemoteSourceConnection>&&);

    // RTCDataChannelHandlerClient
    void didChangeReadyState(RTCDataChannelState state) final { m_connection->didChangeReadyState(m_identifier, state); }
    void didReceiveStringData(const String& text) final { m_connection->didReceiveStringData(m_identifier, text); }
    void didReceiveRawData(const uint8_t* data, size_t size) final { m_connection->didReceiveRawData(m_identifier, data, size); }
    void didDetectError() final { m_connection->didDetectError(m_identifier); }
    void bufferedAmountIsDecreasing(size_t amount) final { m_connection->bufferedAmountIsDecreasing(m_identifier, amount); }

    RTCDataChannelIdentifier m_identifier;
    UniqueRef<RTCDataChannelHandler> m_handler;
    Ref<RTCDataChannelRemoteSourceConnection> m_connection;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
