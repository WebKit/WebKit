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
#include <wtf/text/CString.h>

namespace WebCore {

PlatformSpeechSynthesisProviderEfl::PlatformSpeechSynthesisProviderEfl(PlatformSpeechSynthesizer* client)
    : m_isEngineStarted(false)
    , m_platformSpeechSynthesizer(client)
{
}

PlatformSpeechSynthesisProviderEfl::~PlatformSpeechSynthesisProviderEfl()
{
}

int PlatformSpeechSynthesisProviderEfl::convertRateToEspeakValue(float rate) const
{
    // The normal value that Espeak expects is 175, minimum is 80 and maximum 450
    return espeakRATE_NORMAL * rate;
}

int PlatformSpeechSynthesisProviderEfl::convertVolumeToEspeakValue(float volume) const
{
    // 0 = silence, 100 = normal, greater values may produce distortion
    return volume * 100;
}

int PlatformSpeechSynthesisProviderEfl::convertPitchToEspeakValue(float pitch) const
{
    // 0 = minimum, 50 = normal, 100 = maximum
    return pitch * 50;
}

String PlatformSpeechSynthesisProviderEfl::voiceName(PassRefPtr<PlatformSpeechSynthesisUtterance> utterance) const
{
    if (!m_platformSpeechSynthesizer)
        return String();

    if (!utterance->lang().isEmpty()) {
        const String& language = utterance->lang();
        const Vector<RefPtr<PlatformSpeechSynthesisVoice>>& voiceList = m_platformSpeechSynthesizer->voiceList();
        for (const auto& voice : voiceList) {
            // Espeak adds an empty character at the beginning of the language
            if (equalIgnoringASCIICase(StringView(voice->lang()).substring(1), language))
                return voice->name();
        }
    }

    espeak_VOICE* espeakVoice = currentVoice();
    ASSERT(espeakVoice);
    return ASCIILiteral(espeakVoice->name);
}

bool PlatformSpeechSynthesisProviderEfl::engineInit()
{
    if (!m_isEngineStarted) {
        if (!(m_isEngineStarted = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, 0, 0) != EE_INTERNAL_ERROR))
            return false;
        espeak_SetVoiceByName("default");
    }
    return true;
}

espeak_VOICE* PlatformSpeechSynthesisProviderEfl::currentVoice() const
{
    return espeak_GetCurrentVoice();
}

void PlatformSpeechSynthesisProviderEfl::initializeVoiceList(Vector<RefPtr<PlatformSpeechSynthesisVoice>>& voiceList)
{
    if (!engineInit()) {
        fireSpeechEvent(SpeechError);
        return;
    }

    espeak_VOICE* espeakVoice = currentVoice();
    ASSERT(espeakVoice);
    String currentLanguage = ASCIILiteral(espeakVoice->languages);

    const espeak_VOICE** voices = espeak_ListVoices(nullptr);
    if (!voices) {
        fireSpeechEvent(SpeechError);
        return;
    }

    // Voices array is terminated by the nullptr
    for (int i = 0; voices[i]; i++) {
        const espeak_VOICE* voice = voices[i];
        String id = ASCIILiteral(voice->identifier);
        String name = ASCIILiteral(voice->name);
        String language = ASCIILiteral(voice->languages);
        voiceList.append(PlatformSpeechSynthesisVoice::create(id, name, language, true, language == currentLanguage));
    }
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
    if (!engineInit() || !utterance) {
        fireSpeechEvent(SpeechError);
        return;
    }

    m_utterance = utterance;
    String voice = voiceName(m_utterance);
    espeak_SetVoiceByName(voice.utf8().data());
    espeak_SetParameter(espeakRATE, convertRateToEspeakValue(m_utterance->rate()), 0);
    espeak_SetParameter(espeakVOLUME, convertVolumeToEspeakValue(m_utterance->volume()), 0);
    espeak_SetParameter(espeakPITCH, convertPitchToEspeakValue(m_utterance->pitch()), 0);

    String textToRead = m_utterance->text();
    espeak_ERROR err = espeak_Synth(textToRead.utf8().data(), textToRead.length(), 0, POS_CHARACTER, 0, espeakCHARS_AUTO, 0, nullptr);
    if (err == EE_INTERNAL_ERROR) {
        fireSpeechEvent(SpeechError);
        m_utterance = nullptr;
        return;
    }

    fireSpeechEvent(SpeechStart);
}

void PlatformSpeechSynthesisProviderEfl::cancel()
{
    if (!m_isEngineStarted || !m_utterance)
        return;

    if (espeak_Cancel() == EE_INTERNAL_ERROR) {
        fireSpeechEvent(SpeechError);
        m_utterance = nullptr;
        return;
    }
    fireSpeechEvent(SpeechCancel);
    m_utterance = nullptr;
}

void PlatformSpeechSynthesisProviderEfl::fireSpeechEvent(SpeechEvent speechEvent)
{
    switch (speechEvent) {
    case SpeechStart:
        m_platformSpeechSynthesizer->client()->didStartSpeaking(m_utterance);
        break;
    case SpeechPause:
        m_platformSpeechSynthesizer->client()->didPauseSpeaking(m_utterance);
        break;
    case SpeechResume:
        m_platformSpeechSynthesizer->client()->didResumeSpeaking(m_utterance);
        break;
    case SpeechError:
        m_isEngineStarted = false;
    case SpeechCancel:
        m_platformSpeechSynthesizer->client()->speakingErrorOccurred(m_utterance);
        break;
    default:
        ASSERT_NOT_REACHED();
    };
}

} // namespace WebCore

#endif
