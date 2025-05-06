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

#include "AudioUtilities.h"
#include "GStreamerAudioMixer.h"
#include "GStreamerCommon.h"
#include <wtf/FileSystem.h>
#include <wtf/Scope.h>
#include <wtf/glib/WTFGType.h>

#if ENABLE(WEB_AUDIO)
#include "AudioDestination.h"
#endif

using namespace WebCore;

struct _WebKitAudioSinkPrivate {
    GRefPtr<GstElement> interAudioSink;
    GRefPtr<GstPad> mixerPad;
    String role;
    String deviceId;
    GRefPtr<GstDevice> device;
    GRefPtr<GstElement> volumeElement;
    GRefPtr<GstElement> unixfdsink;
    String socketPath;
    AudioSinkStartedCallback audioSinkStartedCallback;
    AudioSinkStoppedCallback audioSinkStoppedCallback;
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

static bool webKitAudioSinkConfigure(WebKitAudioSink* sink, String&& socketPath)
{
    if (!socketPath.isEmpty()) {
        FileSystem::makeAllDirectories(FileSystem::parentPath(socketPath));

        if (!gst_check_version(1, 28, 0)) {
            gst_printerrln("GStreamer 1.28 with unixfd plugin is required");
            return false;
        }

        sink->priv->unixfdsink = makeGStreamerElement("unixfdsink"_s);
        if (!sink->priv->unixfdsink) {
            gst_printerrln("Unable to find unixfdsink element, please install gst-plugins-bad.");
            return false;
        }

        sink->priv->volumeElement = gst_element_factory_make("volume", nullptr);
        auto queue = gst_element_factory_make("queue", nullptr);

        g_object_set(sink->priv->unixfdsink.get(), "socket-path", socketPath.utf8().data(), "wait-for-connection", TRUE, nullptr);
        sink->priv->socketPath = WTF::move(socketPath);
        gst_bin_add_many(GST_BIN_CAST(sink), sink->priv->volumeElement.get(), queue, sink->priv->unixfdsink.get(), nullptr);
        gst_element_link_many(sink->priv->volumeElement.get(), queue, sink->priv->unixfdsink.get(), nullptr);

        auto targetPad = adoptGRef(gst_element_get_static_pad(sink->priv->volumeElement.get(), "sink"));
        auto sinkPad = webkitGstGhostPadFromStaticTemplate(&audioSinkTemplate, "sink"_s, targetPad.get());
        gst_element_add_pad(GST_ELEMENT_CAST(sink), sinkPad);
        GST_OBJECT_FLAG_SET(sinkPad, GST_PAD_FLAG_NEED_PARENT);
        return true;
    }

    auto enableAudioMixer = CStringView::unsafeFromUTF8(g_getenv("WEBKIT_GST_ENABLE_AUDIO_MIXER"));
    if (!enableAudioMixer || enableAudioMixer != "1"_s)
        return false;

    if (!GStreamerAudioMixer::isAvailable()) {
        GST_WARNING("Internal audio mixing request cannot be fulfilled.");
        return false;
    }

    sink->priv->interAudioSink = makeGStreamerElement("interaudiosink"_s);
    RELEASE_ASSERT(sink->priv->interAudioSink);

    gst_bin_add(GST_BIN_CAST(sink), sink->priv->interAudioSink.get());
    auto targetPad = adoptGRef(gst_element_get_static_pad(sink->priv->interAudioSink.get(), "sink"));
    gst_element_add_pad(GST_ELEMENT_CAST(sink), webkitGstGhostPadFromStaticTemplate(&audioSinkTemplate, "sink"_s, targetPad.get()));

    if (sink->priv->role != "webaudio"_s)
        return true;

    // Match the interaudiosrc period-time with the WebAudio renderQuantumSize applied to the
    // sample rate, otherwise the samples created by the source will have clipping, leading to
    // garbled rendering. For this to work the sample rate also needs to match between
    // webkitaudiosink and the caps negotiated on the audiomixer sink pad (this is handled in
    // webKitAudioSinkChangeState()).
    gst_pad_add_probe(targetPad.get(), GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM, reinterpret_cast<GstPadProbeCallback>(+[](GstPad* pad, GstPadProbeInfo* info, gpointer) -> GstPadProbeReturn {
        auto event = GST_PAD_PROBE_INFO_EVENT(info);
        if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
            return GST_PAD_PROBE_OK;

        GstCaps* caps;
        gst_event_parse_caps(event, &caps);

        if (gst_caps_is_empty(caps) || gst_caps_is_any(caps)) [[unlikely]]
            return GST_PAD_PROBE_OK;

        auto structure = gst_caps_get_structure(caps, 0);
        auto sampleRate = gstStructureGet<int>(structure, "rate"_s);
        if (!sampleRate) [[unlikely]]
            return GST_PAD_PROBE_OK;

        auto sink = adoptGRef(gst_pad_get_parent_element(pad));
        uint64_t periodTime = gst_util_uint64_scale_ceil(AudioUtilities::renderQuantumSize, GST_SECOND, *sampleRate);
        GStreamerAudioMixer::singleton().configureSourcePeriodTime(CStringView::unsafeFromUTF8(GST_ELEMENT_NAME(sink.get())), periodTime);
        return GST_PAD_PROBE_OK;
    }), nullptr, nullptr);
    return true;
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
    case WEBKIT_AUDIO_SINK_PROP_VOLUME: {
        GstObject* internalObject = getInternalVolumeObject(sink);
        g_object_set_property(G_OBJECT(internalObject), "volume", value);
        break;
    }
    case WEBKIT_AUDIO_SINK_PROP_MUTE: {
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
    case WEBKIT_AUDIO_SINK_PROP_VOLUME: {
        GstObject* internalObject = getInternalVolumeObject(sink);
        g_object_get_property(G_OBJECT(internalObject), "volume", value);
        break;
    }
    case WEBKIT_AUDIO_SINK_PROP_MUTE: {
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

    GST_DEBUG_OBJECT(sink, "Handling %s transition", gst_state_change_get_name(stateChange));

    auto& mixer = GStreamerAudioMixer::singleton();
    if (priv->interAudioSink && stateChange == GST_STATE_CHANGE_NULL_TO_READY) {
        std::optional<int> forcedSampleRate;
#if ENABLE(WEB_AUDIO)
        if (priv->role == "webaudio"_s)
            forcedSampleRate = AudioDestination::hardwareSampleRate();
#endif
        priv->mixerPad = mixer.registerProducer(priv->interAudioSink.get(), forcedSampleRate, priv->deviceId, priv->device);
    }

    if (priv->mixerPad)
        mixer.ensureState(stateChange, priv->deviceId);

    GstStateChangeReturn result = GST_ELEMENT_CLASS(webkit_audio_sink_parent_class)->change_state(element, stateChange);

    if (result <= GST_STATE_CHANGE_FAILURE)
        return result;

    switch (stateChange) {
    case GST_STATE_CHANGE_READY_TO_NULL:
        if (priv->mixerPad) {
            mixer.unregisterProducer(priv->mixerPad);
            priv->mixerPad = nullptr;
        }
        break;
    default:
        break;
    };

    return result;
}

static void webKitAudioSinkHandleMessage(GstBin* bin, GstMessage* message)
{
    auto self = WEBKIT_AUDIO_SINK(bin);
    auto scopeExit = makeScopeExit([&] {
        GST_BIN_CLASS(webkit_audio_sink_parent_class)->handle_message(bin, message);
    });

    if (self->priv->socketPath.isEmpty())
        return;

    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_STATE_CHANGED)
        return;

    if (GST_MESSAGE_SRC(message) != GST_OBJECT_CAST(self->priv->unixfdsink.get()))
        return;

    GstState currentState, newState;
    gst_message_parse_state_changed(message, &currentState, &newState, nullptr);

    if (currentState < GST_STATE_READY && newState == GST_STATE_READY && self->priv->audioSinkStartedCallback) {
        self->priv->audioSinkStartedCallback(self->priv->socketPath);
        return;
    }

    if (currentState == GST_STATE_READY && newState == GST_STATE_NULL && self->priv->audioSinkStoppedCallback)
        self->priv->audioSinkStoppedCallback(self->priv->socketPath);
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
        "Proxies audio data to WebKit's audio mixer or to a WPE external audio handler",
        "Philippe Normand <philn@igalia.com>");

    eklass->change_state = GST_DEBUG_FUNCPTR(webKitAudioSinkChangeState);

    auto binClass = GST_BIN_CLASS(klass);
    binClass->handle_message = webKitAudioSinkHandleMessage;
}

void webkitAudioSinkSetStartedCallback(WebKitAudioSink* sink, AudioSinkStartedCallback&& callback)
{
    sink->priv->audioSinkStartedCallback = WTF::move(callback);
}

void webkitAudioSinkSetStoppedCallback(WebKitAudioSink* sink, AudioSinkStoppedCallback&& callback)
{
    sink->priv->audioSinkStoppedCallback = WTF::move(callback);
}

GstElement* /* (transfer floating) */ webkitAudioSinkNew(const String& role, const String& deviceId, const GRefPtr<GstDevice>& device, String&& socketPath)
{
    auto element = GST_ELEMENT_CAST(g_object_new(WEBKIT_TYPE_AUDIO_SINK, nullptr));
    auto audioSink = WEBKIT_AUDIO_SINK(element);

    audioSink->priv->role = role;
    audioSink->priv->deviceId = deviceId;
    if (device)
        audioSink->priv->device = device;
    if (!webKitAudioSinkConfigure(audioSink, WTF::move(socketPath))) {
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
        std::optional<int> forcedSampleRate;
#if ENABLE(WEB_AUDIO)
        if (priv->role == "webaudio"_s)
            forcedSampleRate = AudioDestination::hardwareSampleRate();
#endif
        priv->mixerPad = mixer.registerProducer(priv->interAudioSink.get(), forcedSampleRate, priv->deviceId, priv->device);

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
