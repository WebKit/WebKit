/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * 3. Neither the name of Google Inc. nor the names of its contributors
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

#include "ExceptionOr.h"
#include "RTCIceCandidateType.h"
#include "RTCIceComponent.h"
#include "RTCIceProtocol.h"
#include "RTCIceTcpCandidateType.h"
#include "ScriptWrappable.h"

namespace WebCore {

struct RTCIceCandidateInit;

class RTCIceCandidate final : public RefCounted<RTCIceCandidate>, public ScriptWrappable {
    WTF_MAKE_ISO_ALLOCATED(RTCIceCandidate);
public:
    static ExceptionOr<Ref<RTCIceCandidate>> create(const RTCIceCandidateInit&);
    static Ref<RTCIceCandidate> create(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex);

    const String& candidate() const { return m_candidate; }
    const String& sdpMid() const { return m_sdpMid; }
    Optional<unsigned short> sdpMLineIndex() const { return m_sdpMLineIndex; }

    struct Fields {
        String foundation;
        Optional<RTCIceComponent> component;
        Optional<unsigned> priority;
        String address;
        Optional<RTCIceProtocol> protocol;
        Optional<unsigned short> port;
        Optional<RTCIceCandidateType> type;
        Optional<RTCIceTcpCandidateType> tcpType;
        String relatedAddress;
        Optional<unsigned short> relatedPort;
        String usernameFragment;
    };

    String foundation() const { return m_fields.foundation; }
    Optional<RTCIceComponent> component() const { return m_fields.component; }
    Optional<unsigned> priority() const { return m_fields.priority; }
    String address() const { return m_fields.address; }
    Optional<RTCIceProtocol> protocol() const { return m_fields.protocol; }
    Optional<unsigned short> port() const { return m_fields.port; }
    Optional<RTCIceCandidateType> type() const { return m_fields.type; }
    Optional<RTCIceTcpCandidateType> tcpType() const { return m_fields.tcpType; }
    String relatedAddress() const { return m_fields.relatedAddress; }
    Optional<unsigned short> relatedPort() const { return m_fields.relatedPort; }
    String usernameFragment() const { return m_fields.usernameFragment; }

    RTCIceCandidateInit toJSON() const;

private:
    RTCIceCandidate(const String& candidate, const String& sdpMid, Optional<unsigned short> sdpMLineIndex, Fields&&);

    String m_candidate;
    String m_sdpMid;
    Optional<unsigned short> m_sdpMLineIndex;
    Fields m_fields;
};

Optional<RTCIceCandidate::Fields> parseIceCandidateSDP(const String&);

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
