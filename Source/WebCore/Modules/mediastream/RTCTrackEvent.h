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

typedef Vector<RefPtr<MediaStream>> MediaStreamArray;

class RTCTrackEvent : public Event {
public:
    static Ref<RTCTrackEvent> create(const AtomString& type, CanBubble, IsCancelable, RefPtr<RTCRtpReceiver>&&, RefPtr<MediaStreamTrack>&&, MediaStreamArray&&, RefPtr<RTCRtpTransceiver>&&);

    struct Init : EventInit {
        RefPtr<RTCRtpReceiver> receiver;
        RefPtr<MediaStreamTrack> track;
        MediaStreamArray streams;
        RefPtr<RTCRtpTransceiver> transceiver;
    };
    static Ref<RTCTrackEvent> create(const AtomString& type, const Init&, IsTrusted = IsTrusted::No);

    RTCRtpReceiver* receiver() const { return m_receiver.get(); }
    MediaStreamTrack* track() const  { return m_track.get(); }
    const MediaStreamArray& streams() const  { return m_streams; }
    RTCRtpTransceiver* transceiver() const  { return m_transceiver.get(); }

    virtual EventInterface eventInterface() const { return RTCTrackEventInterfaceType; }

private:
    RTCTrackEvent(const AtomString& type, CanBubble, IsCancelable, RefPtr<RTCRtpReceiver>&&, RefPtr<MediaStreamTrack>&&, MediaStreamArray&&, RefPtr<RTCRtpTransceiver>&&);
    RTCTrackEvent(const AtomString& type, const Init&, IsTrusted);

    RefPtr<RTCRtpReceiver> m_receiver;
    RefPtr<MediaStreamTrack> m_track;
    MediaStreamArray m_streams;
    RefPtr<RTCRtpTransceiver> m_transceiver;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
