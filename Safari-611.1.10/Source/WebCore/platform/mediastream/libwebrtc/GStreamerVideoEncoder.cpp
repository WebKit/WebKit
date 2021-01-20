/*
 * Copyright (C) 2018-2020 Metrological Group B.V.
 * Copyright (C) 2018-2020 Igalia S.L. All rights reserved.
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

/* NOTE: This file respects GStreamer coding style as we might want to upstream
 * that element in the future */

#include "config.h"
#include "GStreamerVideoEncoder.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "GRefPtrGStreamer.h"
#include <wtf/StdMap.h>

GST_DEBUG_CATEGORY (webrtc_venc_debug);
#define GST_CAT_DEFAULT webrtc_venc_debug

#define KBIT_TO_BIT 1024

static GstStaticPadTemplate sinkTemplate =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS ("video/x-raw(ANY);"));
static GstStaticPadTemplate srcTemplate =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS ("video/x-h264;video/x-vp8"));

typedef void (*SetBitrateFunc) (GObject * encoder, const gchar * propname,  gint bitrate);
typedef void (*SetupFunc) (WebrtcVideoEncoder * self);

struct EncoderDefinition {
    GRefPtr<GstCaps> caps;
    const gchar* name;
    const gchar* parserName;
    GRefPtr<GstCaps> encodedFormat;
    SetBitrateFunc setBitrate;
    SetupFunc setupEncoder;
    const gchar* bitratePropertyName;
    const gchar* keyframeIntervalPropertyName;
};

enum EncoderId { None, X264, OpenH264, OmxH264, Vp8 };

class Encoders {
public:
    static StdMap<EncoderId, EncoderDefinition>& singleton()
    {
        static StdMap<EncoderId, EncoderDefinition> encoders;
        return encoders;
    }

    static void registerEncoder(EncoderId id, const gchar* name, const gchar* parser_name, const gchar* caps, const gchar* encoded_format,
        SetupFunc setupEncoder, const gchar* bitrate_propname, SetBitrateFunc setBitrate, const gchar* keyframe_interval_propname)
    {
        GstPluginFeature *feature = gst_registry_lookup_feature(gst_registry_get(), name);
        if (!feature)
            return;
        gst_object_unref(feature);

        singleton().emplace(std::make_pair(id, (EncoderDefinition) {
            .caps = adoptGRef(gst_caps_from_string(caps)),
            .name = name,
            .parserName = parser_name,
            .encodedFormat = (encoded_format) ? adoptGRef(gst_caps_from_string(encoded_format)) : nullptr,
            .setBitrate = setBitrate,
            .setupEncoder = setupEncoder,
            .bitratePropertyName = bitrate_propname,
            .keyframeIntervalPropertyName = keyframe_interval_propname,
        }));
    }

    static const EncoderDefinition& definition(EncoderId id) {
        ASSERT(id != None);
        return singleton()[id];
    }
};

struct _WebrtcVideoEncoder {
    GstBin parent;

    EncoderId encoderId;
    GstElement* encoder;
    GstElement* parser;
    guint bitrate;
};

G_DEFINE_TYPE(WebrtcVideoEncoder, webrtc_video_encoder, GST_TYPE_BIN);

enum {
     PROP_FORMAT = 1,
     PROP_ENCODER,
     PROP_BITRATE,
     PROP_KEYFRAME_INTERVAL,
     N_PROPS
};

static void webrtcVideoEncoderGetProperty(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    WebrtcVideoEncoder* self = WEBRTC_VIDEO_ENCODER(object);

    switch (prop_id) {
    case PROP_FORMAT:
        if (self->encoderId != None) {
            auto &encoder = Encoders::definition(self->encoderId);
            g_value_set_boxed(value, encoder.caps.get());
        }  else
            g_value_set_boxed(value, nullptr);
        break;
    case PROP_ENCODER:
        g_value_set_object(value, self->encoder);
        break;
    case PROP_BITRATE:
        g_value_set_uint(value, self->bitrate);
        break;
    case PROP_KEYFRAME_INTERVAL:
        if (self->encoder) {
            auto &encoder = Encoders::definition(self->encoderId);
            g_object_get_property(G_OBJECT(self->encoder), encoder.keyframeIntervalPropertyName, value);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
  }
}

static void webrtcVideoEncoderSetBitrate(WebrtcVideoEncoder* self, guint bitrate)
{
    self->bitrate = bitrate;
    if (self->encoder) {
        auto &encoder = Encoders::definition(self->encoderId);
        encoder.setBitrate(G_OBJECT(self->encoder), encoder.bitratePropertyName, self->bitrate);
    }
}

static void webrtcVideoEncoderSetFormat(WebrtcVideoEncoder* self, const GstCaps* caps)
{
    g_return_if_fail(self->encoderId == None);
    g_return_if_fail(caps);

    for (const auto &pair : Encoders::singleton()) {
        const auto &encoder = pair.second;
        GstElement* capsfilter = nullptr;

        if (gst_caps_can_intersect(encoder.caps.get(), caps)) {
            GRefPtr<GstPad> tmppad;
            self->encoderId = pair.first;
            self->encoder = gst_element_factory_make(encoder.name, nullptr);

            if (encoder.parserName)
                self->parser = gst_element_factory_make(encoder.parserName, nullptr);

            encoder.setupEncoder(self);
            if (encoder.encodedFormat.get()) {
                capsfilter = gst_element_factory_make("capsfilter", nullptr);
                g_object_set(capsfilter, "caps", encoder.encodedFormat.get(), nullptr);
            }

            gst_bin_add(GST_BIN(self), self->encoder);

            tmppad = adoptGRef(gst_element_get_static_pad(self->encoder, "sink"));
            GRefPtr<GstPad> sinkpad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT(self), "sink"));
            gst_ghost_pad_set_target(GST_GHOST_PAD(sinkpad.get()), tmppad.get());

            tmppad = adoptGRef(gst_element_get_static_pad(self->encoder, "src"));
            if (self->parser) {
                gst_bin_add(GST_BIN(self), self->parser);
                gst_element_link(self->encoder, self->parser);
                tmppad = adoptGRef(gst_element_get_static_pad(self->parser, "src"));
            }

            if (capsfilter) {
                GRefPtr<GstPad> tmppad2 = adoptGRef(gst_element_get_static_pad(capsfilter, "sink"));

                gst_bin_add(GST_BIN(self), capsfilter);
                gst_pad_link(tmppad.get(), tmppad2.get());
                tmppad = adoptGRef(gst_element_get_static_pad(capsfilter, "src"));
            }

            GRefPtr<GstPad> srcpad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT (self), "src"));
            gboolean ret = gst_ghost_pad_set_target(GST_GHOST_PAD(srcpad.get()), tmppad.get());

            RELEASE_ASSERT(ret);

            webrtcVideoEncoderSetBitrate(self, self->bitrate);
            return;
        }
    }

    GST_ERROR("No encoder found for format %" GST_PTR_FORMAT, caps);
}

static void webrtcVideoEncoderSetProperty(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    WebrtcVideoEncoder* self = WEBRTC_VIDEO_ENCODER(object);

    switch (prop_id) {
    case PROP_FORMAT:
        webrtcVideoEncoderSetFormat(self, gst_value_get_caps(value));
        break;
    case PROP_BITRATE:
        webrtcVideoEncoderSetBitrate(self, g_value_get_uint(value));
        break;
    case PROP_KEYFRAME_INTERVAL:
        if (self->encoder) {
            auto &encoder = Encoders::definition(self->encoderId);
            g_object_set(self->encoder, encoder.keyframeIntervalPropertyName, g_value_get_uint(value), nullptr);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void setupX264enc(WebrtcVideoEncoder* self)
{
    gst_util_set_object_arg(G_OBJECT (self->encoder), "tune", "zerolatency");
    g_object_set(self->parser, "config-interval", 1, nullptr);
}

static void setupOpenh264enc(WebrtcVideoEncoder* self)
{
    g_object_set (self->parser, "config-interval", 1, nullptr);
}

static void setupOmxh264enc(WebrtcVideoEncoder* self)
{
  gst_util_set_object_arg(G_OBJECT(self->encoder), "control-rate", "variable");
  g_object_set(self->parser, "config-interval", 1, nullptr);
}

static void setBitrateKbitPerSec(GObject* encoder, const gchar* prop_name, gint bitrate)
{
  g_object_set(encoder, prop_name, bitrate, nullptr);
}

static void setBitrateBitPerSec(GObject* encoder, const gchar* prop_name, gint bitrate)
{
  g_object_set(encoder, prop_name, bitrate * KBIT_TO_BIT, nullptr);
}

static void webrtc_video_encoder_class_init(WebrtcVideoEncoderClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    GST_DEBUG_CATEGORY_INIT (webrtc_venc_debug, "webrtcencoder", 0, "Video encoder for WebRTC");

    object_class->get_property = webrtcVideoEncoderGetProperty;
    object_class->set_property = webrtcVideoEncoderSetProperty;

    g_object_class_install_property(object_class, PROP_FORMAT,
        g_param_spec_boxed("format", "Format as caps", "Set the caps of the format to be used.", GST_TYPE_CAPS,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(object_class, PROP_ENCODER,
        g_param_spec_object("encoder", "The actual encoder element", "The encoder element", GST_TYPE_ELEMENT,
        static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));

    g_object_class_install_property(object_class, PROP_BITRATE,
        g_param_spec_uint("bitrate", "Bitrate",  "The bitrate in kbit per second", 0, G_MAXINT, 2048,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    g_object_class_install_property(object_class, PROP_KEYFRAME_INTERVAL,
        g_param_spec_uint("keyframe-interval", "Keyframe interval", "The interval between keyframes", 0, G_MAXINT, 0,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    Encoders::registerEncoder(OmxH264, "omxh264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
        setupOmxh264enc, "target-bitrate", setBitrateBitPerSec, "interval-intraframes");
    Encoders::registerEncoder(X264, "x264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
        setupX264enc, "bitrate", setBitrateKbitPerSec, "key-int-max");
    Encoders::registerEncoder(OpenH264, "openh264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
        setupOpenh264enc, "bitrate", setBitrateBitPerSec, "gop-size");
}

static void webrtc_video_encoder_init (WebrtcVideoEncoder* self)
{
    self->encoderId = None;
    gst_element_add_pad(GST_ELEMENT(self),
        gst_ghost_pad_new_no_target_from_template("sink", gst_static_pad_template_get(&sinkTemplate)));

    gst_element_add_pad(GST_ELEMENT(self),
        gst_ghost_pad_new_no_target_from_template("src", gst_static_pad_template_get(&srcTemplate)));
}

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
