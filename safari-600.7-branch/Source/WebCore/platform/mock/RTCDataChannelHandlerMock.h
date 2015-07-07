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

#ifndef RTCDataChannelHandlerMock_h
#define RTCDataChannelHandlerMock_h

#if ENABLE(MEDIA_STREAM)

#include "RTCDataChannelHandler.h"
#include "RTCPeerConnectionHandler.h"
#include "TimerEventBasedMock.h"

namespace WebCore {

class RTCDataChannelHandlerMock final : public RTCDataChannelHandler, public TimerEventBasedMock {
public:
    RTCDataChannelHandlerMock(const String&, const RTCDataChannelInit&);
    virtual ~RTCDataChannelHandlerMock() { }

    virtual void setClient(RTCDataChannelHandlerClient*) override;

    virtual String label() override { return m_label; }
    virtual bool ordered() override { return m_ordered; }
    virtual unsigned short maxRetransmitTime() override { return m_maxRetransmitTime; }
    virtual unsigned short maxRetransmits() override { return m_maxRetransmits; }
    virtual String protocol() override { return m_protocol; }
    virtual bool negotiated() override { return m_negotiated; }
    virtual unsigned short id() override { return m_id; }
    virtual unsigned long bufferedAmount() override { return 0; }

    virtual bool sendStringData(const String&) override;
    virtual bool sendRawData(const char*, size_t) override;
    virtual void close() override;

private:
    RTCDataChannelHandlerClient* m_client;

    String m_label;
    String m_protocol;
    unsigned short m_maxRetransmitTime;
    unsigned short m_maxRetransmits;
    unsigned short m_id;
    bool m_ordered;
    bool m_negotiated;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCDataChannelHandlerMock_h
