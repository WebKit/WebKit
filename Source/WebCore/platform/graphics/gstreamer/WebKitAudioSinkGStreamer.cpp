/*
 * Copyright (C) 2020 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitAudioSinkGStreamer.h"

#if USE(GSTREAMER)

#include "GStreamerAudioMixer.h"
#include "GStreamerCommon.h"
#include <gst/app/gstappsink.h>
#include <gst/pbutils/missing-plugins.h>
#include <sys/mman.h>
#include <wtf/glib/WTFGType.h>

#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
#include "PlatformDisplay.h"
#include "PlatformDisplayLibWPE.h"
#include <wpe/extensions/audio.h>
#endif

using namespace WebCore;

struct _WebKitAudioSinkPrivate {
    GRefPtr<GstElement> interAudioSink;
    GRefPtr<GstPad> mixerPad;
    GRefPtr<GstElement> volumeElement;
    GRefPtr<GstElement> appsink;

#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
    GUniquePtr<struct wpe_audio_source> wpeAudioSource;
    int m_streamId;

    // wpe_audio_source state:
    // - GST_STATE_NULL: not started or EOS
    // - GST_STATE_PLAYING: at least one packet was emitted
    // - GST_STATE_PAUSED: pause notification was sent
    GstState sourceState;
#endif
};

enum {
    PROP_0,
    PROP_VOLUME,
    PROP_MUTE,
};

static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("audio/x-raw"));

GST_DEBUG_CATEGORY_STATIC(webkit_audio_sink_debug);
#define GST_CAT_DEFAULT webkit_audio_sink_debug

#define webkit_audio_sink_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitAudioSink, webkit_audio_sink, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE(GST_TYPE_STREAM_VOLUME, nullptr);
    GST_DEBUG_CATEGORY_INIT(webkit_audio_sink_debug, "webkitaudiosink", 0, "webkit audio sink element")
)

#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
struct AudioPacketHolder {
    explicit AudioPacketHolder(GstBuffer* buffer)
    {
        this->buffer = GstMappedOwnedBuffer::create(buffer);
    }

    ~AudioPacketHolder()
    {
        if (fd)
            close(*fd);
    }

    Optional<std::pair<uint32_t, size_t>> map()
    {
        fd = memfd_create("wpe-audio-buffer", MFD_CLOEXEC);
        if (*fd == -1)
            return WTF::nullopt;

        if (ftruncate(*fd, buffer->size()) == -1)
            return WTF::nullopt;

        ssize_t bytesWritten = write(*fd, buffer->data(), buffer->size());
        if (bytesWritten < 0)
            return WTF::nullopt;

        if (static_cast<size_t>(bytesWritten) != buffer->size())
            return WTF::nullopt;

        if (lseek(*fd, 0, SEEK_SET) == -1)
            return WTF::nullopt;

        return std::make_pair(static_cast<uint32_t>(*fd), buffer->size());
    }

    Optional<int32_t> fd;
    RefPtr<GstMappedOwnedBuffer> buffer;
};

static void webKitAudioSinkHandleSample(WebKitAudioSink* sink, GRefPtr<GstSample>&& sample)
{
    auto* wpeAudioSource = sink->priv->wpeAudioSource.get();
    static Atomic<int32_t> uniqueId;

    if (sink->priv->sourceState == GST_STATE_NULL) {
        sink->priv->m_streamId = uniqueId.exchangeAdd(1);
        GstCaps* caps = gst_sample_get_caps(sample.get());
        GstAudioInfo info;
        gst_audio_info_init(&info);
        gst_audio_info_from_caps(&info, caps);
        const char* format = gst_audio_format_to_string(GST_AUDIO_INFO_FORMAT(&info));
        wpe_audio_source_start(wpeAudioSource, sink->priv->m_streamId, GST_AUDIO_INFO_CHANNELS(&info), format, GST_AUDIO_INFO_RATE(&info));
        sink->priv->sourceState = GST_STATE_PLAYING;
    }

    auto* holder = new AudioPacketHolder(gst_sample_get_buffer(sample.get()));
    auto mapData = holder->map();
    if (!mapData) {
        GST_ERROR_OBJECT(sink, "Unable to prepare shared audio buffer");
        delete holder;
        return;
    }
    wpe_audio_source_packet(wpeAudioSource, sink->priv->m_streamId, mapData->first, mapData->second, [](void* data) {
        auto* holder = reinterpret_cast<AudioPacketHolder*>(data);
        delete holder;
    }, holder);
}
#endif

static bool webKitAudioSinkConfigure(WebKitAudioSink* sink)
{
#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
    sink->priv->sourceState = GST_STATE_NULL;
    auto& sharedDisplay = PlatformDisplay::sharedDisplay();
    if (is<PlatformDisplayLibWPE>(sharedDisplay)) {
        sink->priv->wpeAudioSource.reset(wpe_audio_source_create(downcast<PlatformDisplayLibWPE>(sharedDisplay).backend()));
        if (wpe_audio_source_has_receiver(sink->priv->wpeAudioSource.get())) {
            sink->priv->volumeElement = gst_element_factory_make("volume", nullptr);
            sink->priv->appsink = gst_element_factory_make("appsink", nullptr);
            gst_app_sink_set_emit_signals(GST_APP_SINK(sink->priv->appsink.get()), TRUE);

            g_signal_connect(sink->priv->appsink.get(), "new-sample", G_CALLBACK(+[](GstElement* appsink, WebKitAudioSink* sink) -> GstFlowReturn {
                auto sample = adoptGRef(gst_app_sink_pull_sample(GST_APP_SINK(appsink)));
                webKitAudioSinkHandleSample(sink, WTFMove(sample));
                return GST_FLOW_OK;
            }), sink);
            g_signal_connect(sink->priv->appsink.get(), "new-preroll", G_CALLBACK(+[](GstElement* appsink, WebKitAudioSink* sink) -> GstFlowReturn {
                auto sample = adoptGRef(gst_app_sink_pull_preroll(GST_APP_SINK(appsink)));
                webKitAudioSinkHandleSample(sink, WTFMove(sample));
                return GST_FLOW_OK;
            }), sink);

            g_signal_connect(sink->priv->appsink.get(), "eos", G_CALLBACK(+[](GstElement*, WebKitAudioSink* sink) {
                wpe_audio_source_stop(sink->priv->wpeAudioSource.get(), sink->priv->m_streamId);
                sink->priv->sourceState = GST_STATE_NULL;
            }), sink);

            gst_bin_add_many(GST_BIN_CAST(sink), sink->priv->volumeElement.get(), sink->priv->appsink.get(), nullptr);
            gst_element_link(sink->priv->volumeElement.get(), sink->priv->appsink.get());

            auto targetPad = adoptGRef(gst_element_get_static_pad(sink->priv->volumeElement.get(), "sink"));
            GstPad* sinkPad = webkitGstGhostPadFromStaticTemplate(&sinkTemplate, "sink", targetPad.get());
            gst_element_add_pad(GST_ELEMENT_CAST(sink), sinkPad);
            GST_OBJECT_FLAG_SET(sinkPad, GST_PAD_FLAG_NEED_PARENT);
            return true;
        }
    }
#endif

    const char* value = g_getenv("WEBKIT_GST_ENABLE_AUDIO_MIXER");
    if (value && equal(value, "1")) {
        if (!GStreamerAudioMixer::isAvailable()) {
            GST_WARNING("Internal audio mixing request cannot be fulfilled.");
            return false;
        }

        sink->priv->interAudioSink = gst_element_factory_make("interaudiosink", nullptr);
        RELEASE_ASSERT(sink->priv->interAudioSink);

        gst_bin_add(GST_BIN_CAST(sink), sink->priv->interAudioSink.get());
        auto targetPad = adoptGRef(gst_element_get_static_pad(sink->priv->interAudioSink.get(), "sink"));
        gst_element_add_pad(GST_ELEMENT_CAST(sink), webkitGstGhostPadFromStaticTemplate(&sinkTemplate, "sink", targetPad.get()));
        return true;
    }
    return false;
}

static GstObject* getInternalVolumeObject(WebKitAudioSink* sink)
{
    if (sink->priv->volumeElement)
        return GST_OBJECT_CAST(sink->priv->volumeElement.get());

    RELEASE_ASSERT(sink->priv->mixerPad);
    return GST_OBJECT_CAST(sink->priv->mixerPad.get());
}

static void webKitAudioSinkSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* pspec)
{
    WebKitAudioSink* sink = WEBKIT_AUDIO_SINK(object);

    switch (propID) {
    case PROP_VOLUME: {
        GstObject* internalObject = getInternalVolumeObject(sink);
        g_object_set_property(G_OBJECT(internalObject), "volume", value);
        break;
    }
    case PROP_MUTE: {
        GstObject* internalObject = getInternalVolumeObject(sink);
        g_object_set_property(G_OBJECT(internalObject), "mute", value);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
}

static void webKitAudioSinkGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* pspec)
{
    WebKitAudioSink* sink = WEBKIT_AUDIO_SINK(object);

    switch (propID) {
    case PROP_VOLUME: {
        GstObject* internalObject = getInternalVolumeObject(sink);
        g_object_get_property(G_OBJECT(internalObject), "volume", value);
        break;
    }
    case PROP_MUTE: {
        GstObject* internalObject = getInternalVolumeObject(sink);
        g_object_get_property(G_OBJECT(internalObject), "mute", value);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
}

static GstStateChangeReturn webKitAudioSinkChangeState(GstElement* element, GstStateChange stateChange)
{
    auto* sink = WEBKIT_AUDIO_SINK(element);
    auto* priv = sink->priv;

#if GST_CHECK_VERSION(1, 14, 0)
    GST_DEBUG_OBJECT(sink, "Handling %s transition", gst_state_change_get_name(stateChange));
#endif

    auto& mixer = GStreamerAudioMixer::singleton();
    if (priv->interAudioSink && stateChange == GST_STATE_CHANGE_NULL_TO_READY)
        priv->mixerPad = mixer.registerProducer(priv->interAudioSink.get());

    if (priv->mixerPad)
        mixer.ensureState(stateChange);

    GstStateChangeReturn result = GST_CALL_PARENT_WITH_DEFAULT(GST_ELEMENT_CLASS, change_state, (element, stateChange), GST_STATE_CHANGE_FAILURE);

#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
    if (priv->appsink) {
        bool isEOS = gst_app_sink_is_eos(GST_APP_SINK(priv->appsink.get()));
        if ((stateChange == GST_STATE_CHANGE_PLAYING_TO_PAUSED) && !isEOS) {
            wpe_audio_source_pause(priv->wpeAudioSource.get(), priv->m_streamId);
            priv->sourceState = GST_STATE_PAUSED;
        }
        if (stateChange == GST_STATE_CHANGE_PAUSED_TO_READY && isEOS)
            priv->sourceState = GST_STATE_NULL;
    }
#endif

    if (priv->mixerPad && stateChange == GST_STATE_CHANGE_READY_TO_NULL && result > GST_STATE_CHANGE_FAILURE) {
        mixer.unregisterProducer(priv->mixerPad);
        priv->mixerPad = nullptr;
    }
#if PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
    if (priv->appsink && priv->sourceState == GST_STATE_PAUSED && stateChange == GST_STATE_CHANGE_PAUSED_TO_PLAYING && result > GST_STATE_CHANGE_FAILURE) {
        wpe_audio_source_resume(priv->wpeAudioSource.get(), priv->m_streamId);
        priv->sourceState = GST_STATE_PLAYING;
    }
#endif

    return result;
}

static void webkit_audio_sink_class_init(WebKitAudioSinkClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    oklass->set_property = webKitAudioSinkSetProperty;
    oklass->get_property = webKitAudioSinkGetProperty;

    g_object_class_install_property(oklass, PROP_VOLUME,
        g_param_spec_double("volume", "Volume", "The audio volume, 1.0=100%", 0, 10, 1,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(oklass, PROP_MUTE,
        g_param_spec_boolean("mute", "Mute", "Mute the audio channel without changing the volume", FALSE,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_static_pad_template(eklass, &sinkTemplate);
    gst_element_class_set_metadata(eklass, "WebKit Audio sink element", "Sink/Audio",
        "Proxies audio data to WebKit's audio mixer or to a WPE external audio handler",
        "Philippe Normand <philn@igalia.com>");

    eklass->change_state = GST_DEBUG_FUNCPTR(webKitAudioSinkChangeState);
}

GstElement* webkitAudioSinkNew()
{
    auto* sink = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_AUDIO_SINK, nullptr));
    if (!webKitAudioSinkConfigure(WEBKIT_AUDIO_SINK(sink))) {
        gst_object_unref(sink);
        return nullptr;
    }
    return sink;
}

#endif // USE(GSTREAMER)
