/*
 * Copyright (C) 2023 Igalia S.L. All rights reserved.
 * Copyright (C) 2023 ChangSeok Oh <changseok@webkit.org>
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

#if ENABLE(SPEECH_SYNTHESIS) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "PlatformSpeechSynthesisUtterance.h"
#include "PlatformSpeechSynthesisVoice.h"
#include "WebKitAudioSinkGStreamer.h"
#include "WebKitFliteSourceGStreamer.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

class GstSpeechSynthesisWrapper {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GstSpeechSynthesisWrapper);
    WTF_MAKE_NONCOPYABLE(GstSpeechSynthesisWrapper);
public:
    explicit GstSpeechSynthesisWrapper(const PlatformSpeechSynthesizer&);
    ~GstSpeechSynthesisWrapper();

    void pause();
    void resume();
    void speakUtterance(RefPtr<PlatformSpeechSynthesisUtterance>&&);
    void cancel();
    void resetState();

private:
    bool handleMessage(GstMessage*);

    RefPtr<PlatformSpeechSynthesisUtterance> m_utterance;
    const PlatformSpeechSynthesizer& m_platformSynthesizer;
    GRefPtr<GstElement> m_pipeline;
    GRefPtr<GstElement> m_src;
    GRefPtr<GstElement> m_volumeElement;
    GRefPtr<GstElement> m_pitchElement;
};

GstSpeechSynthesisWrapper::GstSpeechSynthesisWrapper(const PlatformSpeechSynthesizer& synthesizer)
    : m_platformSynthesizer(synthesizer)
{
    ensureGStreamerInitialized();
    registerWebKitGStreamerElements();

    static Atomic<uint32_t> pipelineId;
    m_pipeline = gst_pipeline_new(makeString("speech-synthesizer-"_s, pipelineId.exchangeAdd(1)).ascii().data());
    registerActivePipeline(m_pipeline);
    connectSimpleBusMessageCallback(m_pipeline.get(), [this](GstMessage* message) {
        this->handleMessage(message);
    });

    m_src = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_FLITE_SRC, nullptr));
    GRefPtr<GstElement> audioSink = createPlatformAudioSink("speech"_s);
    if (!audioSink) {
        GST_ERROR("Failed to create GStreamer audio sink element");
        return;
    }

    if (!WEBKIT_IS_AUDIO_SINK(audioSink.get())) {
        g_signal_connect(audioSink.get(), "child-added", G_CALLBACK(+[](GstChildProxy*, GObject* object, gchar*, gpointer) {
            if (GST_IS_AUDIO_BASE_SINK(object))
                g_object_set(GST_AUDIO_BASE_SINK(object), "buffer-time", static_cast<gint64>(100000), nullptr);
        }), nullptr);

        // Autoaudiosink does the real sink detection in the GST_STATE_NULL->READY transition
        // so it's best to roll it to READY as soon as possible to ensure the underlying platform
        // audiosink was loaded correctly.
        GstStateChangeReturn stateChangeReturn = gst_element_set_state(audioSink.get(), GST_STATE_READY);
        if (stateChangeReturn == GST_STATE_CHANGE_FAILURE) {
            GST_ERROR("Failed to change autoaudiosink element state");
            gst_element_set_state(audioSink.get(), GST_STATE_NULL);
            return;
        }
    }

    m_volumeElement = makeGStreamerElement("volume", nullptr);
    m_pitchElement = makeGStreamerElement("pitch", nullptr);
    if (!m_pitchElement)
        WTFLogAlways("The pitch GStreamer plugin is unavailable. The pitch property of Speech Synthesis is ignored.");

    GRefPtr<GstElement> audioConvert = makeGStreamerElement("audioconvert", nullptr);
    GRefPtr<GstElement> audioResample = makeGStreamerElement("audioresample", nullptr);
    if (m_pitchElement)
        gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), m_volumeElement.get(), audioConvert.get(), audioResample.get(), m_pitchElement.get(), audioSink.get(), nullptr);
    else
        gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), m_volumeElement.get(), audioConvert.get(), audioResample.get(), audioSink.get(), nullptr);

    gst_element_link_pads_full(m_src.get(), "src", m_volumeElement.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(m_volumeElement.get(), "src", audioConvert.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioConvert.get(), "src", audioResample.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
    if (m_pitchElement) {
        gst_element_link_pads_full(audioResample.get(), "src", m_pitchElement.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
        gst_element_link_pads_full(m_pitchElement.get(), "src", audioSink.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
    } else
        gst_element_link_pads_full(audioResample.get(), "src", audioSink.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
}

GstSpeechSynthesisWrapper::~GstSpeechSynthesisWrapper()
{
    unregisterPipeline(m_pipeline);
    disconnectSimpleBusMessageCallback(m_pipeline.get());
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
}

void GstSpeechSynthesisWrapper::pause()
{
    if (!m_utterance)
        return;

    webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_PAUSED, [this](GstMessage* message) -> bool {
        return handleMessage(message);
    });
    m_platformSynthesizer.client().didPauseSpeaking(*m_utterance);
}

void GstSpeechSynthesisWrapper::resume()
{
    if (!m_utterance)
        return;

    webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_PLAYING, [this](GstMessage* message) -> bool {
        return handleMessage(message);
    });
    m_platformSynthesizer.client().didResumeSpeaking(*m_utterance);
}

void GstSpeechSynthesisWrapper::speakUtterance(RefPtr<PlatformSpeechSynthesisUtterance>&& utterance)
{
    ASSERT(!m_utterance);
    ASSERT(utterance);
    if (!utterance)
        return;

    m_utterance = WTFMove(utterance);
    webKitFliteSrcSetUtterance(WEBKIT_FLITE_SRC(m_src.get()), m_utterance->voice(), m_utterance->text());

    // The pitch element does not handle the rate bigger than 1.0. We control
    // the rate with a seek event instead.
    gst_element_seek(m_src.get(), m_utterance->rate(),
        GST_FORMAT_TIME, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, toGstClockTime(MediaTime::zeroTime()),
        GST_SEEK_TYPE_SET, toGstClockTime(MediaTime::invalidTime()));
    if (m_pitchElement)
        g_object_set(m_pitchElement.get(), "pitch", m_utterance->pitch(), nullptr);
    g_object_set(m_volumeElement.get(), "volume", static_cast<double>(m_utterance->volume()), nullptr);

    webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_PLAYING, [this](GstMessage* message) -> bool {
        return handleMessage(message);
    });

    m_platformSynthesizer.client().didStartSpeaking(*m_utterance);
}

void GstSpeechSynthesisWrapper::cancel()
{
    if (!m_utterance)
        return;

    webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_READY, [this](GstMessage* message) -> bool {
        return handleMessage(message);
    });
    m_platformSynthesizer.client().didFinishSpeaking(*m_utterance);
}

void GstSpeechSynthesisWrapper::resetState()
{
    cancel();
    m_utterance = nullptr;
}

bool GstSpeechSynthesisWrapper::handleMessage(GstMessage* message)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_EOS:
        resetState();
        break;
    default:
        break;
    }
    return true;
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
    Vector<Ref<PlatformSpeechSynthesisVoice>>& voiceList = ensureFliteVoicesInitialized();
    for (auto& voice : voiceList)
        m_voiceList.append(PlatformSpeechSynthesisVoice::create(voice->voiceURI(), voice->name(), voice->lang(), voice->localService(), voice->isDefault()));
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
    if (!m_platformSpeechWrapper)
        m_platformSpeechWrapper = makeUnique<GstSpeechSynthesisWrapper>(*this);

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
    m_platformSpeechWrapper->resetState();
}

} // namespace WebCore

#endif // ENABLE(SPEECH_SYNTHESIS) && USE(GSTREAMER)
