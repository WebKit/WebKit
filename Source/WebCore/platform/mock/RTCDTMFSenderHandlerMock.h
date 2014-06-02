/*
 *  Copyright (C) 2014 Samsung Electornics and/or its subsidiary(-ies).
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

#ifndef RTCDTMFSenderHandlerMock_h
#define RTCDTMFSenderHandlerMock_h

#if ENABLE(MEDIA_STREAM)

#include "RTCDTMFSenderHandler.h"
#include "RTCPeerConnectionHandler.h"

namespace WebCore {

class RTCDTMFSenderHandlerMock final : public RTCDTMFSenderHandler {
public:
    RTCDTMFSenderHandlerMock();
    virtual ~RTCDTMFSenderHandlerMock() { }

    virtual void setClient(RTCDTMFSenderHandlerClient*) override;
    virtual String currentToneBuffer() override { return m_toneBuffer; }
    virtual bool canInsertDTMF() override { return true; }
    virtual bool insertDTMF(const String& tones, long duration, long interToneGap) override;

private:
    RTCDTMFSenderHandlerClient* m_client;

    String m_toneBuffer;
    long m_duration;
    long m_interToneGap;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RTCDataChannelHandlerMock_h
