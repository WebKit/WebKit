/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LibWebRTCDTMFSenderBackend.h"

#if ENABLE(WEB_RTC)

#include <wtf/MainThread.h>

namespace WebCore {

static inline String toWTFString(const std::string& value)
{
    return String::fromUTF8(value.data(), value.length());
}

LibWebRTCDTMFSenderBackend::LibWebRTCDTMFSenderBackend(rtc::scoped_refptr<webrtc::DtmfSenderInterface>&& sender)
    : m_sender(WTFMove(sender))
{
    m_sender->RegisterObserver(this);
}

LibWebRTCDTMFSenderBackend::~LibWebRTCDTMFSenderBackend()
{
    m_sender->UnregisterObserver();
}

bool LibWebRTCDTMFSenderBackend::canInsertDTMF()
{
    return m_sender->CanInsertDtmf();
}

void LibWebRTCDTMFSenderBackend::playTone(const String& tone, size_t duration, size_t interToneGap)
{
    bool ok = m_sender->InsertDtmf(tone.utf8().data(), duration, interToneGap);
    ASSERT_UNUSED(ok, ok);
}

String LibWebRTCDTMFSenderBackend::tones() const
{
    return toWTFString(m_sender->tones());
}

size_t LibWebRTCDTMFSenderBackend::duration() const
{
    return m_sender->duration();
}

size_t LibWebRTCDTMFSenderBackend::interToneGap() const
{
    return m_sender->inter_tone_gap();
}

void LibWebRTCDTMFSenderBackend::OnToneChange(const std::string& tone, const std::string&)
{
    // We are just interested in notifying the end of the tone, which corresponds to the empty string.
    if (!tone.empty())
        return;
    callOnMainThread([this, weakThis = makeWeakPtr(*this), tone = toWTFString(tone)] {
        if (!weakThis)
            return;
        if (m_onTonePlayed)
            m_onTonePlayed(tone);
    });
}

void LibWebRTCDTMFSenderBackend::onTonePlayed(Function<void(const String&)>&& onTonePlayed)
{
    m_onTonePlayed = WTFMove(onTonePlayed);
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
