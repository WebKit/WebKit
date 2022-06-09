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

#include "config.h"
#include "GStreamerVideoEncoder.h"

#if ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/StdMap.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

GST_DEBUG_CATEGORY(webrtc_venc_debug);
#define GST_CAT_DEFAULT webrtc_venc_debug

#define KBIT_TO_BIT 1024

static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw(ANY)"));
static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-h264"));

using SetBitrateFunc = Function<void(GObject* encoder, const char* propertyName, int bitrate)>;
using SetupFunc = Function<void(WebKitWebrtcVideoEncoder*)>;

struct EncoderDefinition {
    GRefPtr<GstCaps> caps;
    const char* name;
    const char* parserName;
    GRefPtr<GstCaps> encodedFormat;
    SetBitrateFunc setBitrate;
    SetupFunc setupEncoder;
    const char* bitratePropertyName;
    const char* keyframeIntervalPropertyName;
};

enum EncoderId { None, X264, OpenH264, OmxH264 };

class Encoders {
public:
    static StdMap<EncoderId, EncoderDefinition>& singleton()
    {
        static StdMap<EncoderId, EncoderDefinition> encoders;
        return encoders;
    }

    static void registerEncoder(EncoderId id, const char* name, const char* parserName, const char* caps, const char* encodedFormat,
        SetupFunc&& setupEncoder, const char* bitratePropertyName, SetBitrateFunc&& setBitrate, const char* keyframeIntervalPropertyName)
    {
        auto feature = adoptGRef(gst_registry_lookup_feature(gst_registry_get(), name));
        if (!feature)
            return;

        singleton().emplace(std::make_pair(id, (EncoderDefinition) {
            .caps = adoptGRef(gst_caps_from_string(caps)),
            .name = name,
            .parserName = parserName,
            .encodedFormat = encodedFormat ? adoptGRef(gst_caps_from_string(encodedFormat)) : nullptr,
            .setBitrate = WTFMove(setBitrate),
            .setupEncoder = WTFMove(setupEncoder),
            .bitratePropertyName = bitratePropertyName,
            .keyframeIntervalPropertyName = keyframeIntervalPropertyName,
        }));
    }

    static EncoderDefinition* definition(EncoderId id)
    {
        if (id == None)
            return nullptr;
        return &singleton()[id];
    }
};

struct _WebKitWebrtcVideoEncoderPrivate {
    EncoderId encoderId;
    GRefPtr<GstElement> encoder;
    GRefPtr<GstElement> parser;
    unsigned bitrate;
};

#define webkit_webrtc_video_encoder_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitWebrtcVideoEncoder, webkit_webrtc_video_encoder, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(webrtc_venc_debug, "webrtcencoder", 0, "Video encoder for WebRTC"))

enum {
    PROP_FORMAT = 1,
    PROP_ENCODER,
    PROP_BITRATE,
    PROP_KEYFRAME_INTERVAL,
    N_PROPS
};

static void webrtcVideoEncoderGetProperty(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    auto* self = WEBKIT_WEBRTC_VIDEO_ENCODER(object);
    auto* priv = self->priv;

    switch (prop_id) {
    case PROP_FORMAT:
        if (priv->encoderId != None) {
            auto encoder = Encoders::definition(priv->encoderId);
            g_value_set_boxed(value, encoder->caps.get());
        } else
            g_value_set_boxed(value, nullptr);
        break;
    case PROP_ENCODER:
        g_value_set_object(value, priv->encoder.get());
        break;
    case PROP_BITRATE:
        g_value_set_uint(value, priv->bitrate);
        break;
    case PROP_KEYFRAME_INTERVAL:
        if (priv->encoder) {
            auto encoder = Encoders::definition(priv->encoderId);
            g_object_get_property(G_OBJECT(priv->encoder.get()), encoder->keyframeIntervalPropertyName, value);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void webrtcVideoEncoderSetBitrate(WebKitWebrtcVideoEncoder* self, guint bitrate)
{
    auto* priv = self->priv;
    priv->bitrate = bitrate;

    if (priv->encoderId != None) {
        auto encoder = Encoders::definition(priv->encoderId);
        encoder->setBitrate(G_OBJECT(priv->encoder.get()), encoder->bitratePropertyName, priv->bitrate);
    }
}

static void webrtcVideoEncoderSetEncoder(WebKitWebrtcVideoEncoder* self, GRefPtr<GstElement>&& encoderElement, EncoderId encoderId)
{
    auto* priv = self->priv;
    priv->encoderId = encoderId;
    priv->encoder = WTFMove(encoderElement);

    auto encoder = Encoders::definition(encoderId);
    ASSERT(encoder);
    if (encoder->parserName)
        priv->parser = makeGStreamerElement(encoder->parserName, nullptr);

    encoder->setupEncoder(self);

    gst_bin_add(GST_BIN_CAST(self), priv->encoder.get());

    auto sinkPadTarget = adoptGRef(gst_element_get_static_pad(priv->encoder.get(), "sink"));
    auto sinkPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), "sink"));
    gst_ghost_pad_set_target(GST_GHOST_PAD(sinkPad.get()), sinkPadTarget.get());

    GRefPtr<GstPad> srcPadTarget;
    if (priv->parser) {
        gst_bin_add(GST_BIN_CAST(self), priv->parser.get());
        gst_element_link(priv->encoder.get(), priv->parser.get());
        srcPadTarget = adoptGRef(gst_element_get_static_pad(priv->parser.get(), "src"));
    } else
        srcPadTarget = adoptGRef(gst_element_get_static_pad(priv->encoder.get(), "src"));

    if (encoder->encodedFormat) {
        auto* capsfilter = gst_element_factory_make("capsfilter", nullptr);
        g_object_set(capsfilter, "caps", encoder->encodedFormat.get(), nullptr);

        gst_bin_add(GST_BIN_CAST(self), capsfilter);
        auto sinkPad = adoptGRef(gst_element_get_static_pad(capsfilter, "sink"));
        gst_pad_link(srcPadTarget.get(), sinkPad.get());
        srcPadTarget = adoptGRef(gst_element_get_static_pad(capsfilter, "src"));
    }

    auto srcPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), "src"));
    gst_ghost_pad_set_target(GST_GHOST_PAD(srcPad.get()), srcPadTarget.get());

    webrtcVideoEncoderSetBitrate(self, priv->bitrate);
}

static void webrtcVideoEncoderSetFormat(WebKitWebrtcVideoEncoder* self, const GstCaps* caps)
{
    if (!caps)
        return;

    for (const auto& pair : Encoders::singleton()) {
        const auto& encoder = pair.second;
        if (gst_caps_can_intersect(encoder.caps.get(), caps)) {
            GRefPtr<GstElement> element = makeGStreamerElement(encoder.name, nullptr);
            webrtcVideoEncoderSetEncoder(self, WTFMove(element), pair.first);
            return;
        }
    }

    GST_ERROR("No encoder found for format %" GST_PTR_FORMAT, caps);
}

static void webrtcVideoEncoderSetProperty(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    auto* self = WEBKIT_WEBRTC_VIDEO_ENCODER(object);
    auto* priv = self->priv;

    switch (prop_id) {
    case PROP_FORMAT:
        webrtcVideoEncoderSetFormat(self, gst_value_get_caps(value));
        break;
    case PROP_BITRATE:
        webrtcVideoEncoderSetBitrate(self, g_value_get_uint(value));
        break;
    case PROP_KEYFRAME_INTERVAL:
        if (priv->encoder) {
            auto encoder = Encoders::definition(priv->encoderId);
            g_object_set(priv->encoder.get(), encoder->keyframeIntervalPropertyName, g_value_get_uint(value), nullptr);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void setBitrateKbitPerSec(GObject* encoder, const char* propertyName, int bitrate)
{
    g_object_set(encoder, propertyName, bitrate, nullptr);
}

static void setBitrateBitPerSec(GObject* encoder, const char* propertyName, int bitrate)
{
    g_object_set(encoder, propertyName, bitrate * KBIT_TO_BIT, nullptr);
}

static void webrtcVideoEncoderConstructed(GObject* encoder)
{
    auto* self = WEBKIT_WEBRTC_VIDEO_ENCODER(encoder);
    self->priv->encoderId = None;
    gst_element_add_pad(GST_ELEMENT_CAST(self), webkitGstGhostPadFromStaticTemplate(&sinkTemplate, "sink", nullptr));
    gst_element_add_pad(GST_ELEMENT_CAST(self), webkitGstGhostPadFromStaticTemplate(&srcTemplate, "src", nullptr));
}

static void webkit_webrtc_video_encoder_class_init(WebKitWebrtcVideoEncoderClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webrtcVideoEncoderConstructed;
    objectClass->get_property = webrtcVideoEncoderGetProperty;
    objectClass->set_property = webrtcVideoEncoderSetProperty;

    g_object_class_install_property(objectClass, PROP_FORMAT, g_param_spec_boxed("format", "Format as caps", "Set the caps of the format to be used.", GST_TYPE_CAPS, WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(objectClass, PROP_ENCODER, g_param_spec_object("encoder", "The actual encoder element", "The encoder element", GST_TYPE_ELEMENT, WEBKIT_PARAM_READABLE));

    g_object_class_install_property(objectClass, PROP_BITRATE, g_param_spec_uint("bitrate", "Bitrate", "The bitrate in kbit per second", 0, G_MAXINT, 2048,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    g_object_class_install_property(objectClass, PROP_KEYFRAME_INTERVAL, g_param_spec_uint("keyframe-interval", "Keyframe interval", "The interval between keyframes", 0, G_MAXINT, 0,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    Encoders::registerEncoder(OmxH264, "omxh264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
        [](WebKitWebrtcVideoEncoder* self) {
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "control-rate", "variable");
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "target-bitrate", setBitrateBitPerSec, "interval-intraframes");
    Encoders::registerEncoder(X264, "x264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
        [](WebKitWebrtcVideoEncoder* self) {
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "tune", "zerolatency");
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "bitrate", setBitrateKbitPerSec, "key-int-max");
    Encoders::registerEncoder(OpenH264, "openh264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
        [](WebKitWebrtcVideoEncoder* self) {
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "bitrate", setBitrateBitPerSec, "gop-size");
}

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
