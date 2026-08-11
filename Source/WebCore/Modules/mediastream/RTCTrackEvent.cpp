/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
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

#include "config.h"
#include "RTCTrackEvent.h"

#if ENABLE(WEB_RTC)

#include "MediaStream.h"
#include "MediaStreamTrack.h"
#include "RTCRtpTransceiver.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RTCTrackEvent);

Ref<RTCTrackEvent> RTCTrackEvent::create(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, Ref<RTCRtpReceiver>&& receiver, Ref<MediaStreamTrack>&& track, Vector<Ref<MediaStream>>&& streams, Ref<RTCRtpTransceiver>&& transceiver)
{
    return adoptRef(*new RTCTrackEvent(type, canBubble, cancelable, WTF::move(receiver), WTF::move(track), WTF::move(streams), WTF::move(transceiver)));
}

Ref<RTCTrackEvent> RTCTrackEvent::create(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new RTCTrackEvent(type, WTF::move(initializer), isTrusted));
}

RTCTrackEvent::RTCTrackEvent(const AtomString& type, CanBubble canBubble, IsCancelable cancelable, Ref<RTCRtpReceiver>&& receiver, Ref<MediaStreamTrack>&& track, Vector<Ref<MediaStream>>&& streams, Ref<RTCRtpTransceiver>&& transceiver)
    : Event(EventInterfaceType::RTCTrackEvent, type, canBubble, cancelable)
    , m_receiver(WTF::move(receiver))
    , m_track(WTF::move(track))
    , m_streams(WTF::move(streams))
    , m_transceiver(WTF::move(transceiver))
{
}

RTCTrackEvent::RTCTrackEvent(const AtomString& type, Init&& initializer, IsTrusted isTrusted)
    : Event(EventInterfaceType::RTCTrackEvent, type, WTF::move(initializer), isTrusted)
    , m_receiver(WTF::move(initializer.receiver))
    , m_track(WTF::move(initializer.track))
    , m_streams(WTF::move(initializer.streams))
    , m_transceiver(WTF::move(initializer.transceiver))
{
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
