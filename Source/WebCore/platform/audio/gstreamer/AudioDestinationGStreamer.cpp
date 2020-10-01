/*
 *  Copyright (C) 2011, 2012 Igalia S.L
 *  Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
#include "AudioUtilities.h"
#include "GStreamerCommon.h"
#include "Logging.h"
#include "WebKitAudioSinkGStreamer.h"
#include "WebKitWebAudioSourceGStreamer.h"
#include <gst/audio/gstaudiobasesink.h>
#include <gst/gst.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebCore {

gboolean messageCallback(GstBus*, GstMessage* message, AudioDestinationGStreamer* destination)
{
    return destination->handleMessage(message);
}

static void autoAudioSinkChildAddedCallback(GstChildProxy*, GObject* object, gchar*, gpointer)
{
    if (GST_IS_AUDIO_BASE_SINK(object))
        g_object_set(GST_AUDIO_BASE_SINK(object), "buffer-time", static_cast<gint64>(100000), nullptr);
}

Ref<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    // FIXME: Add support for multi-channel (> stereo) output.
    if (numberOfOutputChannels != 2)
        LOG(Media, "AudioDestination::create(%u, %u, %f) - unhandled output channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    return adoptRef(*new AudioDestinationGStreamer(callback, sampleRate));
}

float AudioDestination::hardwareSampleRate()
{
    return 44100;
}

unsigned long AudioDestination::maxChannelCount()
{
    // FIXME: query the default audio hardware device to return the actual number
    // of channels of the device. Also see corresponding FIXME in create().
    return 0;
}

AudioDestinationGStreamer::AudioDestinationGStreamer(AudioIOCallback& callback, float sampleRate)
    : m_callback(callback)
    , m_renderBus(AudioBus::create(2, AudioUtilities::renderQuantumSize, false))
    , m_sampleRate(sampleRate)
    , m_isPlaying(false)
{
    m_pipeline = gst_pipeline_new("play");
    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline)));
    ASSERT(bus);
    gst_bus_add_signal_watch_full(bus.get(), RunLoopSourcePriority::RunLoopDispatcher);
    g_signal_connect(bus.get(), "message", G_CALLBACK(messageCallback), this);

    GstElement* webkitAudioSrc = reinterpret_cast<GstElement*>(g_object_new(WEBKIT_TYPE_WEB_AUDIO_SRC,
                                                                            "rate", sampleRate,
                                                                            "bus", m_renderBus.get(),
                                                                            "provider", &m_callback,
                                                                            "frames", AudioUtilities::renderQuantumSize, nullptr));

    GRefPtr<GstElement> audioSink = createPlatformAudioSink();
    m_audioSinkAvailable = audioSink;
    if (!audioSink) {
        LOG_ERROR("Failed to create GStreamer audio sink element");
        return;
    }

    // Probe platform early on for a working audio output device. This is not needed for the WebKit
    // custom audio sink because it doesn't rely on autoaudiosink.
    if (!WEBKIT_IS_AUDIO_SINK(audioSink.get())) {
        g_signal_connect(audioSink.get(), "child-added", G_CALLBACK(autoAudioSinkChildAddedCallback), nullptr);

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
    }

    GstElement* audioConvert = gst_element_factory_make("audioconvert", nullptr);
    GstElement* audioResample = gst_element_factory_make("audioresample", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_pipeline), webkitAudioSrc, audioConvert, audioResample, audioSink.get(), nullptr);

    // Link src pads from webkitAudioSrc to audioConvert ! audioResample ! autoaudiosink.
    gst_element_link_pads_full(webkitAudioSrc, "src", audioConvert, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioConvert, "src", audioResample, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample, "src", audioSink.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
}

AudioDestinationGStreamer::~AudioDestinationGStreamer()
{
    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline)));
    ASSERT(bus);
    g_signal_handlers_disconnect_by_func(bus.get(), reinterpret_cast<gpointer>(messageCallback), this);
    gst_bus_remove_signal_watch(bus.get());

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(m_pipeline);
}

unsigned AudioDestinationGStreamer::framesPerBuffer() const
{
    return AudioUtilities::renderQuantumSize;
}

gboolean AudioDestinationGStreamer::handleMessage(GstMessage* message)
{
    GUniqueOutPtr<GError> error;
    GUniqueOutPtr<gchar> debug;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message, &error.outPtr(), &debug.outPtr());
        g_warning("Warning: %d, %s. Debug output: %s", error->code,  error->message, debug.get());
        break;
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &error.outPtr(), &debug.outPtr());
        g_warning("Error: %d, %s. Debug output: %s", error->code,  error->message, debug.get());
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        m_isPlaying = false;
        break;
    default:
        break;
    }
    return TRUE;
}

void AudioDestinationGStreamer::start(Function<void(Function<void()>&&)>&&)
{
    ASSERT(m_audioSinkAvailable);
    if (!m_audioSinkAvailable)
        return;

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_warning("Error: Failed to set pipeline to playing");
        m_isPlaying = false;
        return;
    }

    m_isPlaying = true;
}

void AudioDestinationGStreamer::stop()
{
    ASSERT(m_audioSinkAvailable);
    if (!m_audioSinkAvailable)
        return;

    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    m_isPlaying = false;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
