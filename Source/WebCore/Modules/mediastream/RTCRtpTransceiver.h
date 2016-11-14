/*
 * Copyright (C) 2016 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#pragma once

#if ENABLE(WEB_RTC)

#include "RTCIceTransport.h"
#include "RTCRtpReceiver.h"
#include "RTCRtpSender.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RTCRtpTransceiver : public RefCounted<RTCRtpTransceiver>, public ScriptWrappable {
public:
    // This enum is mirrored in RTCPeerConnection.h
    enum class Direction { Sendrecv, Sendonly, Recvonly, Inactive };

    static Ref<RTCRtpTransceiver> create(Ref<RTCRtpSender>&& sender, Ref<RTCRtpReceiver>&& receiver) { return adoptRef(*new RTCRtpTransceiver(WTFMove(sender), WTFMove(receiver))); }
    virtual ~RTCRtpTransceiver() { }

    bool hasSendingDirection() const;
    void enableSendingDirection();
    void disableSendingDirection();

    const String& directionString() const;
    Direction direction() const { return m_direction; }
    void setDirection(Direction direction) { m_direction = direction; }

    const String& provisionalMid() const { return m_provisionalMid; }
    void setProvisionalMid(const String& provisionalMid) { m_provisionalMid = provisionalMid; }

    const String& mid() const { return m_mid; }
    void setMid(const String& mid) { m_mid = mid; }

    RTCRtpSender& sender() { return m_sender.get(); }
    RTCRtpReceiver& receiver() { return m_receiver.get(); }

    bool stopped() const { return m_stopped; }
    void stop() { m_stopped = true; }

    // FIXME: Temporary solution to keep track of ICE states for this transceiver. Later, each
    // sender and receiver will have up to two DTLS transports, which in turn will have an ICE
    // transport each.
    RTCIceTransport& iceTransport() { return m_iceTransport.get(); }

    static String getNextMid();

private:
    RTCRtpTransceiver(Ref<RTCRtpSender>&&, Ref<RTCRtpReceiver>&&);

    String m_provisionalMid;
    String m_mid;

    Direction m_direction;

    Ref<RTCRtpSender> m_sender;
    Ref<RTCRtpReceiver> m_receiver;

    bool m_stopped { false };

    Ref<RTCIceTransport> m_iceTransport;
};

class RtpTransceiverSet {
public:
    const Vector<RefPtr<RTCRtpTransceiver>>& list() const { return m_transceivers; }
    void append(Ref<RTCRtpTransceiver>&&);

    const Vector<std::reference_wrapper<RTCRtpSender>>& senders() const { return m_senders; }
    const Vector<std::reference_wrapper<RTCRtpReceiver>>& receivers() const { return m_receivers; }

private:
    Vector<RefPtr<RTCRtpTransceiver>> m_transceivers;

    Vector<std::reference_wrapper<RTCRtpSender>> m_senders;
    Vector<std::reference_wrapper<RTCRtpReceiver>> m_receivers;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
