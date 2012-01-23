/*
 *  Copyright (C) 2011 Igalia S.L
 *  Copyright (C) 2011 Zan Dobersek  <zandobersek@gmail.com>
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

#include "AudioFileReader.h"

#include "AudioBus.h"
#include "GOwnPtr.h"
#include "GRefPtr.h"
#include <gio/gio.h>
#include <gst/app/gstappsink.h>
#include <gst/audio/multichannel.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class AudioFileReader {
    WTF_MAKE_NONCOPYABLE(AudioFileReader);
public:
    AudioFileReader(const char* filePath);
    AudioFileReader(const void* data, size_t dataSize);
    ~AudioFileReader();

    PassOwnPtr<AudioBus> createBus(float sampleRate, bool mixToMono);

    GstFlowReturn handleBuffer(GstAppSink*);
    gboolean handleMessage(GstMessage*);
    void handleNewDeinterleavePad(GstPad*);
    void deinterleavePadsConfigured();
    void plugDeinterleave(GstPad*);

private:
    const void* m_data;
    size_t m_dataSize;
    const char* m_filePath;

    float m_sampleRate;
    GstBufferList* m_frontLeftBuffers;
    GstBufferListIterator* m_frontLeftBuffersIterator;
    GstBufferList* m_frontRightBuffers;
    GstBufferListIterator* m_frontRightBuffersIterator;
    GstElement* m_pipeline;
    bool m_eos;
    unsigned m_channelSize;

    gulong m_deinterleavePadAddedSignalHandlerId;
    gulong m_deinterleaveNoMorePadSignalHandlerId;
};

static GstCaps* getGStreamerAudioCaps(int channels, float sampleRate)
{
    return gst_caps_new_simple("audio/x-raw-float", "rate", G_TYPE_INT, static_cast<int>(sampleRate), "channels", G_TYPE_INT, channels, "endianness", G_TYPE_INT, G_BYTE_ORDER, "width", G_TYPE_INT, 32, NULL);
}

static void copyGstreamerBuffersToAudioChannel(GstBufferList* buffers, AudioChannel* audioChannel)
{
    GstBufferListIterator* iter = gst_buffer_list_iterate(buffers);
    gst_buffer_list_iterator_next_group(iter);
    GstBuffer* buffer = gst_buffer_list_iterator_merge_group(iter);
    if (buffer) {
        memcpy(audioChannel->mutableData(), reinterpret_cast<float*>(GST_BUFFER_DATA(buffer)), GST_BUFFER_SIZE(buffer));
        gst_buffer_unref(buffer);
    }

    gst_buffer_list_iterator_free(iter);
}

static GstFlowReturn onAppsinkNewBufferCallback(GstAppSink* sink, gpointer userData)
{
    return static_cast<AudioFileReader*>(userData)->handleBuffer(sink);
}

gboolean messageCallback(GstBus* bus, GstMessage* message, AudioFileReader* reader)
{
    return reader->handleMessage(message);
}

static void onGStreamerDeinterleavePadAddedCallback(GstElement* element, GstPad* pad, AudioFileReader* reader)
{
    reader->handleNewDeinterleavePad(pad);
}

static void onGStreamerDeinterleaveReadyCallback(GstElement* element, AudioFileReader* reader)
{
    reader->deinterleavePadsConfigured();
}

static void onGStreamerDecodebinPadAddedCallback(GstElement* element, GstPad* pad, AudioFileReader* reader)
{
    reader->plugDeinterleave(pad);
}

AudioFileReader::AudioFileReader(const char* filePath)
    : m_data(0)
    , m_dataSize(0)
    , m_filePath(filePath)
    , m_eos(false)
{
}

AudioFileReader::AudioFileReader(const void* data, size_t dataSize)
    : m_data(data)
    , m_dataSize(dataSize)
    , m_filePath(0)
    , m_eos(false)
{
}

AudioFileReader::~AudioFileReader()
{
}

GstFlowReturn AudioFileReader::handleBuffer(GstAppSink* sink)
{
    GstBuffer* buffer = gst_app_sink_pull_buffer(sink);
    if (!buffer)
        return GST_FLOW_ERROR;

    GstCaps* caps = gst_buffer_get_caps(buffer);
    GstStructure* structure = gst_caps_get_structure(caps, 0);

    gint channels = 0;
    if (!gst_structure_get_int(structure, "channels", &channels) || !channels) {
        gst_caps_unref(caps);
        gst_buffer_unref(buffer);
        return GST_FLOW_ERROR;
    }

    // Check the first audio channel. The buffer is supposed to store
    // data of a single channel anyway.
    GstAudioChannelPosition* positions = gst_audio_get_channel_positions(structure);
    switch (positions[0]) {
    case GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT:
        gst_buffer_list_iterator_add(m_frontLeftBuffersIterator, buffer);
        m_channelSize += GST_BUFFER_OFFSET_END(buffer) - GST_BUFFER_OFFSET(buffer);
        break;
    case GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT:
        gst_buffer_list_iterator_add(m_frontRightBuffersIterator, buffer);
        break;
    default:
        gst_buffer_unref(buffer);
        break;
    }

    g_free(positions);
    gst_caps_unref(caps);
    return GST_FLOW_OK;
}

gboolean AudioFileReader::handleMessage(GstMessage* message)
{
    GOwnPtr<GError> error;
    GOwnPtr<gchar> debug;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        m_eos = true;
        break;
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message, &error.outPtr(), &debug.outPtr());
        g_warning("Warning: %d, %s", error->code,  error->message);
        break;
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &error.outPtr(), &debug.outPtr());
        ASSERT_WITH_MESSAGE(0, "Fatal error: %d, %s", error->code,  error->message);
        break;
    default:
        break;
    }
    return TRUE;
}

void AudioFileReader::handleNewDeinterleavePad(GstPad* pad)
{
    // A new pad for a planar channel was added in deinterleave. Plug
    // in an appsink so we can pull the data from each
    // channel. Pipeline looks like:
    // ... deinterleave ! queue ! appsink.
    GstElement* queue = gst_element_factory_make("queue", 0);
    GstElement* sink = gst_element_factory_make("appsink", 0);

    GstAppSinkCallbacks callbacks;
    callbacks.eos = 0;
    callbacks.new_preroll = 0;
    callbacks.new_buffer_list = 0;
    callbacks.new_buffer = onAppsinkNewBufferCallback;
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, this, 0);

    g_object_set(sink, "sync", FALSE, NULL);

    GstCaps* caps = getGStreamerAudioCaps(1, m_sampleRate);
    gst_app_sink_set_caps(GST_APP_SINK(sink), caps);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(m_pipeline), queue, sink, NULL);

    GstPad* sinkPad = gst_element_get_static_pad(queue, "sink");
    gst_pad_link(pad, sinkPad);
    gst_object_unref(GST_OBJECT(sinkPad));

    gst_element_link_pads_full(queue, "src", sink, "sink", GST_PAD_LINK_CHECK_NOTHING);

    gst_element_set_state(queue, GST_STATE_READY);
    gst_element_set_state(sink, GST_STATE_READY);
}

void AudioFileReader::deinterleavePadsConfigured()
{
    // All deinterleave src pads are now available, let's roll to
    // PLAYING so data flows towards the sinks and it can be retrieved.
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
}

void AudioFileReader::plugDeinterleave(GstPad* pad)
{
    // A decodebin pad was added, plug in a deinterleave element to
    // separate each planar channel. Sub pipeline looks like
    // ... decodebin2 ! audioconvert ! audioresample ! capsfilter ! deinterleave.
    GstElement* audioConvert  = gst_element_factory_make("audioconvert", 0);
    GstElement* audioResample = gst_element_factory_make("audioresample", 0);
    GstElement* capsFilter = gst_element_factory_make("capsfilter", 0);
    GstElement* deInterleave = gst_element_factory_make("deinterleave", "deinterleave");

    g_object_set(deInterleave, "keep-positions", TRUE, NULL);
    m_deinterleavePadAddedSignalHandlerId = g_signal_connect(deInterleave, "pad-added", G_CALLBACK(onGStreamerDeinterleavePadAddedCallback), this);
    m_deinterleaveNoMorePadSignalHandlerId = g_signal_connect(deInterleave, "no-more-pads", G_CALLBACK(onGStreamerDeinterleaveReadyCallback), this);

    GstCaps* caps = getGStreamerAudioCaps(2, m_sampleRate);
    g_object_set(capsFilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(m_pipeline), audioConvert, audioResample, capsFilter, deInterleave, NULL);

    GstPad* sinkPad = gst_element_get_static_pad(audioConvert, "sink");
    gst_pad_link(pad, sinkPad);
    gst_object_unref(GST_OBJECT(sinkPad));

    gst_element_link_pads_full(audioConvert, "src", audioResample, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(audioResample, "src", capsFilter, "sink", GST_PAD_LINK_CHECK_NOTHING);
    gst_element_link_pads_full(capsFilter, "src", deInterleave, "sink", GST_PAD_LINK_CHECK_NOTHING);

    gst_element_sync_state_with_parent(audioConvert);
    gst_element_sync_state_with_parent(audioResample);
    gst_element_sync_state_with_parent(capsFilter);
    gst_element_sync_state_with_parent(deInterleave);
}


PassOwnPtr<AudioBus> AudioFileReader::createBus(float sampleRate, bool mixToMono)
{
    m_sampleRate = sampleRate;

    static bool gstInitialized = false;
    if (!gstInitialized)
        gstInitialized = gst_init_check(0, 0, 0);

    if (!gstInitialized)
        return nullptr;

    m_frontLeftBuffers = gst_buffer_list_new();
    m_frontLeftBuffersIterator = gst_buffer_list_iterate(m_frontLeftBuffers);
    gst_buffer_list_iterator_add_group(m_frontLeftBuffersIterator);

    m_frontRightBuffers = gst_buffer_list_new();
    m_frontRightBuffersIterator = gst_buffer_list_iterate(m_frontRightBuffers);
    gst_buffer_list_iterator_add_group(m_frontRightBuffersIterator);

    // Build the pipeline (giostreamsrc | filesrc) ! decodebin2
    // A deinterleave element is added once a src pad becomes available in decodebin.
    m_pipeline = gst_pipeline_new(0);

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(m_pipeline));
    gst_bus_add_signal_watch(bus);
    gulong busSignalHandlerId = g_signal_connect(bus, "message", G_CALLBACK(messageCallback), this);

    GstElement* source;
    if (m_data) {
        ASSERT(m_dataSize);
        source = gst_element_factory_make("giostreamsrc", 0);
        GRefPtr<GInputStream> memoryStream = g_memory_input_stream_new_from_data(m_data, m_dataSize, 0);
        g_object_set(source, "stream", memoryStream.get(), NULL);
    } else {
        source = gst_element_factory_make("filesrc", 0);
        g_object_set(source, "location", m_filePath, NULL);
    }

    GstElement* decodebin = gst_element_factory_make("decodebin2", "decodebin");
    gulong decodebinPadAddedSignalHandlerId = g_signal_connect(decodebin, "pad-added", G_CALLBACK(onGStreamerDecodebinPadAddedCallback), this);

    gst_bin_add_many(GST_BIN(m_pipeline), source, decodebin, NULL);
    gst_element_link_pads_full(source, "src", decodebin, "sink", GST_PAD_LINK_CHECK_NOTHING);

    m_eos = false;
    m_channelSize = 0;
    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    while (!m_eos)
        g_main_context_iteration(0, FALSE);

    g_signal_handler_disconnect(bus, busSignalHandlerId);
    gst_object_unref(bus);

    g_signal_handler_disconnect(decodebin, decodebinPadAddedSignalHandlerId);

    if (GstElement* deinterleave = gst_bin_get_by_name(GST_BIN(m_pipeline), "deinterleave")) {
        if (g_signal_handler_is_connected(deinterleave, m_deinterleavePadAddedSignalHandlerId))
            g_signal_handler_disconnect(deinterleave, m_deinterleavePadAddedSignalHandlerId);
        if (g_signal_handler_is_connected(deinterleave, m_deinterleaveNoMorePadSignalHandlerId))
            g_signal_handler_disconnect(deinterleave, m_deinterleaveNoMorePadSignalHandlerId);
        gst_object_unref(deinterleave);
    }

    unsigned channels = mixToMono ? 1 : 2;
    OwnPtr<AudioBus> audioBus = adoptPtr(new AudioBus(channels, m_channelSize, true));
    audioBus->setSampleRate(m_sampleRate);

    copyGstreamerBuffersToAudioChannel(m_frontLeftBuffers, audioBus->channel(0));
    if (!mixToMono)
        copyGstreamerBuffersToAudioChannel(m_frontRightBuffers, audioBus->channel(1));

    gst_buffer_list_iterator_free(m_frontLeftBuffersIterator);
    gst_buffer_list_iterator_free(m_frontRightBuffersIterator);
    gst_buffer_list_unref(m_frontLeftBuffers);
    gst_buffer_list_unref(m_frontRightBuffers);

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(m_pipeline));

    return audioBus.release();
}

PassOwnPtr<AudioBus> createBusFromAudioFile(const char* filePath, bool mixToMono, float sampleRate)
{
    return AudioFileReader(filePath).createBus(sampleRate, mixToMono);
}

PassOwnPtr<AudioBus> createBusFromInMemoryAudioFile(const void* data, size_t dataSize, bool mixToMono, float sampleRate)
{
    return AudioFileReader(data, dataSize).createBus(sampleRate, mixToMono);
}

} // WebCore

#endif // ENABLE(WEB_AUDIO)
