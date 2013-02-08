/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SpeechSynthesis.h"

#if ENABLE(SPEECH_SYNTHESIS)

#include "PlatformSpeechSynthesis.h"
#include "PlatformSpeechSynthesisVoice.h"
#include "SpeechSynthesisUtterance.h"

namespace WebCore {
    
PassRefPtr<SpeechSynthesis> SpeechSynthesis::create()
{
    return adoptRef(new SpeechSynthesis());
}
    
SpeechSynthesis::SpeechSynthesis()
    : m_platformSpeechSynthesizer(PlatformSpeechSynthesizer(this))
{
}
    
void SpeechSynthesis::voicesDidChange()
{
    m_voiceList.clear();
}
    
const Vector<RefPtr<SpeechSynthesisVoice> >& SpeechSynthesis::getVoices()
{
    if (m_voiceList.size())
        return m_voiceList;
    
    // If the voiceList is empty, that's the cue to get the voices from the platform again.
    const Vector<RefPtr<PlatformSpeechSynthesisVoice> >& platformVoices = m_platformSpeechSynthesizer.voiceList();
    size_t voiceCount = platformVoices.size();
    for (size_t k = 0; k < voiceCount; k++)
        m_voiceList.append(SpeechSynthesisVoice::create(platformVoices[k]));

    return m_voiceList;
}

bool SpeechSynthesis::pending() const
{
    return false;
}

bool SpeechSynthesis::speaking() const
{
    return false;
}

bool SpeechSynthesis::paused() const
{
    return false;
}

void SpeechSynthesis::speak(SpeechSynthesisUtterance* utterance)
{
    m_platformSpeechSynthesizer.speak(utterance->platformUtterance());
}

void SpeechSynthesis::cancel()
{
}

void SpeechSynthesis::pause()
{
}

void SpeechSynthesis::resume()
{
}
    
} // namespace WebCore

#endif // ENABLE(INPUT_SPEECH)
