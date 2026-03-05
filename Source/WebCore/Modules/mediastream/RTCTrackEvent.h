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

#pragma once

#if ENABLE(WEB_RTC)

#include "Event.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class MediaStream;
class MediaStreamTrack;
class RTCRtpReceiver;
class RTCRtpTransceiver;

typedef Vector<Ref<MediaStream>> MediaStreamArray;

class RTCTrackEvent final : public Event {
    WTF_MAKE_TZONE_ALLOCATED(RTCTrackEvent);
public:
    static Ref<RTCTrackEvent> create(const AtomString& type, CanBubble, IsCancelable, Ref<RTCRtpReceiver>&&, Ref<MediaStreamTrack>&&, MediaStreamArray&&, Ref<RTCRtpTransceiver>&&);

    struct Init : EventInit {
        Ref<RTCRtpReceiver> receiver;
        Ref<MediaStreamTrack> track;
        MediaStreamArray streams;
        Ref<RTCRtpTransceiver> transceiver;
    };
    static Ref<RTCTrackEvent> create(const AtomString& type, Init&&, IsTrusted = IsTrusted::No);

    RTCRtpReceiver& receiver() const { return m_receiver; }
    MediaStreamTrack& track() const  { return m_track; }
    const MediaStreamArray& streams() const  { return m_streams; }
    RTCRtpTransceiver& transceiver() const  { return m_transceiver; }

private:
    RTCTrackEvent(const AtomString& type, CanBubble, IsCancelable, Ref<RTCRtpReceiver>&&, Ref<MediaStreamTrack>&&, MediaStreamArray&&, Ref<RTCRtpTransceiver>&&);
    RTCTrackEvent(const AtomString& type, Init&&, IsTrusted);

    const Ref<RTCRtpReceiver> m_receiver;
    const Ref<MediaStreamTrack> m_track;
    MediaStreamArray m_streams;
    const Ref<RTCRtpTransceiver> m_transceiver;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
