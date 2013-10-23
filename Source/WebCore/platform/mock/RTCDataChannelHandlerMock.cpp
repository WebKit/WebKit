/*
 *  Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "RTCDataChannelHandlerMock.h"

#include "RTCDataChannelHandlerClient.h"
#include "RTCNotifiersMock.h"

namespace WebCore {

RTCDataChannelHandlerMock::RTCDataChannelHandlerMock(const String& label, const RTCDataChannelInit& init)
    : m_label(label)
    , m_protocol(init.protocol)
    , m_maxRetransmitTime(init.maxRetransmitTime)
    , m_maxRetransmits(init.maxRetransmits)
    , m_id(init.id)
    , m_ordered(init.ordered)
    , m_negotiated(init.negotiated)
{
}

void RTCDataChannelHandlerMock::setClient(RTCDataChannelHandlerClient* client)
{
    if (!client)
        return;

    m_client = client;
    RefPtr<DataChannelStateNotifier> notifier = adoptRef(new DataChannelStateNotifier(m_client, RTCDataChannelHandlerClient::ReadyStateOpen));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

bool RTCDataChannelHandlerMock::sendStringData(const String& string)
{
    m_client->didReceiveStringData(string);
    return true;
}

bool RTCDataChannelHandlerMock::sendRawData(const char* data, size_t size)
{
    m_client->didReceiveRawData(data, size);
    return true;
}

void RTCDataChannelHandlerMock::close()
{
    RefPtr<DataChannelStateNotifier> notifier = adoptRef(new DataChannelStateNotifier(m_client, RTCDataChannelHandlerClient::ReadyStateClosed));
    m_timerEvents.append(adoptRef(new TimerEvent(this, notifier)));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
