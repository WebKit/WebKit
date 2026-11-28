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
#include <gst/base/gstbasesink.h>
#include <wtf/glib/WTFGType.h>

#if ENABLE(VIRTUAL_AUDIO_DEVICES)
#include "GStreamerVirtualAudioDeviceManager.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioDestination.h"
#include "WebKitWebAudioSourceGStreamer.h"
#endif

using namespace WebCore;

struct _WebKitAudioSinkPrivate {
    GRefPtr<GstElement> interAudioSink;
    // Both are protected by the object lock, the latency is read from the streaming thread of the
    // producer.
    GRefPtr<GstPad> mixerPad;
    GStreamerAudioMixer::SharedLatency mixerLatency;
    String role;
    String deviceId;
    GRefPtr<GstDevice> device;

    // Producer state applied again to the mixer pad when switching device.
    double volume { 1 };
    bool isMuted { false };
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

// Default period-time of the interaudiosrc, used by the producers of media elements.
static constexpr GstClockTime interAudioSrcPeriodTime = 25 * GST_MSECOND;

static void webKitAudioSinkUpdateRenderDelay(WebKitAudioSink*);

static bool webKitAudioSinkShouldUseAudioMixer(WebKitAudioSink* sink, AudioMixerUsage mixerUsage)
{
    if (mixerUsage == AudioMixerUsage::Required)
        return true;

#if ENABLE(VIRTUAL_AUDIO_DEVICES)
    // Virtual audio devices are only rendered by the mixer.
    if (GStreamerVirtualAudioDeviceManager::isVirtualDevice(sink->priv->device.get()))
        return true;
#else
    UNUSED_PARAM(sink);
#endif

    auto enableAudioMixer = UTF8CStringView::unsafeFromUTF8(g_getenv("WEBKIT_GST_ENABLE_AUDIO_MIXER"));
    return enableAudioMixer && enableAudioMixer == "1"_s;
}

static bool webKitAudioSinkConfigure(WebKitAudioSink* sink, AudioMixerUsage mixerUsage)
{
    if (!webKitAudioSinkShouldUseAudioMixer(sink, mixerUsage))
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

    // The mixer latency is only known once the mixer pipeline plays, after the producer started.
    if (sink->priv->role != "webaudio"_s) {
        GRefPtr interAudioSinkPad = adoptGRef(gst_element_get_static_pad(sink->priv->interAudioSink.get(), "sink"));
        gst_pad_add_probe(interAudioSinkPad.get(), GST_PAD_PROBE_TYPE_BUFFER, [](GstPad*, GstPadProbeInfo*, gpointer userData) -> GstPadProbeReturn {
            webKitAudioSinkUpdateRenderDelay(WEBKIT_AUDIO_SINK(userData));
            return GST_PAD_PROBE_OK;
        }, sink, nullptr);
    }
    return true;
}

static GStreamerAudioMixer::Producer webKitAudioSinkRegisterProducer(WebKitAudioSink* sink)
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

    // The mixer pad can be replaced from the streaming thread, see webKitAudioSinkSetProducer().
    GRefPtr<GstPad> mixerPad;
    {
        GstObjectLocker locker(sink);
        switch (propID) {
        case WEBKIT_AUDIO_SINK_PROP_VOLUME:
            sink->priv->volume = g_value_get_double(value);
            break;
        case WEBKIT_AUDIO_SINK_PROP_MUTE:
            sink->priv->isMuted = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
            return;
        }
        mixerPad = sink->priv->mixerPad;
    }
    // The volume and mute properties of the mixer pad have the same names.
    if (mixerPad)
        g_object_set_property(G_OBJECT(mixerPad.get()), pspec->name, value);
}

static void webKitAudioSinkGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* pspec)
{
    WebKitAudioSink* sink = WEBKIT_AUDIO_SINK(object);

    switch (propID) {
    case WEBKIT_AUDIO_SINK_PROP_VOLUME: {
        GstObjectLocker locker(sink);
        g_value_set_double(value, sink->priv->volume);
        break;
    }
    case WEBKIT_AUDIO_SINK_PROP_MUTE: {
        GstObjectLocker locker(sink);
        g_value_set_boolean(value, sink->priv->isMuted);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
        break;
    }
}

static void webKitAudioSinkSetProducer(WebKitAudioSink* sink, GStreamerAudioMixer::Producer&& producer)
{
    GstObjectLocker locker(sink);
    // A new mixer pad starts with the default volume and unmuted. It's configured with the lock
    // held, so that a volume change made meanwhile from another thread is not lost.
    if (producer.mixerPad)
        g_object_set(producer.mixerPad.get(), "volume", sink->priv->volume, "mute", static_cast<gboolean>(sink->priv->isMuted), nullptr);
    sink->priv->mixerPad = WTF::move(producer.mixerPad);
    sink->priv->mixerLatency = WTF::move(producer.latency);
}

static GstClockTime webKitAudioSinkMixerLatency(WebKitAudioSink* sink)
{
    GstObjectLocker locker(sink);
    return sink->priv->mixerLatency ? sink->priv->mixerLatency->load() : 0;
}

// The audio of a media element is played after the mixer pipeline, which its own pipeline doesn't
// know: the video would be ahead of the audio. The render delay of the interaudiosink is reported
// as latency to the producer pipeline, which delays the video accordingly, and the interaudiosink
// renders the audio that much earlier. By default, it only accounts for the latency of the
// interaudiosrc. Once rendered, the audio waits in the inter surface, the interaudiosrc reads it one
// period before the timestamp of the buffer it creates, and the mixer pipeline plays that buffer
// after its latency. The surface holds about the margin of the interaudiosink and one period of
// audio, as the audio is written by producer buffers and read once per period: measured, the audio
// is played about the mixer latency, two periods and the margin after it's rendered.
//
// The mixer latency is only known once the mixer pipeline plays, after the producer started: the
// render delay is updated when the producer pipeline is already playing, its latency is recomputed
// then, which delays the video once.
//
// WebAudio producers keep the default render delay: the AudioContext reports the mixer latency
// in its outputLatency instead, see webkitAudioSinkGetMixerLatency().
static void webKitAudioSinkUpdateRenderDelay(WebKitAudioSink* sink)
{
    auto mixerLatency = webKitAudioSinkMixerLatency(sink);
    if (!mixerLatency)
        return;

    auto renderDelay = mixerLatency + 2 * interAudioSrcPeriodTime + interAudioSinkMargin;
    auto* interAudioSink = GST_BASE_SINK_CAST(sink->priv->interAudioSink.get());
    if (gst_base_sink_get_render_delay(interAudioSink) == renderDelay)
        return;

    GST_DEBUG_OBJECT(sink, "Mixer latency %" GST_TIME_FORMAT ", setting the render delay to %" GST_TIME_FORMAT, GST_TIME_ARGS(mixerLatency), GST_TIME_ARGS(renderDelay));
    gst_base_sink_set_render_delay(interAudioSink, renderDelay);
}

static GstStateChangeReturn webKitAudioSinkChangeState(GstElement* element, GstStateChange stateChange)
{
    auto* sink = WEBKIT_AUDIO_SINK(element);
    auto* priv = sink->priv;

    GST_DEBUG_OBJECT(sink, "Handling %s transition", gst_state_change_get_name(stateChange));

    auto& mixer = GStreamerAudioMixer::singleton();
    if (priv->interAudioSink && !priv->mixerPad && stateChange == GST_STATE_CHANGE_NULL_TO_READY)
        webKitAudioSinkSetProducer(sink, webKitAudioSinkRegisterProducer(sink));

    if (priv->mixerPad)
        mixer.ensureState(stateChange, priv->mixerPad);

    GstStateChangeReturn result = GST_ELEMENT_CLASS(webkit_audio_sink_parent_class)->change_state(element, stateChange);

    // The mixer pipeline is shared by the producers of the device: a producer that failed to change
    // state must not keep it in the target state.
    if (priv->mixerPad && result == GST_STATE_CHANGE_FAILURE)
        mixer.ensureState(GST_STATE_TRANSITION(GST_STATE_TRANSITION_NEXT(stateChange), GST_STATE_TRANSITION_CURRENT(stateChange)), priv->mixerPad);

    // The interaudiosink resets its render delay when it starts. The mixer latency is already known
    // when the mixer pipeline is already playing.
    if (priv->mixerPad && stateChange == GST_STATE_CHANGE_NULL_TO_READY && priv->role != "webaudio"_s)
        webKitAudioSinkUpdateRenderDelay(sink);

    // The producer is unregistered once it stopped, or when it failed to start.
    bool isStopped = stateChange == GST_STATE_CHANGE_READY_TO_NULL ? result > GST_STATE_CHANGE_FAILURE : stateChange == GST_STATE_CHANGE_NULL_TO_READY && result == GST_STATE_CHANGE_FAILURE;
    if (priv->mixerPad && isStopped) {
        mixer.unregisterProducer(priv->mixerPad);
        webKitAudioSinkSetProducer(sink, { });
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

GstElement* /* (transfer floating) */ webkitAudioSinkNew(const String& role, const String& deviceId, const GRefPtr<GstDevice>& device, AudioMixerUsage mixerUsage)
{
    auto element = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_AUDIO_SINK, nullptr));
    auto audioSink = WEBKIT_AUDIO_SINK(element);

    audioSink->priv->role = role;
    // Used from the streaming threads, always with the state lock of the sink held.
    audioSink->priv->deviceId = deviceId.isolatedCopy();
    if (device)
        audioSink->priv->device = device;
    if (!webKitAudioSinkConfigure(audioSink, mixerUsage)) {
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

    // Serialized with the state changes, which register the producer when reaching
    // GST_STATE_READY and unregister it when going back to GST_STATE_NULL, possibly from
    // another thread. The mixer is always called with the state lock of the sink held, as in
    // webKitAudioSinkChangeState().
    auto stateLocker = GstStateLocker(element);

    // No-op if already on the requested device.
    if (priv->deviceId == deviceId)
        return true;

    auto& mixer = GStreamerAudioMixer::singleton();

    if (priv->mixerPad) {
        mixer.unregisterProducer(priv->mixerPad);
        webKitAudioSinkSetProducer(sink, { });
    }

    priv->deviceId = deviceId.isolatedCopy();
    priv->device = device;

    // Below READY, the producer is registered on the new device when it reaches READY.
    GstState currentState;
    gst_element_get_state(element, &currentState, nullptr, 0);
    if (priv->interAudioSink && currentState >= GST_STATE_READY) {
        webKitAudioSinkSetProducer(sink, webKitAudioSinkRegisterProducer(sink));

        // Bring the new pipeline to the current element state.
        if (priv->mixerPad && currentState >= GST_STATE_PAUSED) {
            mixer.ensureState(GST_STATE_CHANGE_READY_TO_PAUSED, priv->mixerPad);
            if (currentState >= GST_STATE_PLAYING)
                mixer.ensureState(GST_STATE_CHANGE_PAUSED_TO_PLAYING, priv->mixerPad);
        }
    }
    return true;
}

GstClockTime webkitAudioSinkGetMixerLatency(GstElement* element)
{
    if (!WEBKIT_IS_AUDIO_SINK(element))
        return 0;

    // Called from the streaming thread of the producer, for every buffer, while the producer can
    // be changed from another thread.
    auto* sink = WEBKIT_AUDIO_SINK(element);
    auto mixerLatency = webKitAudioSinkMixerLatency(sink);
    if (!mixerLatency)
        return 0;

    // The interaudiosink already reports the latency of the interaudiosrc as its render delay,
    // and renders the audio that much earlier. Only the rest of the mixer pipeline latency adds up.
    auto renderDelay = gst_base_sink_get_render_delay(GST_BASE_SINK_CAST(sink->priv->interAudioSink.get()));
    return mixerLatency > renderDelay ? mixerLatency - renderDelay : 0;
}

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER)
