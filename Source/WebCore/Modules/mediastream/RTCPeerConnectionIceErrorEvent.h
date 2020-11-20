/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include "Event.h"
#include <wtf/Optional.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
class RTCIceCandidate;

class RTCPeerConnectionIceErrorEvent final : public Event {
    WTF_MAKE_ISO_ALLOCATED(RTCPeerConnectionIceErrorEvent);
public:
    virtual ~RTCPeerConnectionIceErrorEvent();

    struct Init : EventInit {
        String address;
        Optional<uint16_t> port;
        String url;
        uint16_t errorCode { 0 };
        String errorText;
    };

    static Ref<RTCPeerConnectionIceErrorEvent> create(const AtomString& type, Init&&);
    static Ref<RTCPeerConnectionIceErrorEvent> create(CanBubble, IsCancelable, String&& address, Optional<uint16_t> port, String&& url, uint16_t errorCode, String&& errorText);

    const String& address() const { return m_address; }
    Optional<uint16_t> port() const { return m_port; }
    const String& url() const { return m_url; }
    uint16_t errorCode() const { return m_errorCode; }
    const String& errorText() const { return m_errorText; }

    virtual EventInterface eventInterface() const;

private:
    RTCPeerConnectionIceErrorEvent(const AtomString& type, CanBubble, IsCancelable, String&& address, Optional<uint16_t> port, String&& url, uint16_t errorCode, String&& errorText);

    String m_address;
    Optional<uint16_t> m_port;
    String m_url;
    uint16_t m_errorCode { 0 };
    String m_errorText;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
