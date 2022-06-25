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

#include "config.h"
#include "RTCPeerConnectionIceErrorEvent.h"

#if ENABLE(WEB_RTC)

#include "EventNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCPeerConnectionIceErrorEvent);

Ref<RTCPeerConnectionIceErrorEvent> RTCPeerConnectionIceErrorEvent::create(CanBubble canBubble, IsCancelable isCancelable, String&& address, std::optional<uint16_t> port, String&& url, uint16_t errorCode, String&& errorText)
{
    return adoptRef(*new RTCPeerConnectionIceErrorEvent(eventNames().icecandidateerrorEvent, canBubble, isCancelable, WTFMove(address), port, WTFMove(url), errorCode, WTFMove(errorText)));
}

Ref<RTCPeerConnectionIceErrorEvent> RTCPeerConnectionIceErrorEvent::create(const AtomString& type, Init&& init)
{
    return adoptRef(*new RTCPeerConnectionIceErrorEvent(type, init.bubbles ? CanBubble::Yes : CanBubble::No,
        init.cancelable ? IsCancelable::Yes : IsCancelable::No, WTFMove(init.address), init.port, WTFMove(init.url), WTFMove(init.errorCode), WTFMove(init.errorText)));
}

RTCPeerConnectionIceErrorEvent::RTCPeerConnectionIceErrorEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, String&& address, std::optional<uint16_t> port, String&& url, uint16_t errorCode, String&& errorText)
    : Event(type, canBubble, cancelable)
    , m_address(WTFMove(address))
    , m_port(port)
    , m_url(WTFMove(url))
    , m_errorCode(errorCode)
    , m_errorText(WTFMove(errorText))
{
}

RTCPeerConnectionIceErrorEvent::~RTCPeerConnectionIceErrorEvent() = default;

EventInterface RTCPeerConnectionIceErrorEvent::eventInterface() const
{
    return RTCPeerConnectionIceErrorEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
