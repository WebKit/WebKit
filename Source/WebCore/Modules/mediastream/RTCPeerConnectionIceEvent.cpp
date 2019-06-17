/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "RTCPeerConnectionIceEvent.h"

#if ENABLE(WEB_RTC)

#include "EventNames.h"
#include "RTCIceCandidate.h"

namespace WebCore {

Ref<RTCPeerConnectionIceEvent> RTCPeerConnectionIceEvent::create(CanBubble canBubble, IsCancelable cancelable, RefPtr<RTCIceCandidate>&& candidate, String&& serverURL)
{
    return adoptRef(*new RTCPeerConnectionIceEvent(eventNames().icecandidateEvent, canBubble, cancelable, WTFMove(candidate), WTFMove(serverURL)));
}

Ref<RTCPeerConnectionIceEvent> RTCPeerConnectionIceEvent::create(const AtomString& type, Init&& init)
{
    return adoptRef(*new RTCPeerConnectionIceEvent(type, init.bubbles ? CanBubble::Yes : CanBubble::No,
        init.cancelable ? IsCancelable::Yes : IsCancelable::No, WTFMove(init.candidate), WTFMove(init.url)));
}

RTCPeerConnectionIceEvent::RTCPeerConnectionIceEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, RefPtr<RTCIceCandidate>&& candidate, String&& serverURL)
    : Event(type, canBubble, cancelable)
    , m_candidate(WTFMove(candidate))
    , m_url(WTFMove(serverURL))
{
}

RTCPeerConnectionIceEvent::~RTCPeerConnectionIceEvent() = default;

RTCIceCandidate* RTCPeerConnectionIceEvent::candidate() const
{
    return m_candidate.get();
}

EventInterface RTCPeerConnectionIceEvent::eventInterface() const
{
    return RTCPeerConnectionIceEventInterfaceType;
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
