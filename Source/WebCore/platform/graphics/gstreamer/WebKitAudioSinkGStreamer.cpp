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
#include <wtf/glib/WTFGType.h>

#if ENABLE(WEB_AUDIO)
#include "AudioDestination.h"
#include "WebKitWebAudioSourceGStreamer.h"
#endif

using namespace WebCore;

struct _WebKitAudioSinkPrivate {
    GRefPtr<GstElement> interAudioSink;
    GRefPtr<GstPad> mixerPad;
    String role;
    String deviceId;
    GRefPtr<GstDevice> device;
};

enum {
    WEBKIT_AUDIO_SINK_PROP_0,
    WEBKIT_AUDIO_SINK_PROP_VOLUME,
    WEBKIT_AUDIO_SINK_PROP_MUTE,
};

static GstStaticPadTemplate audioSinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS("audio/x-raw"));

GST_DEBUG_CATEGORY_STATIC(webkit_audio_sink_debug);
#define GST_CAT_DEFAULT webkit_audio_sink_debug

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitAudioSink, webkit_audio_sink, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE(GST_TYPE_STREAM_VOLUME, nullptr);
    GST_DEBUG_CATEGORY_INIT(webkit_audio_sink_debug, "webkitaudiosink", 0, "webkit audio sink element")
)

// The interaudiosrc of the mixer reads the audio at its own pace, without any margin: when the
// audio is written slightly after it's read, the missing samples are replaced by silence, which
// produces audible clicks. The audio is rendered a bit earlier so that it's available when it's
// read. The latency is unchanged, the audio just waits longer in the inter surface. WebAudio only
// renders about 20ms ahead, a bigger margin wouldn't help.
static constexpr GstClockTime interAudioSinkMargin = 20 * GST_MSECOND;

static bool webKitAudioSinkConfigure(WebKitAudioSink* sink)
{
    auto enableAudioMixer = UTF8CStringView::unsafeFromUTF8(g_getenv("WEBKIT_GST_ENABLE_AUDIO_MIXER"));
    if (!enableAudioMixer || enableAudioMixer != "1"_s)
        return false;

    if (!GStreamerAudioMixer::isAvailable()) {
        GST_WARNING("Internal audio mixing request cannot be fulfilled.");
        return false;
    }

    sink->priv->interAudioSink = makeGStreamerElement("interaudiosink"_s);
    RELEASE_ASSERT(sink->priv->interAudioSink);
    g_object_set(sink->priv->interAudioSink.get(), "ts-offset", -static_cast<GstClockTimeDiff>(interAudioSinkMargin), nullptr);

    // The inter audio elements don't support non-interleaved audio, such as the WebAudio output:
    // the planes would be mixed up, so make sure the audio is interleaved.
    auto* audioConvert = makeGStreamerElement("audioconvert"_s);
    auto* capsFilter = gst_element_factory_make("capsfilter", nullptr);
    GRefPtr interleavedCaps = adoptGRef(gst_caps_new_simple("audio/x-raw", "layout", G_TYPE_STRING, "interleaved", nullptr));
    g_object_set(capsFilter, "caps", interleavedCaps.get(), nullptr);

    gst_bin_add_many(GST_BIN_CAST(sink), audioConvert, capsFilter, sink->priv->interAudioSink.get(), nullptr);
    gst_element_link_many(audioConvert, capsFilter, sink->priv->interAudioSink.get(), nullptr);
    GRefPtr convertPad = adoptGRef(gst_element_get_static_pad(audioConvert, "sink"));
    gst_element_add_pad(GST_ELEMENT_CAST(sink), webkitGstGhostPadFromStaticTemplate(&audioSinkTemplate, "sink"_s, convertPad.get()));
    return true;
}

static GRefPtr<GstPad> webKitAudioSinkRegisterProducer(WebKitAudioSink* sink)
{
    auto* priv = sink->priv;
    std::optional<int> forcedSampleRate;
    std::optional<GstClockTime> periodTime;
#if ENABLE(WEB_AUDIO)
    if (priv->role == "webaudio"_s) {
        // Match the interaudiosrc period-time with the size of the buffers pushed by the WebAudio
        // source, otherwise the inter elements accumulate the audio in chunks that don't match the
        // reads, producing underruns and clicks. The period is applied when the interaudiosrc
        // starts, so it must be known when the producer is registered.
        forcedSampleRate = AudioDestination::hardwareSampleRate();
        periodTime = gst_util_uint64_scale_ceil(webkitWebAudioSourceFramesPerBuffer(), GST_SECOND, *forcedSampleRate);
    }
#endif
    return GStreamerAudioMixer::singleton().registerProducer(priv->interAudioSink.get(), forcedSampleRate, periodTime, priv->deviceId, priv->device);
}

static void webKitAudioSinkSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* pspec)
{
    WebKitAudioSink* sink = WEBKIT_AUDIO_SINK(object);

    switch (propID) {
    case WEBKIT_AUDIO_SINK_PROP_VOLUME: {
        g_object_set_property(G_OBJECT(sink->priv->mixerPad.get()), "volume", value);
        break;
    }
    case WEBKIT_AUDIO_SINK_PROP_MUTE: {
        g_object_set_property(G_OBJECT(sink->priv->mixerPad.get()), "mute", value);
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
    case WEBKIT_AUDIO_SINK_PROP_VOLUME: {
        g_object_get_property(G_OBJECT(sink->priv->mixerPad.get()), "volume", value);
        break;
    }
    case WEBKIT_AUDIO_SINK_PROP_MUTE: {
        g_object_get_property(G_OBJECT(sink->priv->mixerPad.get()), "mute", value);
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

    GST_DEBUG_OBJECT(sink, "Handling %s transition", gst_state_change_get_name(stateChange));

    auto& mixer = GStreamerAudioMixer::singleton();
    if (priv->interAudioSink && stateChange == GST_STATE_CHANGE_NULL_TO_READY)
        priv->mixerPad = webKitAudioSinkRegisterProducer(sink);

    if (priv->mixerPad)
        mixer.ensureState(stateChange, priv->deviceId);

    GstStateChangeReturn result = GST_ELEMENT_CLASS(webkit_audio_sink_parent_class)->change_state(element, stateChange);

    if (priv->mixerPad && stateChange == GST_STATE_CHANGE_READY_TO_NULL && result > GST_STATE_CHANGE_FAILURE) {
        mixer.unregisterProducer(priv->mixerPad);
        priv->mixerPad = nullptr;
    }

    return result;
}

static void webKitAudioSinkConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_audio_sink_parent_class)->constructed(object);
    IGNORE_WARNINGS_BEGIN("cast-align");
    GST_OBJECT_FLAG_SET(GST_OBJECT_CAST(object), GST_ELEMENT_FLAG_SINK);
    gst_bin_set_suppressed_flags(GST_BIN_CAST(object), static_cast<GstElementFlags>(GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK));
    IGNORE_WARNINGS_END;
}

static void webkit_audio_sink_class_init(WebKitAudioSinkClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    oklass->set_property = webKitAudioSinkSetProperty;
    oklass->get_property = webKitAudioSinkGetProperty;
    oklass->constructed = webKitAudioSinkConstructed;

    g_object_class_install_property(oklass, WEBKIT_AUDIO_SINK_PROP_VOLUME,
        g_param_spec_double("volume", nullptr, nullptr, 0, 10, 1,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(oklass, WEBKIT_AUDIO_SINK_PROP_MUTE,
        g_param_spec_boolean("mute", nullptr, nullptr, FALSE,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_static_pad_template(eklass, &audioSinkTemplate);
    gst_element_class_set_metadata(eklass, "WebKit Audio sink element", "Sink/Audio",
        "Proxies audio data to WebKit's audio mixer",
        "Philippe Normand <philn@igalia.com>");

    eklass->change_state = GST_DEBUG_FUNCPTR(webKitAudioSinkChangeState);
}

GstElement* /* (transfer floating) */ webkitAudioSinkNew(const String& role, const String& deviceId, const GRefPtr<GstDevice>& device)
{
    auto element = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_AUDIO_SINK, nullptr));
    auto audioSink = WEBKIT_AUDIO_SINK(element);

    audioSink->priv->role = role;
    audioSink->priv->deviceId = deviceId;
    if (device)
        audioSink->priv->device = device;
    if (!webKitAudioSinkConfigure(audioSink)) {
        gst_object_unref(element);
        return nullptr;
    }
    ASSERT(g_object_is_floating(element));
    return element;
}

bool webkitAudioSinkSetDevice(GstElement* element, const String& deviceId, const GRefPtr<GstDevice>& device)
{
    if (!WEBKIT_IS_AUDIO_SINK(element))
        return false;

    auto* sink = WEBKIT_AUDIO_SINK(element);
    auto* priv = sink->priv;

    // No-op if already on the requested device.
    if (priv->deviceId == deviceId)
        return true;

    auto& mixer = GStreamerAudioMixer::singleton();

    if (priv->mixerPad) {
        mixer.unregisterProducer(priv->mixerPad);
        priv->mixerPad = nullptr;
    }

    priv->deviceId = deviceId;
    priv->device = device;

    if (priv->interAudioSink) {
        priv->mixerPad = webKitAudioSinkRegisterProducer(sink);

        // Bring the new pipeline to the current element state.
        GstState currentState;
        gst_element_get_state(element, &currentState, nullptr, 0);
        if (currentState >= GST_STATE_PAUSED) {
            mixer.ensureState(GST_STATE_CHANGE_READY_TO_PAUSED, priv->deviceId);
            if (currentState >= GST_STATE_PLAYING)
                mixer.ensureState(GST_STATE_CHANGE_PAUSED_TO_PLAYING, priv->deviceId);
        }
    }
    return true;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER)
