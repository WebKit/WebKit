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

using namespace WebCore;

struct _WebKitAudioSinkPrivate {
    GRefPtr<GstElement> interAudioSink;
    GRefPtr<GstPad> mixerPad;
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

static bool webKitAudioSinkConfigure(WebKitAudioSink* sink)
{
    const char* value = g_getenv("WEBKIT_GST_ENABLE_AUDIO_MIXER");
    if (value && !strcmp(value, "1")) {
        if (!GStreamerAudioMixer::isAvailable()) {
            GST_WARNING("Internal audio mixing request cannot be fulfilled.");
            return false;
        }

        sink->priv->interAudioSink = makeGStreamerElement("interaudiosink", nullptr);
        RELEASE_ASSERT(sink->priv->interAudioSink);

        gst_bin_add(GST_BIN_CAST(sink), sink->priv->interAudioSink.get());
        auto targetPad = adoptGRef(gst_element_get_static_pad(sink->priv->interAudioSink.get(), "sink"));
        gst_element_add_pad(GST_ELEMENT_CAST(sink), webkitGstGhostPadFromStaticTemplate(&sinkTemplate, "sink", targetPad.get()));
        return true;
    }
    return false;
}

static void webKitAudioSinkSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* pspec)
{
    WebKitAudioSink* sink = WEBKIT_AUDIO_SINK(object);

    switch (propID) {
    case PROP_VOLUME: {
        g_object_set_property(G_OBJECT(sink->priv->mixerPad.get()), "volume", value);
        break;
    }
    case PROP_MUTE: {
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
    case PROP_VOLUME: {
        g_object_get_property(G_OBJECT(sink->priv->mixerPad.get()), "volume", value);
        break;
    }
    case PROP_MUTE: {
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
        priv->mixerPad = mixer.registerProducer(priv->interAudioSink.get());

    if (priv->mixerPad)
        mixer.ensureState(stateChange);

    GstStateChangeReturn result = GST_CALL_PARENT_WITH_DEFAULT(GST_ELEMENT_CLASS, change_state, (element, stateChange), GST_STATE_CHANGE_FAILURE);

    if (priv->mixerPad && stateChange == GST_STATE_CHANGE_READY_TO_NULL && result > GST_STATE_CHANGE_FAILURE) {
        mixer.unregisterProducer(priv->mixerPad);
        priv->mixerPad = nullptr;
    }

    return result;
}

static void webKitAudioSinkConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    GST_OBJECT_FLAG_SET(GST_OBJECT_CAST(object), GST_ELEMENT_FLAG_SINK);
    gst_bin_set_suppressed_flags(GST_BIN_CAST(object), static_cast<GstElementFlags>(GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK));
}

static void webkit_audio_sink_class_init(WebKitAudioSinkClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    oklass->set_property = webKitAudioSinkSetProperty;
    oklass->get_property = webKitAudioSinkGetProperty;
    oklass->constructed = webKitAudioSinkConstructed;

    g_object_class_install_property(oklass, PROP_VOLUME,
        g_param_spec_double("volume", nullptr, nullptr, 0, 10, 1,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(oklass, PROP_MUTE,
        g_param_spec_boolean("mute", nullptr, nullptr, FALSE,
            static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_static_pad_template(eklass, &sinkTemplate);
    gst_element_class_set_metadata(eklass, "WebKit Audio sink element", "Sink/Audio",
        "Proxies audio data to WebKit's audio mixer",
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

#undef GST_CAT_DEFAULT

#endif // USE(GSTREAMER)
