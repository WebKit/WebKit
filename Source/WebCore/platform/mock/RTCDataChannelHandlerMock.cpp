/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
#include "RTCDataChannelHandlerMock.h"

#if ENABLE(WEB_RTC)

#include "RTCDataChannelHandlerClient.h"
#include "RTCNotifiersMock.h"

namespace WebCore {

RTCDataChannelHandlerMock::RTCDataChannelHandlerMock(const String& label, const RTCDataChannelInit& init)
    : m_label(label)
    , m_protocol(init.protocol)
{
}

void RTCDataChannelHandlerMock::setClient(RTCDataChannelHandlerClient& client)
{
    ASSERT(!m_client);
    m_client = &client;
    auto notifier = adoptRef(*new DataChannelStateNotifier(m_client, RTCDataChannelState::Open));
    m_timerEvents.append(adoptRef(new TimerEvent(this, WTFMove(notifier))));
}

bool RTCDataChannelHandlerMock::sendStringData(const CString& string)
{
    m_client->didReceiveStringData(String::fromUTF8(string));
    return true;
}

bool RTCDataChannelHandlerMock::sendRawData(const char* data, size_t size)
{
    m_client->didReceiveRawData(data, size);
    return true;
}

void RTCDataChannelHandlerMock::close()
{
    auto notifier = adoptRef(*new DataChannelStateNotifier(m_client, RTCDataChannelState::Closed));
    m_timerEvents.append(adoptRef(new TimerEvent(this, WTFMove(notifier))));
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
