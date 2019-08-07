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
#include "RTCDTMFSender.h"

#if ENABLE(WEB_RTC)

#include "RTCDTMFSenderBackend.h"
#include "RTCDTMFToneChangeEvent.h"
#include "ScriptExecutionContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RTCDTMFSender);

static const size_t minToneDurationMs = 40;
static const size_t maxToneDurationMs = 6000;
static const size_t minInterToneGapMs = 30;

RTCDTMFSender::RTCDTMFSender(ScriptExecutionContext& context, RTCRtpSender& sender, std::unique_ptr<RTCDTMFSenderBackend>&& backend)
    : ActiveDOMObject(&context)
    , m_toneTimer(*this, &RTCDTMFSender::toneTimerFired)
    , m_sender(makeWeakPtr(sender))
    , m_backend(WTFMove(backend))
{
    m_backend->onTonePlayed([this](const String&) {
        onTonePlayed();
    });
    suspendIfNeeded();
}

RTCDTMFSender::~RTCDTMFSender() = default;

bool RTCDTMFSender::canInsertDTMF() const
{
    if (!m_sender || m_sender->isStopped())
        return false;

    auto currentDirection = m_sender->currentTransceiverDirection();
    if (!currentDirection)
        return false;
    if (*currentDirection != RTCRtpTransceiverDirection::Sendrecv && *currentDirection != RTCRtpTransceiverDirection::Sendonly)
        return false;

    return m_backend && m_backend->canInsertDTMF();
}

String RTCDTMFSender::toneBuffer() const
{
    return m_tones;
}

static inline bool isToneCharacterInvalid(UChar character)
{
    if (character >= '0' && character <= '9')
        return false;
    if (character >= 'A' && character <= 'D')
        return false;
    return character != '#' && character != '*' && character != ',';
}

ExceptionOr<void> RTCDTMFSender::insertDTMF(const String& tones, size_t duration, size_t interToneGap)
{
    if (!canInsertDTMF())
        return Exception { InvalidStateError, "Cannot insert DTMF"_s };

    auto normalizedTones = tones.convertToUppercaseWithoutLocale();
    if (normalizedTones.find(isToneCharacterInvalid) != notFound)
        return Exception { InvalidCharacterError, "Tones are not valid"_s };

    m_tones = WTFMove(normalizedTones);
    m_duration = clampTo(duration, minToneDurationMs, maxToneDurationMs);
    m_interToneGap = std::max(interToneGap, minInterToneGapMs);

    if (m_tones.isEmpty() || m_isPendingPlayoutTask)
        return { };

    m_isPendingPlayoutTask = true;
    scriptExecutionContext()->postTask([this, protectedThis = makeRef(*this)](auto&) {
        playNextTone();
    });
    return { };
}

void RTCDTMFSender::playNextTone()
{
    if (m_tones.isEmpty()) {
        m_isPendingPlayoutTask = false;
        dispatchEvent(RTCDTMFToneChangeEvent::create({ }));
        return;
    }

    if (!canInsertDTMF()) {
        m_isPendingPlayoutTask = false;
        return;
    }

    auto currentTone = m_tones.substring(0, 1);
    m_tones.remove(0);

    m_backend->playTone(currentTone, m_duration, m_interToneGap);
    dispatchEvent(RTCDTMFToneChangeEvent::create(currentTone));
}

void RTCDTMFSender::onTonePlayed()
{
    m_toneTimer.startOneShot(1_ms * m_interToneGap);
}

void RTCDTMFSender::toneTimerFired()
{
    playNextTone();
}

void RTCDTMFSender::stop()
{
    m_backend = nullptr;
    m_toneTimer.stop();
}

const char* RTCDTMFSender::activeDOMObjectName() const
{
    return "RTCDTMFSender";
}

bool RTCDTMFSender::canSuspendForDocumentSuspension() const
{
    return !m_sender || m_sender->isStopped();
}

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
