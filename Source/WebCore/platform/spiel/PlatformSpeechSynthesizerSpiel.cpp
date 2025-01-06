/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformSpeechSynthesizer.h"

#if USE(SPIEL)

#include "GStreamerCommon.h"
#include "PlatformSpeechSynthesisUtterance.h"
#include "PlatformSpeechSynthesisVoice.h"
#include "WebKitAudioSinkGStreamer.h"
#include <spiel/spiel.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

#define URI_PREFIX "urn:webkit-tts:spiel:"_s

GST_DEBUG_CATEGORY(webkit_spiel_debug);
#define GST_CAT_DEFAULT webkit_spiel_debug

class SpielSpeechWrapper {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(SpielSpeechWrapper);
    WTF_MAKE_NONCOPYABLE(SpielSpeechWrapper);
public:
    explicit SpielSpeechWrapper(const PlatformSpeechSynthesizer&, Function<void()>&&);
    ~SpielSpeechWrapper();

    Vector<RefPtr<PlatformSpeechSynthesisVoice>> initializeVoiceList();
    void pause();
    void resume();
    void speakUtterance(RefPtr<PlatformSpeechSynthesisUtterance>&&);
    void cancel();
    void clearUtterance();

private:
    void finishSpeakerInitialization();
    String generateVoiceURI(const GRefPtr<SpielVoice>&, const String& language);

    const PlatformSpeechSynthesizer& m_platformSynthesizer;
    Function<void()> m_speakerCreatedCallback;
    GRefPtr<GstElement> m_sink;
    GRefPtr<SpielSpeaker> m_speaker;
    HashMap<String, GRefPtr<SpielVoice>> m_voices;
    RefPtr<PlatformSpeechSynthesisUtterance> m_utterance;
};

SpielSpeechWrapper::SpielSpeechWrapper(const PlatformSpeechSynthesizer& synthesizer, Function<void()>&& speakerCreatedCallback)
    : m_platformSynthesizer(synthesizer)
    , m_speakerCreatedCallback(WTFMove(speakerCreatedCallback))
{
    ensureGStreamerInitialized();
    registerWebKitGStreamerElements();

    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_spiel_debug, "webkitspiel", 0, "WebKit Spiel");
    });

    m_sink = createPlatformAudioSink("speech"_s);
    if (!m_sink) {
        GST_ERROR("Failed to create GStreamer audio sink element");
        return;
    }

    spiel_speaker_new(nullptr, [](GObject*, GAsyncResult* result, gpointer userData) {
        auto self = reinterpret_cast<SpielSpeechWrapper*>(userData);
        GUniqueOutPtr<GError> error;
        self->m_speaker = adoptGRef(spiel_speaker_new_finish(result, &error.outPtr()));
        if (error) {
            WTFLogAlways("Spiel speaker failed to initialize: %s", error->message);
            GST_ERROR("Spiel speaker failed to initialize: %s", error->message);
            return;
        }
        self->finishSpeakerInitialization();
    }, this);
}

void SpielSpeechWrapper::finishSpeakerInitialization()
{
    g_object_set(m_speaker.get(), "sink", m_sink.get(), nullptr);

    // TODO: Plumb support for boundaryEventOccurred? Using range-started signal?

    g_signal_connect_swapped(m_speaker.get(), "utterance-started", G_CALLBACK(+[](SpielSpeechWrapper* self, SpielUtterance*) {
        self->m_platformSynthesizer.client().didStartSpeaking(*self->m_utterance);
    }), this);

    g_signal_connect_swapped(m_speaker.get(), "utterance-finished", G_CALLBACK(+[](SpielSpeechWrapper* self, SpielUtterance*) {
        self->m_platformSynthesizer.client().didFinishSpeaking(*self->m_utterance);
        self->clearUtterance();
    }), this);

    g_signal_connect_swapped(m_speaker.get(), "utterance-canceled", G_CALLBACK(+[](SpielSpeechWrapper* self, SpielUtterance*) {
        self->m_platformSynthesizer.client().didFinishSpeaking(*self->m_utterance);
        self->clearUtterance();
    }), this);

    g_signal_connect_swapped(m_speaker.get(), "utterance-error", G_CALLBACK(+[](SpielSpeechWrapper* self, SpielUtterance*) {
        self->m_platformSynthesizer.client().speakingErrorOccurred(*self->m_utterance);
        self->clearUtterance();
    }), this);

    g_signal_connect_swapped(m_speaker.get(), "notify::paused", G_CALLBACK(+[](SpielSpeechWrapper* self, SpielSpeaker* speaker) {
        gboolean isPaused;
        g_object_get(speaker, "paused", &isPaused, nullptr);
        if (isPaused)
            self->m_platformSynthesizer.client().didPauseSpeaking(*self->m_utterance);
        else
            self->m_platformSynthesizer.client().didResumeSpeaking(*self->m_utterance);
    }), this);

    g_signal_connect_swapped(m_speaker.get(), "notify::voices", G_CALLBACK(+[](SpielSpeechWrapper* self, SpielSpeaker*) {
        self->m_platformSynthesizer.client().voicesDidChange();
    }), this);

    m_speakerCreatedCallback();
}

SpielSpeechWrapper::~SpielSpeechWrapper()
{
    if (m_speaker)
        g_signal_handlers_disconnect_by_data(m_speaker.get(), this);
}

String SpielSpeechWrapper::generateVoiceURI(const GRefPtr<SpielVoice>& voice, const String& language)
{
    auto provider = adoptGRef(spiel_voice_get_provider(voice.get()));
    return makeString(URI_PREFIX, span(spiel_provider_get_well_known_name(provider.get())), '#', span(spiel_voice_get_identifier(voice.get())), '#', language);
}

Vector<RefPtr<PlatformSpeechSynthesisVoice>> SpielSpeechWrapper::initializeVoiceList()
{
    Vector<RefPtr<PlatformSpeechSynthesisVoice>> platformVoices;
    auto voices = spiel_speaker_get_voices(m_speaker.get());
    unsigned position = 0;
    m_voices.clear();
    while (auto item = g_list_model_get_item(voices, position++)) {
        auto voice = SPIEL_VOICE(item);
        auto name = makeString(span(spiel_voice_get_name(voice)));
        auto isDefault = true;
        const auto languages = span(const_cast<char**>(spiel_voice_get_languages(voice)));
        for (const auto language : languages) {
            auto languageString = makeString(span(language));
            auto uri = generateVoiceURI(voice, languageString);
            platformVoices.append(PlatformSpeechSynthesisVoice::create(uri, name, languageString, false, isDefault));
            m_voices.add(uri, GRefPtr(voice));
            isDefault = false;
        }
    }
    return platformVoices;
}

void SpielSpeechWrapper::pause()
{
    if (!m_utterance)
        return;

    spiel_speaker_pause(m_speaker.get());
}

void SpielSpeechWrapper::resume()
{
    if (!m_utterance)
        return;

    spiel_speaker_resume(m_speaker.get());
}

void SpielSpeechWrapper::speakUtterance(RefPtr<PlatformSpeechSynthesisUtterance>&& utterance)
{
    ASSERT(!m_utterance);
    ASSERT(utterance);
    if (!utterance) {
        GST_ERROR("Utterance is null");
        ASSERT_NOT_REACHED();
        return;
    }

    if (!utterance->voice()) {
        m_platformSynthesizer.client().didFinishSpeaking(*utterance);
        return;
    }

    const auto& uri = utterance->voice()->voiceURI();
    if (!m_voices.contains(uri)) {
        GST_ERROR("Unknown voice URI: %s", uri.utf8().data());
        ASSERT_NOT_REACHED();
        return;
    }

    // TODO: Detect whether the utterance text is XML and enable SSML if that is the case.
    auto voice = m_voices.get(uri);
    auto spielUtterance = adoptGRef(spiel_utterance_new(utterance->text().utf8().data()));
    spiel_utterance_set_language(spielUtterance.get(), utterance->lang().utf8().data());
    spiel_utterance_set_voice(spielUtterance.get(), voice);
    spiel_utterance_set_volume(spielUtterance.get(), utterance->volume());
    spiel_utterance_set_pitch(spielUtterance.get(), utterance->pitch());
    spiel_utterance_set_rate(spielUtterance.get(), utterance->rate());
    m_utterance = WTFMove(utterance);
    spiel_speaker_speak(m_speaker.get(), spielUtterance.get());
}

void SpielSpeechWrapper::cancel()
{
    if (!m_utterance)
        return;

    spiel_speaker_cancel(m_speaker.get());
}

void SpielSpeechWrapper::clearUtterance()
{
    m_utterance = nullptr;
}

Ref<PlatformSpeechSynthesizer> PlatformSpeechSynthesizer::create(PlatformSpeechSynthesizerClient& client)
{
    return adoptRef(*new PlatformSpeechSynthesizer(client));
}

PlatformSpeechSynthesizer::PlatformSpeechSynthesizer(PlatformSpeechSynthesizerClient& client)
    : m_speechSynthesizerClient(client)
{
}

PlatformSpeechSynthesizer::~PlatformSpeechSynthesizer() = default;

void PlatformSpeechSynthesizer::initializeVoiceList()
{
    if (!m_platformSpeechWrapper) {
        m_platformSpeechWrapper = makeUnique<SpielSpeechWrapper>(*this, [&] {
            m_voiceList = m_platformSpeechWrapper->initializeVoiceList();
            client().voicesDidChange();
        });
        return;
    }
    m_voiceList = m_platformSpeechWrapper->initializeVoiceList();
}

void PlatformSpeechSynthesizer::pause()
{
    if (!m_platformSpeechWrapper)
        return;
    m_platformSpeechWrapper->pause();
}

void PlatformSpeechSynthesizer::resume()
{
    if (!m_platformSpeechWrapper)
        return;
    m_platformSpeechWrapper->resume();
}

void PlatformSpeechSynthesizer::speak(RefPtr<PlatformSpeechSynthesisUtterance>&& utterance)
{
    if (!m_platformSpeechWrapper) {
        m_platformSpeechWrapper = makeUnique<SpielSpeechWrapper>(*this, [&, utterance = WTFMove(utterance)]() mutable {
            m_platformSpeechWrapper->speakUtterance(WTFMove(utterance));
        });
        return;
    }
    m_platformSpeechWrapper->speakUtterance(WTFMove(utterance));
}

void PlatformSpeechSynthesizer::cancel()
{
    if (!m_platformSpeechWrapper)
        return;
    m_platformSpeechWrapper->cancel();
}

void PlatformSpeechSynthesizer::resetState()
{
    if (!m_platformSpeechWrapper)
        return;
    m_platformSpeechWrapper->cancel();
}

#undef GST_CAT_DEFAULT
#undef URI_PREFIX

} // namespace WebCore

#endif // USE(SPIEL)
