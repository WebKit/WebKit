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
#include <wtf/PrintStream.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_audio_destination_debug);
#define GST_CAT_DEFAULT webkit_audio_destination_debug

static void initializeDebugCategory()
{
    ensureGStreamerInitialized();
    registerWebKitGStreamerElements();

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_audio_destination_debug, "webkitaudiodestination", 0, "WebKit WebAudio Destination");
    });
}

static unsigned long maximumNumberOfOutputChannels()
{
    initializeDebugCategory();

    static int count = 0;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        auto monitor = adoptGRef(gst_device_monitor_new());
        auto caps = adoptGRef(gst_caps_new_empty_simple("audio/x-raw"));
        gst_device_monitor_add_filter(monitor.get(), "Audio/Sink", caps.get());
        gst_device_monitor_start(monitor.get());
        auto* devices = gst_device_monitor_get_devices(monitor.get());
        while (devices) {
            auto device = adoptGRef(GST_DEVICE_CAST(devices->data));
            auto caps = adoptGRef(gst_device_get_caps(device.get()));
            unsigned size = gst_caps_get_size(caps.get());
            for (unsigned i = 0; i < size; i++) {
                auto* structure = gst_caps_get_structure(caps.get(), i);
                if (!g_str_equal(gst_structure_get_name(structure), "audio/x-raw"))
                    continue;
                int value;
                if (!gst_structure_get_int(structure, "channels", &value))
                    continue;
                count = std::max(count, value);
            }
            devices = g_list_delete_link(devices, devices);
        }
        GST_DEBUG("maximumNumberOfOutputChannels: %d", count);
        gst_device_monitor_stop(monitor.get());
    });

    return count;
}

Ref<AudioDestination> AudioDestination::create(AudioIOCallback& callback, const String&, unsigned numberOfInputChannels, unsigned numberOfOutputChannels, float sampleRate)
{
    initializeDebugCategory();
    // FIXME: make use of inputDeviceId as appropriate.

    // FIXME: Add support for local/live audio input.
    if (numberOfInputChannels)
        WTFLogAlways("AudioDestination::create(%u, %u, %f) - unhandled input channels", numberOfInputChannels, numberOfOutputChannels, sampleRate);

    return adoptRef(*new AudioDestinationGStreamer(callback, numberOfOutputChannels, sampleRate));
}

float AudioDestination::hardwareSampleRate()
{
    return 44100;
}

unsigned long AudioDestination::maxChannelCount()
{
    return maximumNumberOfOutputChannels();
}

AudioDestinationGStreamer::AudioDestinationGStreamer(AudioIOCallback& callback, unsigned long numberOfOutputChannels, float sampleRate)
    : AudioDestination(callback, sampleRate)
    , m_renderBus(AudioBus::create(numberOfOutputChannels, AudioUtilities::renderQuantumSize, false))
{
    static Atomic<uint32_t> pipelineId;
    m_pipeline = gst_pipeline_new(makeString("audio-destination-", pipelineId.exchangeAdd(1)).ascii().data());
    connectSimpleBusMessageCallback(m_pipeline.get(), [this](GstMessage* message) {
        this->handleMessage(message);
    });

    m_src = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_WEB_AUDIO_SRC, "rate", sampleRate,
        "bus", m_renderBus.get(), "destination", this, "frames", AudioUtilities::renderQuantumSize, nullptr));

    GRefPtr<GstElement> audioSink = createPlatformAudioSink("music"_s);
    m_audioSinkAvailable = audioSink;
    if (!audioSink) {
        GST_ERROR("Failed to create GStreamer audio sink element");
        return;
    }

    // Probe platform early on for a working audio output device. This is not needed for the WebKit
    // custom audio sink because it doesn't rely on autoaudiosink.
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
            m_audioSinkAvailable = false;
            return;
        }
    }

    GstElement* audioConvert = makeGStreamerElement("audioconvert", nullptr);
    GstElement* audioResample = makeGStreamerElement("audioresample", nullptr);
    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), m_src.get(), audioConvert, audioResample, audioSink.get(), nullptr);

    // Link src pads from webkitAudioSrc to audioConvert ! audioResample ! autoaudiosink.
    gst_element_link_pads_full(m_src.get(), "src", audioConvert, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioConvert, "src", audioResample, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample, "src", audioSink.get(), "sink", GST_PAD_LINK_CHECK_NOTHING);
}

AudioDestinationGStreamer::~AudioDestinationGStreamer()
{
    GST_DEBUG_OBJECT(m_pipeline.get(), "Disposing");
    disconnectSimpleBusMessageCallback(m_pipeline.get());
    gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
    notifyStopResult(true);
}

unsigned AudioDestinationGStreamer::framesPerBuffer() const
{
    return AudioUtilities::renderQuantumSize;
}

bool AudioDestinationGStreamer::handleMessage(GstMessage* message)
{
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        notifyIsPlaying(false);
        break;
    case GST_MESSAGE_LATENCY:
        gst_bin_recalculate_latency(GST_BIN_CAST(m_pipeline.get()));
        break;
    default:
        break;
    }
    return true;
}

void AudioDestinationGStreamer::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    webkitWebAudioSourceSetDispatchToRenderThreadFunction(WEBKIT_WEB_AUDIO_SRC(m_src.get()), WTFMove(dispatchToRenderThread));
    startRendering(WTFMove(completionHandler));
}

void AudioDestinationGStreamer::startRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(m_audioSinkAvailable);
    m_startupCompletionHandler = WTFMove(completionHandler);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Starting audio rendering, sink %s", m_audioSinkAvailable ? "available" : "not available");

    if (m_isPlaying) {
        notifyStartupResult(true);
        return;
    }

    if (!m_audioSinkAvailable) {
        notifyStartupResult(false);
        return;
    }

    notifyStartupResult(webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_PLAYING, [this](GstMessage* message) -> bool {
        return handleMessage(message);
    }));
}

void AudioDestinationGStreamer::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    stopRendering(WTFMove(completionHandler));
    webkitWebAudioSourceSetDispatchToRenderThreadFunction(WEBKIT_WEB_AUDIO_SRC(m_src.get()), nullptr);
}

void AudioDestinationGStreamer::stopRendering(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(m_audioSinkAvailable);
    m_stopCompletionHandler = WTFMove(completionHandler);
    GST_DEBUG_OBJECT(m_pipeline.get(), "Stopping audio rendering, sink %s", m_audioSinkAvailable ? "available" : "not available");

    if (!m_isPlaying) {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Already stopped");
        notifyStopResult(true);
        return;
    }

    if (!m_audioSinkAvailable) {
        notifyStopResult(false);
        return;
    }

    notifyStopResult(webkitGstSetElementStateSynchronously(m_pipeline.get(), GST_STATE_READY, [this](GstMessage* message) -> bool {
        return handleMessage(message);
    }));
}

void AudioDestinationGStreamer::notifyStartupResult(bool success)
{
    callOnMainThreadAndWait([this, completionHandler = WTFMove(m_startupCompletionHandler), success]() mutable {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Has start completion handler: %s", boolForPrinting(!!completionHandler));
        if (completionHandler)
            completionHandler(success);
    });
}

void AudioDestinationGStreamer::notifyStopResult(bool success)
{
    if (success)
        notifyIsPlaying(false);

    callOnMainThreadAndWait([this, completionHandler = WTFMove(m_stopCompletionHandler), success]() mutable {
        GST_DEBUG_OBJECT(m_pipeline.get(), "Has stop completion handler: %s", boolForPrinting(!!completionHandler));
        if (completionHandler)
            completionHandler(success);
    });
}

void AudioDestinationGStreamer::notifyIsPlaying(bool isPlaying)
{
    if (m_isPlaying == isPlaying)
        return;

    GST_DEBUG("Is playing: %s", boolForPrinting(isPlaying));
    m_isPlaying = isPlaying;
    if (m_callback)
        m_callback->isPlayingDidChange();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
