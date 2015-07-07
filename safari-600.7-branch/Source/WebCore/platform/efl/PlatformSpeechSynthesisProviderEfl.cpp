/*
 * Copyright (C) 2014 Samsung Electronics
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformSpeechSynthesisProviderEfl.h"

#if ENABLE(SPEECH_SYNTHESIS)

#include <NotImplemented.h>
#include <PlatformSpeechSynthesisUtterance.h>
#include <PlatformSpeechSynthesisVoice.h>
#include <PlatformSpeechSynthesizer.h>

namespace WebCore {

PlatformSpeechSynthesisProviderEfl::PlatformSpeechSynthesisProviderEfl(PlatformSpeechSynthesizer* platformSpeechSynthesizer)
    : m_platformSpeechSynthesizer(platformSpeechSynthesizer)
{
}

PlatformSpeechSynthesisProviderEfl::~PlatformSpeechSynthesisProviderEfl()
{
}

void PlatformSpeechSynthesisProviderEfl::initializeVoiceList(Vector<RefPtr<PlatformSpeechSynthesisVoice>>& voiceList)
{
    UNUSED_PARAM(voiceList);
    notImplemented();
}

void PlatformSpeechSynthesisProviderEfl::pause()
{
    notImplemented();
}

void PlatformSpeechSynthesisProviderEfl::resume()
{
    notImplemented();
}

void PlatformSpeechSynthesisProviderEfl::speak(PassRefPtr<PlatformSpeechSynthesisUtterance> utterance)
{
    UNUSED_PARAM(utterance);
    notImplemented();
}

void PlatformSpeechSynthesisProviderEfl::cancel()
{
    notImplemented();
}

} // namespace WebCore

#endif
