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
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>

using namespace WebCore;

GST_DEBUG_CATEGORY(webrtc_venc_debug);
#define GST_CAT_DEFAULT webrtc_venc_debug

#define KBIT_TO_BIT 1024

static GstStaticPadTemplate sinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw(ANY)"));
static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-h264;video/x-vp8;video/x-vp9;video/x-h265"));

using SetBitrateFunc = Function<void(GObject* encoder, const char* propertyName, int bitrate)>;
using SetupFunc = Function<void(WebKitWebrtcVideoEncoder*)>;

struct EncoderDefinition {
    GRefPtr<GstCaps> caps;
    const char* name;
    const char* parserName;
    GRefPtr<GstElementFactory> factory;
    GRefPtr<GstCaps> encodedFormat;
    SetBitrateFunc setBitrate;
    SetupFunc setupEncoder;
    const char* bitratePropertyName;
    const char* keyframeIntervalPropertyName;
};

enum EncoderId {
    None,
    X264,
    OpenH264,
    OmxH264,
    VaapiH264,
    VaapiH265,
    Vp8,
    Vp9
};

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
        auto encoderFactory = adoptGRef(gst_element_factory_find(name));
        if (!encoderFactory) {
            GST_WARNING("Encoder %s not found, will not be used", name);
            return;
        }

        if (gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE_CAST(encoderFactory.get())) < GST_RANK_MARGINAL) {
            GST_WARNING("Encoder %s rank is below MARGINAL, will not be used.", name);
            return;
        }

        if (parserName) {
            auto parserFactory = adoptGRef(gst_element_factory_find(parserName));
            if (!parserFactory) {
                GST_WARNING("Parser %s is required for encoder %s. Skipping registration", parserName, name);
                return;
            }
        }

        singleton().emplace(std::make_pair(id, EncoderDefinition {
            .caps = adoptGRef(gst_caps_from_string(caps)),
            .name = name,
            .parserName = parserName,
            .factory = WTFMove(encoderFactory),
            .encodedFormat = encodedFormat ? adoptGRef(gst_caps_from_string(encodedFormat)) : nullptr,
            .setBitrate = WTFMove(setBitrate),
            .setupEncoder = WTFMove(setupEncoder),
            .bitratePropertyName = bitratePropertyName,
            .keyframeIntervalPropertyName = keyframeIntervalPropertyName,
        }));
        GST_INFO("Encoder %s registered", name);
    }

    static EncoderDefinition* definition(EncoderId id)
    {
        if (id == None)
            return nullptr;
        return &singleton()[id];
    }
};

/* Internal bin structure: videoconvert ! inputCapsFilter ! encoder ! outputCapsFilter ! (optional
   parser) ! capsFilter */
struct _WebKitWebrtcVideoEncoderPrivate {
    EncoderId encoderId;
    GRefPtr<GstElement> encoder;
    GRefPtr<GstElement> parser;
    GRefPtr<GstElement> capsFilter;
    GRefPtr<GstElement> inputCapsFilter;
    GRefPtr<GstElement> outputCapsFilter;
    GRefPtr<GstElement> videoConvert;
    GRefPtr<GstElement> videoScale;
    GRefPtr<GstCaps> encodedCaps;
    unsigned bitrate;
};

#define webkit_webrtc_video_encoder_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitWebrtcVideoEncoder, webkit_webrtc_video_encoder, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(webrtc_venc_debug, "webkitwebrtcvideoencoder", 0, "Video encoder for WebRTC"))

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

static void webrtcVideoEncoderSetEncoder(WebKitWebrtcVideoEncoder* self, EncoderId encoderId, GRefPtr<GstCaps>&& encodedCaps)
{
    ASSERT(encoderId != EncoderId::None);

    auto* priv = self->priv;
    auto srcPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), "src"));

    priv->encodedCaps = WTFMove(encodedCaps);

    gst_element_set_locked_state(GST_ELEMENT_CAST(self), TRUE);

    if (priv->capsFilter) {
        gst_element_set_locked_state(priv->capsFilter.get(), TRUE);
        auto sinkPad = adoptGRef(gst_element_get_static_pad(priv->capsFilter.get(), "sink"));
        auto peerPad = adoptGRef(gst_pad_get_peer(sinkPad.get()));
        auto peer = adoptGRef(gst_pad_get_parent_element(peerPad.get()));
        gst_element_set_state(priv->capsFilter.get(), GST_STATE_NULL);
        gst_element_unlink(peer.get(), priv->capsFilter.get());
        gst_bin_remove(GST_BIN_CAST(self), priv->capsFilter.get());
        sinkPad.clear();
        priv->capsFilter.clear();
    }

    auto encoderDefinition = Encoders::definition(encoderId);
    ASSERT(encoderDefinition);

    bool shouldLinkEncoder = false;
    if (priv->encoderId != encoderId) {
        if (priv->encoder) {
#ifndef GST_DISABLE_GST_DEBUG
            auto previousEncoder = Encoders::definition(priv->encoderId);
            GST_DEBUG_OBJECT(self, "Switching from %s to %s", previousEncoder->name, encoderDefinition->name);
#endif
            gst_element_set_locked_state(priv->encoder.get(), TRUE);
            gst_element_set_state(priv->encoder.get(), GST_STATE_NULL);
            gst_element_unlink(priv->inputCapsFilter.get(), priv->encoder.get());
            gst_bin_remove(GST_BIN_CAST(self), priv->encoder.get());
        }
        priv->encoder = gst_element_factory_create(encoderDefinition->factory.get(), nullptr);
        gst_bin_add(GST_BIN_CAST(self), priv->encoder.get());
        shouldLinkEncoder = true;
    } else {
        GST_DEBUG_OBJECT(self, "Reconfiguring existing %s encoder", encoderDefinition->name);
        gst_element_set_state(priv->encoder.get(), GST_STATE_READY);
    }

    if (priv->parser) {
        gst_element_set_locked_state(priv->parser.get(), TRUE);
        gst_element_set_state(priv->parser.get(), GST_STATE_NULL);
        gst_element_unlink_many(priv->encoder.get(), priv->parser.get(), priv->outputCapsFilter.get(), nullptr);
        gst_bin_remove(GST_BIN_CAST(self), priv->parser.get());
        priv->parser.clear();
    }

    priv->encoderId = encoderId;

    if (!priv->inputCapsFilter) {
        priv->inputCapsFilter = gst_element_factory_make("capsfilter", nullptr);
        gst_bin_add(GST_BIN_CAST(self), priv->inputCapsFilter.get());
    }

    if (!priv->videoScale) {
        priv->videoScale = makeGStreamerElement("videoscale", nullptr);
        gst_bin_add(GST_BIN_CAST(self), priv->videoScale.get());
    }

    if (!priv->videoConvert) {
        priv->videoConvert = makeGStreamerElement("videoconvert", nullptr);
        gst_bin_add(GST_BIN_CAST(self), priv->videoConvert.get());

        auto sinkPadTarget = adoptGRef(gst_element_get_static_pad(priv->videoConvert.get(), "sink"));
        auto sinkPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), "sink"));
        gst_ghost_pad_set_target(GST_GHOST_PAD(sinkPad.get()), sinkPadTarget.get());
    } else {
        gst_element_unlink(priv->videoConvert.get(), priv->inputCapsFilter.get());
        auto caps = adoptGRef(gst_caps_new_any());
        g_object_set(priv->inputCapsFilter.get(), "caps", caps.get(), nullptr);
    }

    if (encoderDefinition->parserName) {
        priv->parser = makeGStreamerElement(encoderDefinition->parserName, nullptr);

        if (!priv->outputCapsFilter) {
            priv->outputCapsFilter = gst_element_factory_make("capsfilter", nullptr);
            gst_bin_add(GST_BIN_CAST(self), priv->outputCapsFilter.get());
        }
    }

    encoderDefinition->setupEncoder(self);

    gst_element_link_many(priv->videoConvert.get(), priv->videoScale.get(), priv->inputCapsFilter.get(), nullptr);
    if (shouldLinkEncoder)
        gst_element_link(priv->inputCapsFilter.get(), priv->encoder.get());

    if (priv->parser) {
        gst_bin_add(GST_BIN_CAST(self), priv->parser.get());
        if (shouldLinkEncoder)
            gst_element_link(priv->encoder.get(), priv->outputCapsFilter.get());
        gst_element_link(priv->outputCapsFilter.get(), priv->parser.get());
    }

    priv->capsFilter = gst_element_factory_make("capsfilter", nullptr);
    if (encoderDefinition->encodedFormat)
        g_object_set(priv->capsFilter.get(), "caps", encoderDefinition->encodedFormat.get(), nullptr);
    else
        g_object_set(priv->capsFilter.get(), "caps", priv->encodedCaps.get(), nullptr);

    gst_bin_add(GST_BIN_CAST(self), priv->capsFilter.get());

    auto srcPadTarget = adoptGRef(gst_element_get_static_pad(priv->capsFilter.get(), "src"));
    gst_ghost_pad_set_target(GST_GHOST_PAD(srcPad.get()), srcPadTarget.get());

    gst_element_link(priv->parser ? priv->parser.get() : priv->encoder.get(), priv->capsFilter.get());

    gst_bin_sync_children_states(GST_BIN_CAST(self));
    gst_element_set_locked_state(GST_ELEMENT_CAST(self), FALSE);

    webrtcVideoEncoderSetBitrate(self, priv->bitrate);
}

EncoderId webrtcVideoEncoderFindForFormat(WebKitWebrtcVideoEncoder* self, const GRefPtr<GstCaps>& caps)
{
    if (!caps)
        return None;

    Vector<std::pair<EncoderId, const EncoderDefinition*>> candidates;
    GST_DEBUG_OBJECT(self, "Looking for an encoder matching caps %" GST_PTR_FORMAT, caps.get());
    for (const auto& [id, encoder] : Encoders::singleton()) {
        if (gst_element_factory_can_src_any_caps(encoder.factory.get(), caps.get())) {
            GST_DEBUG_OBJECT(self, "Compatible encoder found: %s", encoder.name);
            candidates.append(std::make_pair(id, &encoder));
        }
    }

    if (candidates.isEmpty())
        return None;

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
        auto rankA = gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE_CAST(a.second->factory.get()));
        auto rankB = gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE_CAST(b.second->factory.get()));
        return rankA > rankB;
    });

    GST_DEBUG_OBJECT(self, "The highest ranked encoder is %s", candidates[0].second->name);
    return candidates[0].first;
}

bool webrtcVideoEncoderSupportsFormat(WebKitWebrtcVideoEncoder* self, const GRefPtr<GstCaps>& caps)
{
    return webrtcVideoEncoderFindForFormat(self, caps) != None;
}

bool webrtcVideoEncoderSetFormat(WebKitWebrtcVideoEncoder* self, GRefPtr<GstCaps>&& caps)
{
    auto encoderId = webrtcVideoEncoderFindForFormat(self, caps);
    if (encoderId == None) {
        GST_ERROR_OBJECT(self, "No encoder found for format %" GST_PTR_FORMAT, caps.get());
        return false;
    }

    webrtcVideoEncoderSetEncoder(self, encoderId, WTFMove(caps));
    return true;
}

static void webrtcVideoEncoderSetProperty(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    auto* self = WEBKIT_WEBRTC_VIDEO_ENCODER(object);
    auto* priv = self->priv;

    switch (prop_id) {
    case PROP_FORMAT: {
        auto caps = adoptGRef(gst_caps_copy(gst_value_get_caps(value)));
        webrtcVideoEncoderSetFormat(self, WTFMove(caps));
        break;
    }
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
    GST_INFO_OBJECT(encoder, "Setting bitrate to %d Kbits/sec", bitrate);
    g_object_set(encoder, propertyName, bitrate, nullptr);
}

static void setBitrateBitPerSec(GObject* encoder, const char* propertyName, int bitrate)
{
    GST_INFO_OBJECT(encoder, "Setting bitrate to %d bits/sec", bitrate);
    g_object_set(encoder, propertyName, bitrate * KBIT_TO_BIT, nullptr);
}

static GRefPtr<GstCaps> createSrcPadTemplateCaps()
{
    auto* caps = gst_caps_new_empty();

    for (const auto& [id, encoder] : Encoders::singleton()) {
        if (encoder.encodedFormat)
            caps = gst_caps_merge(caps, gst_caps_ref(encoder.encodedFormat.get()));
        else
            caps = gst_caps_merge(caps, gst_caps_ref(encoder.caps.get()));
    }

    GST_DEBUG("Source pad template caps: %" GST_PTR_FORMAT, caps);
    return caps;
}

static void webrtcVideoEncoderConstructed(GObject* encoder)
{
    auto* self = WEBKIT_WEBRTC_VIDEO_ENCODER(encoder);
    self->priv->encoderId = None;

    auto* sinkPad = webkitGstGhostPadFromStaticTemplate(&sinkTemplate, "sink", nullptr);
    GST_OBJECT_FLAG_SET(sinkPad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_event_function(sinkPad, reinterpret_cast<GstPadEventFunction>(+[](GstPad* pad, GstObject* parent, GstEvent* event) -> gboolean {
        if (GST_EVENT_TYPE(event) == GST_EVENT_CUSTOM_DOWNSTREAM_OOB) {
            const auto* structure = gst_event_get_structure(event);
            if (gst_structure_has_name(structure, "encoder-bitrate-change-request")) {
                uint32_t bitrate;
                gst_structure_get_uint(structure, "bitrate", &bitrate);
                g_object_set(parent, "target-bitrate", bitrate, nullptr);
                return TRUE;
            }
        }
        return gst_pad_event_default(pad, parent, event);
    }));
    gst_element_add_pad(GST_ELEMENT_CAST(self), sinkPad);

    gst_element_add_pad(GST_ELEMENT_CAST(self), webkitGstGhostPadFromStaticTemplate(&srcTemplate, "src", nullptr));
}

static void webkit_webrtc_video_encoder_class_init(WebKitWebrtcVideoEncoderClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webrtcVideoEncoderConstructed;
    objectClass->get_property = webrtcVideoEncoderGetProperty;
    objectClass->set_property = webrtcVideoEncoderSetProperty;

    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(elementClass, "WebKit WebRTC video encoder", "Codec/Encoder/Video", "Encodes video for streaming", "Igalia");
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&sinkTemplate));

    Encoders::registerEncoder(OmxH264, "omxh264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream,profile=baseline",
        [](WebKitWebrtcVideoEncoder* self) {
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "control-rate", "variable");
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "target-bitrate", setBitrateBitPerSec, "interval-intraframes");
    Encoders::registerEncoder(X264, "x264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream",
        [](WebKitWebrtcVideoEncoder* self) {
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "tune", "zerolatency");
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "speed-preset", "ultrafast");
            g_object_set(self->priv->encoder.get(), "key-int-max", 15, "threads", 4, nullptr);
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);

            const auto* structure = gst_caps_get_structure(self->priv->encodedCaps.get(), 0);
            auto inputCaps = adoptGRef(gst_caps_new_any());
            if (const char* profileString = gst_structure_get_string(structure, "profile")) {
                auto profile = StringView::fromLatin1(profileString);
                if (profile.find("high"_s) != notFound)
                    inputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "Y444", nullptr));
            }
            g_object_set(self->priv->inputCapsFilter.get(), "caps", inputCaps.get(), nullptr);
            g_object_set(self->priv->outputCapsFilter.get(), "caps", self->priv->encodedCaps.get(), nullptr);
        }, "bitrate", setBitrateKbitPerSec, "key-int-max");
    Encoders::registerEncoder(OpenH264, "openh264enc", "h264parse", "video/x-h264",
        "video/x-h264,alignment=au,stream-format=byte-stream",
        [](WebKitWebrtcVideoEncoder* self) {
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
            g_object_set(self->priv->outputCapsFilter.get(), "caps", self->priv->encodedCaps.get(), nullptr);
        }, "bitrate", setBitrateBitPerSec, "gop-size");
    Encoders::registerEncoder(Vp8, "vp8enc", nullptr, "video/x-vp8", nullptr,
        [](WebKitWebrtcVideoEncoder* self) {
            gst_preset_load_preset(GST_PRESET(self->priv->encoder.get()), "Profile Realtime");
        }, "target-bitrate", setBitrateBitPerSec, "keyframe-max-dist");

    Encoders::registerEncoder(Vp9, "vp9enc", nullptr, "video/x-vp9", nullptr,
        [](WebKitWebrtcVideoEncoder* self) {
            g_object_set(self->priv->encoder.get(), "threads", 4, "cpu-used", 4, "tile-rows", 2, "row-mt", true, nullptr);
            auto inputCaps = adoptGRef(gst_caps_new_any());
            const auto* structure = gst_caps_get_structure(self->priv->encodedCaps.get(), 0);
            if (const char* profileString = gst_structure_get_string(structure, "profile")) {
                auto profile = StringView::fromLatin1(profileString);
                auto profileId = parseInteger<int>(profile, 10);
                if (profileId && *profileId >= 2)
                    inputCaps = adoptGRef(gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "I420_10LE", nullptr));
            }
            g_object_set(self->priv->inputCapsFilter.get(), "caps", inputCaps.get(), nullptr);
        }, "target-bitrate", setBitrateBitPerSec, "keyframe-max-dist");

    Encoders::registerEncoder(VaapiH264, "vah264lpenc", "h264parse", "video/x-h264", nullptr,
        [](WebKitWebrtcVideoEncoder* self) {
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "bitrate", setBitrateKbitPerSec, "key-int-max");

    Encoders::registerEncoder(VaapiH264, "vah264enc", "h264parse", "video/x-h264", nullptr,
        [](WebKitWebrtcVideoEncoder* self) {
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "rate-control", "cbr");
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "bitrate", setBitrateKbitPerSec, "key-int-max");

    Encoders::registerEncoder(VaapiH265, "vah265enc", "h265parse", "video/x-h265", nullptr,
        [](WebKitWebrtcVideoEncoder* self) {
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "rate-control", "cbr");
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "bitrate", setBitrateKbitPerSec, "key-int-max");

    auto srcPadTemplateCaps = createSrcPadTemplateCaps();
    gst_element_class_add_pad_template(elementClass, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, srcPadTemplateCaps.get()));

    g_object_class_install_property(objectClass, PROP_FORMAT, g_param_spec_boxed("format", "Format as caps", "Set the caps of the format to be used.", GST_TYPE_CAPS, WEBKIT_PARAM_READWRITE));

    g_object_class_install_property(objectClass, PROP_ENCODER, g_param_spec_object("encoder", "The actual encoder element", "The encoder element", GST_TYPE_ELEMENT, WEBKIT_PARAM_READABLE));

    g_object_class_install_property(objectClass, PROP_BITRATE, g_param_spec_uint("bitrate", "Bitrate", "The bitrate in kbit per second", 0, G_MAXINT, 2048,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    g_object_class_install_property(objectClass, PROP_KEYFRAME_INTERVAL, g_param_spec_uint("keyframe-interval", "Keyframe interval", "The interval between keyframes", 0, G_MAXINT, 0,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));
}

#endif // ENABLE(VIDEO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
