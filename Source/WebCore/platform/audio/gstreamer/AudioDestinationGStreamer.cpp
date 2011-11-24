/*
 *  Copyright (C) 2011 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "AudioDestinationGStreamer.h"

#include "AudioChannel.h"
#include "AudioSourceProvider.h"
#include "GOwnPtr.h"
#include "GRefPtrGStreamer.h"
#include "WebKitWebAudioSourceGStreamer.h"
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

namespace WebCore {

// Size of the AudioBus for playback. The webkitwebaudiosrc element
// needs to handle this number of frames per cycle as well.
const unsigned framesToPull = 128;

PassOwnPtr<AudioDestination> AudioDestination::create(AudioSourceProvider& provider, float sampleRate)
{
    return adoptPtr(new AudioDestinationGStreamer(provider, sampleRate));
}

float AudioDestination::hardwareSampleRate()
{
    return 44100;
}

static void onGStreamerWavparsePadAddedCallback(GstElement* element, GstPad* pad, AudioDestinationGStreamer* destination)
{
    destination->finishBuildingPipelineAfterWavParserPadReady(pad);
}

AudioDestinationGStreamer::AudioDestinationGStreamer(AudioSourceProvider& provider, float sampleRate)
    : m_provider(provider)
    , m_renderBus(2, framesToPull, true)
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
    static bool gstInitialized = false;
    if (!gstInitialized)
        gstInitialized = gst_init_check(0, 0, 0);
    ASSERT_WITH_MESSAGE(gstInitialized, "GStreamer initialization failed");

    m_pipeline = gst_pipeline_new("play");

    GstElement* webkitAudioSrc = reinterpret_cast<GstElement*>(g_object_new(WEBKIT_TYPE_WEB_AUDIO_SRC,
                                                                            "rate", sampleRate,
                                                                            "bus", &m_renderBus,
                                                                            "provider", &m_provider,
                                                                            "frames", framesToPull, NULL));

    GstElement* wavParser = gst_element_factory_make("wavparse", 0);

    m_wavParserAvailable = wavParser;
    ASSERT_WITH_MESSAGE(m_wavParserAvailable, "Failed to create GStreamer wavparse element");
    if (!m_wavParserAvailable)
        return;

    g_signal_connect(wavParser, "pad-added", G_CALLBACK(onGStreamerWavparsePadAddedCallback), this);
    gst_bin_add_many(GST_BIN(m_pipeline), webkitAudioSrc, wavParser, NULL);
    gst_element_link_pads_full(webkitAudioSrc, "src", wavParser, "sink", GST_PAD_LINK_CHECK_NOTHING);
}

AudioDestinationGStreamer::~AudioDestinationGStreamer()
{
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(m_pipeline);
}

void AudioDestinationGStreamer::finishBuildingPipelineAfterWavParserPadReady(GstPad* pad)
{
    ASSERT(m_wavParserAvailable);

    GRefPtr<GstElement> audioSink = gst_element_factory_make("autoaudiosink", 0);
    m_audioSinkAvailable = audioSink;

    if (!audioSink) {
        LOG_ERROR("Failed to create GStreamer autoaudiosink element");
        return;
    }

    // Autoaudiosink does the real sink detection in the GST_STATE_NULL->READY transition
    // so it's best to roll it to READY as soon as possible to ensure the underlying platform
    // audiosink was loaded correctly.
    GstStateChangeReturn stateChangeReturn = gst_element_set_state(audioSink.get(), GST_STATE_READY);
    if (stateChangeReturn == GST_STATE_CHANGE_FAILURE) {
        LOG_ERROR("Failed to change autoaudiosink element state");
        gst_element_set_state(audioSink.get(), GST_STATE_NULL);
        m_audioSinkAvailable = false;
        return;
    }

    GstElement* audioConvert = gst_element_factory_make("audioconvert", 0);
    gst_bin_add_many(GST_BIN(m_pipeline), audioConvert, audioSink.get(), NULL);

    // Link wavparse's src pad to audioconvert sink pad.
    GRefPtr<GstPad> sinkPad = adoptGRef(gst_element_get_static_pad(audioConvert, "sink"));
    gst_pad_link(pad, sinkPad.get());

    // Link audioconvert to audiosink and roll states.
    gst_element_link_pads_full(audioConvert, "src", audioSink.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_sync_state_with_parent(audioConvert);
    gst_element_sync_state_with_parent(audioSink.leakRef());
}

void AudioDestinationGStreamer::start()
{
    ASSERT(m_wavParserAvailable);
    if (!m_wavParserAvailable)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    m_isPlaying = true;
}

void AudioDestinationGStreamer::stop()
{
    ASSERT(m_wavParserAvailable && m_audioSinkAvailable);
    if (!m_wavParserAvailable || m_audioSinkAvailable)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    m_isPlaying = false;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
