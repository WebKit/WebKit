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
#include "VideoEncoderPrivateGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCodecUtilities.h"
#include "GStreamerCommon.h"
#include "GUniquePtrGStreamer.h"
#include "IntSize.h"
#include "NotImplemented.h"
#include <gst/video/gstvideoencoder.h>
#include <wtf/StdMap.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/StringView.h>

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebKitVideoEncoderBitRateAllocation);

GST_DEBUG_CATEGORY(video_encoder_debug);
#define GST_CAT_DEFAULT video_encoder_debug

#define KBIT_TO_BIT 1000

// FIXME: Make this configurable at runtime?
#define NUMBER_OF_THREADS 4

#define MAX_WIDTH 4096
#define MAX_HEIGHT 4096

static GstStaticPadTemplate encoderSinkTemplate = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw(ANY)"));
static GstStaticPadTemplate encoderSrcTemplate = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-h264;video/x-vp8;video/x-vp9;video/x-h265;video/x-av1"));

// https://www.w3.org/TR/mediastream-recording/#bitratemode
typedef enum {
    CONSTANT_BITRATE_MODE = 0,
    VARIABLE_BITRATE_MODE = 1,
} BitrateMode;

#define VIDEO_ENCODER_TYPE_BITRATE_MODE (videoEncoderBitrateModeGetType())
static GType videoEncoderBitrateModeGetType()
{
    static GType bitrateModeGType = 0;
    static const GEnumValue values[] = {
        { CONSTANT_BITRATE_MODE, "Encode at a constant bitrate", "constant" },
        { VARIABLE_BITRATE_MODE, "Encode using a variable bitrate, allowing more space to be used for complex signals and less space for less complex signals.", "variable" },
        { 0, nullptr, nullptr },
    };

    if (!bitrateModeGType)
        bitrateModeGType = g_enum_register_static("BitrateMode", values);
    return bitrateModeGType;
}

// https://www.w3.org/TR/webcodecs/#enumdef-latencymode
typedef enum {
    QUALITY_LATENCY_MODE = 0,
    REALTIME_LATENCY_MODE = 1,
} LatencyMode;

#define VIDEO_ENCODER_TYPE_LATENCY_MODE (videoEncoderLatencyModeGetType())
static GType videoEncoderLatencyModeGetType()
{
    static GType latencyModeGType = 0;
    static const GEnumValue values[] = {
        { QUALITY_LATENCY_MODE, "Optimize for encoding quality", "quality" },
        { REALTIME_LATENCY_MODE, "Optimize for low latency", "realtime" },
        { 0, nullptr, nullptr },
    };

    if (!latencyModeGType)
        latencyModeGType = g_enum_register_static("LatencyMode", values);
    return latencyModeGType;
}

using SetBitrateFunc = Function<void(GObject* encoder, ASCIILiteral propertyName, int bitrate)>;
using SetupFunc = Function<void(WebKitVideoEncoder*)>;
using SetBitrateModeFunc = Function<void(GstElement*, BitrateMode)>;
using SetLatencyModeFunc = Function<void(GstElement*, LatencyMode)>;
using SetBitRateAllocationFunc = Function<void(GstElement*, const WebKitVideoEncoderBitRateAllocation&)>;

struct EncoderDefinition {
    GRefPtr<GstCaps> caps;
    ASCIILiteral name;
    ASCIILiteral parserName;
    GRefPtr<GstElementFactory> factory;
    GRefPtr<GstCaps> encodedFormat;
    SetBitrateFunc setBitrate;
    SetupFunc setupEncoder;
    SetBitrateModeFunc setBitrateMode;
    SetLatencyModeFunc setLatencyMode;
    SetBitRateAllocationFunc setBitRateAllocation;
    ASCIILiteral bitratePropertyName;
    ASCIILiteral keyframeIntervalPropertyName;
};

static void defaultSetBitRateAllocation(GstElement*, const WebKitVideoEncoderBitRateAllocation&)
{
    notImplemented();
}

enum EncoderId {
    None,
    X264,
    X265,
    OpenH264,
    OmxH264,
    VaapiH264,
    VaapiH264LP,
    VaapiH265,
    Vp8,
    Vp9,
    Av1,
    VaapiAv1,
    SvtAv1
};

class Encoders {
public:
    static StdMap<EncoderId, EncoderDefinition>& singleton()
    {
        static StdMap<EncoderId, EncoderDefinition> encoders;
        return encoders;
    }

    static void registerEncoder(EncoderId id, ASCIILiteral name, ASCIILiteral parserName, ASCIILiteral capsString, ASCIILiteral encodedFormatString,
        SetupFunc&& setupEncoder, ASCIILiteral bitratePropertyName, SetBitrateFunc&& setBitrate, ASCIILiteral keyframeIntervalPropertyName, SetBitrateModeFunc&& setBitrateMode, SetLatencyModeFunc&& setLatency, SetBitRateAllocationFunc&& setBitRateAllocation = defaultSetBitRateAllocation)
    {
        auto encoderFactory = adoptGRef(gst_element_factory_find(name));
        if (!encoderFactory) {
            GST_DEBUG("Encoder %s not found, will not be used", name.characters());
            return;
        }

        if (gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE_CAST(encoderFactory.get())) < GST_RANK_MARGINAL) {
            GST_DEBUG("Encoder %s rank is below MARGINAL, will not be used.", name.characters());
            return;
        }

        if (parserName) {
            auto parserFactory = adoptGRef(gst_element_factory_find(parserName.characters()));
            if (!parserFactory) {
                GST_WARNING("Parser %s is required for encoder %s. Skipping registration", parserName.characters(), name.characters());
                return;
            }
        }

        auto caps = adoptGRef(gst_caps_from_string(capsString));
        GST_MINI_OBJECT_FLAG_SET(caps.get(), GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

        GRefPtr<GstCaps> encodedFormat;
        if (encodedFormatString) {
            encodedFormat = adoptGRef(gst_caps_from_string(encodedFormatString));
            GST_MINI_OBJECT_FLAG_SET(encodedFormat.get(), GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
        }

        singleton().emplace(std::make_pair(id, EncoderDefinition {
            .caps = WTFMove(caps),
            .name = name,
            .parserName = parserName,
            .factory = WTFMove(encoderFactory),
            .encodedFormat = WTFMove(encodedFormat),
            .setBitrate = WTFMove(setBitrate),
            .setupEncoder = WTFMove(setupEncoder),
            .setBitrateMode = WTFMove(setBitrateMode),
            .setLatencyMode = WTFMove(setLatency),
            .setBitRateAllocation = WTFMove(setBitRateAllocation),
            .bitratePropertyName = bitratePropertyName,
            .keyframeIntervalPropertyName = keyframeIntervalPropertyName,
        }));
        GST_INFO("Encoder %s registered", name.characters());
    }

    static EncoderDefinition* definition(EncoderId id)
    {
        if (id == None)
            return nullptr;
        return &singleton()[id];
    }
};

void teardownVideoEncoderSingleton()
{
    Encoders::singleton().clear();
}

/* Internal bin structure: videoconvert ! inputCapsFilter ! encoder ! outputCapsFilter ! (optional
   parser) ! capsFilter */
struct _WebKitVideoEncoderPrivate {
    EncoderId encoderId;
    GRefPtr<GstElement> encoder;
    GRefPtr<GstElement> parser;
    GRefPtr<GstElement> outputCapsFilter;
    GRefPtr<GstCaps> encodedCaps;
    unsigned bitrate;
    BitrateMode bitrateMode;
    LatencyMode latencyMode;
    RefPtr<WebKitVideoEncoderBitRateAllocation> bitRateAllocation;
    double scaleResolutionDownBy;
    bool enableVideoFlip;
};

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitVideoEncoder, webkit_video_encoder, GST_TYPE_BIN,
    GST_DEBUG_CATEGORY_INIT(video_encoder_debug, "webkitvideoencoderprivate", 0, "WebKit Video Encoder Private"))

enum {
    PROP_ENCODER = 1,
    PROP_BITRATE,
    PROP_KEYFRAME_INTERVAL,
    PROP_BITRATE_MODE,
    PROP_LATENCY_MODE,
    PROP_SCALE_RESOLUTION_DOWN_BY,
    N_PROPS
};

static void videoEncoderGetProperty(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    auto* self = WEBKIT_VIDEO_ENCODER(object);
    auto* priv = self->priv;

    switch (propertyId) {
    case PROP_ENCODER:
        g_value_set_object(value, priv->encoder.get());
        break;
    case PROP_BITRATE:
        g_value_set_uint(value, priv->bitrate);
        break;
    case PROP_KEYFRAME_INTERVAL:
        if (priv->encoder) {
            auto encoder = Encoders::definition(priv->encoderId);
            g_object_get_property(G_OBJECT(priv->encoder.get()), encoder->keyframeIntervalPropertyName.characters(), value);
        }
        break;
    case PROP_BITRATE_MODE:
        g_value_set_enum(value, priv->bitrateMode);
        break;
    case PROP_LATENCY_MODE:
        g_value_set_enum(value, priv->latencyMode);
        break;
    case PROP_SCALE_RESOLUTION_DOWN_BY:
        g_value_set_double(value, priv->scaleResolutionDownBy);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void videoEncoderSetBitrate(WebKitVideoEncoder* self, guint bitrate)
{
    auto* priv = self->priv;
    priv->bitrate = bitrate;

    if (priv->encoderId != None) {
        auto encoder = Encoders::definition(priv->encoderId);
        encoder->setBitrate(G_OBJECT(priv->encoder.get()), encoder->bitratePropertyName, priv->bitrate);
    }
}

static bool videoEncoderSetEncoder(WebKitVideoEncoder* self, EncoderId encoderId, GRefPtr<GstCaps>&& inputCaps, GRefPtr<GstCaps>&& encodedCaps)
{
    ASSERT(encoderId != EncoderId::None);

    if (auto structure = gst_caps_get_structure(encodedCaps.get(), 0)) {
        auto width = gstStructureGet<int>(structure, "width"_s);
        if (width && *width > MAX_WIDTH) {
            GST_WARNING_OBJECT(self, "Encoded width (%d) is too high. Maximum allowed: %d.", *width, MAX_WIDTH);
            return false;
        }
        auto height = gstStructureGet<int>(structure, "height"_s);
        if (height && *height > MAX_HEIGHT) {
            GST_WARNING_OBJECT(self, "Encoded height (%d) is too high. Maximum allowed: %d.", *height, MAX_HEIGHT);
            return false;
        }
    }

    auto priv = self->priv;
    auto srcPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), "src"));

    priv->encodedCaps = WTFMove(encodedCaps);

    auto encoderDefinition = Encoders::definition(encoderId);
    ASSERT(encoderDefinition);

    auto bin = GST_BIN_CAST(self);

    priv->encoder = gst_element_factory_create(encoderDefinition->factory.get(), nullptr);
    priv->encoderId = encoderId;
    auto inputCapsFilter = gst_element_factory_make("capsfilter", nullptr);
    g_object_set(inputCapsFilter, "caps", inputCaps.get(), nullptr);
    gst_bin_add_many(bin, priv->encoder.get(), inputCapsFilter, nullptr);

    // Keep videoconvertscale disabled for now due to some performance issues.
    // https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/3815
    auto useVideoConvertScale = StringView::fromLatin1(std::getenv("WEBKIT_GST_USE_VIDEOCONVERT_SCALE"));
    GRefPtr<GstElement> videoConvert, videoScale;
    if (useVideoConvertScale == "1"_s) {
        videoConvert = makeGStreamerElement("videoconvertscale"_s);
        if (!videoConvert)
            return false;

        gst_bin_add(bin, videoConvert.get());
    } else {
        videoScale = makeGStreamerElement("videoscale"_s);
        if (!videoScale)
            return false;

        videoConvert = makeGStreamerElement("videoconvert"_s);
        if (!videoConvert)
            return false;

        gst_bin_add_many(bin, videoScale.get(), videoConvert.get(), nullptr);
    }

    GRefPtr<GstElement> videoFlip;
    if (priv->enableVideoFlip) {
        videoFlip = makeGStreamerElement("autovideoflip"_s);
        if (!videoFlip)
            return false;

        gst_util_set_object_arg(G_OBJECT(videoFlip.get()), "video-direction", "auto");
        gst_bin_add(bin, videoFlip.get());
        gst_element_link(videoFlip.get(), videoConvert.get());
    }

    const auto& element = videoFlip ? videoFlip : videoConvert;
    auto sinkPadTarget = adoptGRef(gst_element_get_static_pad(element.get(), "sink"));
    auto sinkPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), "sink"));
    gst_ghost_pad_set_target(GST_GHOST_PAD(sinkPad.get()), sinkPadTarget.get());

    if (encoderDefinition->parserName) {
        priv->parser = makeGStreamerElement(encoderDefinition->parserName);
        if (!priv->parser)
            return false;

        priv->outputCapsFilter = gst_element_factory_make("capsfilter", nullptr);
        gst_bin_add_many(bin, priv->parser.get(), priv->outputCapsFilter.get(), nullptr);
    }

    encoderDefinition->setupEncoder(self);

    ASSERT(GST_IS_VIDEO_ENCODER(priv->encoder.get()));
    gst_video_encoder_set_qos_enabled(GST_VIDEO_ENCODER_CAST(priv->encoder.get()), TRUE);

    encoderDefinition->setBitrateMode(priv->encoder.get(), priv->bitrateMode);
    encoderDefinition->setLatencyMode(priv->encoder.get(), priv->latencyMode);

    if (useVideoConvertScale) {
        if (!gst_element_link(videoConvert.get(), inputCapsFilter)) {
            GST_WARNING_OBJECT(self, "Failed to link videoconvertscale and input capsfilter");
            return false;
        }
    } else if (!gst_element_link_many(videoConvert.get(), videoScale.get(), inputCapsFilter, nullptr)) {
        GST_WARNING_OBJECT(self, "Failed to link videoconvert, videoscale and input capsfilter");
        return false;
    }

    if (!gst_element_link(inputCapsFilter, priv->encoder.get())) {
        GST_WARNING_OBJECT(self, "Failed to link input capsfilter to encoder");
        return false;
    }

    if (priv->parser && !gst_element_link_many(priv->encoder.get(), priv->outputCapsFilter.get(), priv->parser.get(), nullptr)) {
        GST_WARNING_OBJECT(self, "Failed to link encoder to parser");
        return false;
    }

    auto capsFilter = gst_element_factory_make("capsfilter", nullptr);
    if (encoderDefinition->encodedFormat)
        g_object_set(capsFilter, "caps", encoderDefinition->encodedFormat.get(), nullptr);
    else
        g_object_set(capsFilter, "caps", priv->encodedCaps.get(), nullptr);

    gst_bin_add(bin, capsFilter);

    auto srcPadTarget = adoptGRef(gst_element_get_static_pad(capsFilter, "src"));
    gst_ghost_pad_set_target(GST_GHOST_PAD(srcPad.get()), srcPadTarget.get());

    if (!gst_element_link(priv->parser ? priv->parser.get() : priv->encoder.get(), capsFilter)) {
        GST_WARNING_OBJECT(self, "Failed to link to final capsfilter");
        return false;
    }

    gst_bin_sync_children_states(bin);
    GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(bin, GST_DEBUG_GRAPH_SHOW_ALL, "configured-encoder");
    videoEncoderSetBitrate(self, priv->bitrate);
    return true;
}

EncoderId videoEncoderFindForFormat([[maybe_unused]] WebKitVideoEncoder* self, const GRefPtr<GstCaps>& caps)
{
    if (!caps)
        return None;

    Vector<std::pair<EncoderId, const EncoderDefinition*>> candidates;
    GST_DEBUG_OBJECT(self, "Looking for an encoder matching caps %" GST_PTR_FORMAT, caps.get());
    for (const auto& [id, encoder] : Encoders::singleton()) {
        if (gst_element_factory_can_src_any_caps(encoder.factory.get(), caps.get())) {
            GST_DEBUG_OBJECT(self, "Compatible encoder found: %s", encoder.name.characters());
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

    GST_DEBUG_OBJECT(self, "The highest ranked encoder is %s", candidates[0].second->name.characters());
    return candidates[0].first;
}

bool videoEncoderSupportsCodec(WebKitVideoEncoder* self, const String& codecName)
{
    auto [_, outputCaps] = GStreamerCodecUtilities::capsFromCodecString(codecName, { });
    return videoEncoderFindForFormat(self, outputCaps) != None;
}

bool videoEncoderSetCodec(WebKitVideoEncoder* self, const String& codecName, const IntSize& size, std::optional<double> frameRate, bool enableVideoFlip)
{
    if (self->priv->encoder) {
        GST_ERROR_OBJECT(self, "Encoder already configured");
        return false;
    }

    self->priv->enableVideoFlip = enableVideoFlip;

    auto [inputCaps, outputCaps] = GStreamerCodecUtilities::capsFromCodecString(codecName, size, frameRate);
    GST_DEBUG_OBJECT(self, "Input caps: %" GST_PTR_FORMAT, inputCaps.get());
    GST_DEBUG_OBJECT(self, "Output caps: %" GST_PTR_FORMAT, outputCaps.get());
    auto encoderId = videoEncoderFindForFormat(self, outputCaps);
    if (encoderId == None) {
        GST_ERROR_OBJECT(self, "No encoder found for codec %s", codecName.ascii().data());
        return false;
    }

    return videoEncoderSetEncoder(self, encoderId, WTFMove(inputCaps), WTFMove(outputCaps));
}

void videoEncoderSetBitRateAllocation(WebKitVideoEncoder* self, RefPtr<WebKitVideoEncoderBitRateAllocation>&& allocation)
{
    auto* priv = self->priv;
    priv->bitRateAllocation = WTFMove(allocation);

    if (priv->encoderId != None) {
        auto encoder = Encoders::definition(priv->encoderId);
        encoder->setBitRateAllocation(priv->encoder.get(), *priv->bitRateAllocation);
    }
}

void videoEncoderScaleResolutionDownBy(WebKitVideoEncoder* self, double scaleResolutionDownBy)
{
    self->priv->scaleResolutionDownBy = scaleResolutionDownBy;

    auto pad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT_CAST(self), "sink"));
    if (!pad)
        return;

    auto peer = adoptGRef(gst_pad_get_peer(pad.get()));
    if (!peer)
        return;

    gst_pad_send_event(peer.get(), gst_event_new_reconfigure());
}

static void videoEncoderSetProperty(GObject* object, guint propertyId, const GValue* value, GParamSpec* pspec)
{
    auto* self = WEBKIT_VIDEO_ENCODER(object);
    auto* priv = self->priv;

    switch (propertyId) {
    case PROP_BITRATE:
        videoEncoderSetBitrate(self, g_value_get_uint(value));
        break;
    case PROP_KEYFRAME_INTERVAL:
        if (priv->encoder) {
            auto encoder = Encoders::definition(priv->encoderId);
            g_object_set(priv->encoder.get(), encoder->keyframeIntervalPropertyName.characters(), g_value_get_uint(value), nullptr);
        }
        break;
    case PROP_BITRATE_MODE:
        priv->bitrateMode = static_cast<BitrateMode>(g_value_get_enum(value));
        if (priv->encoder) {
            auto encoder = Encoders::definition(priv->encoderId);
            encoder->setBitrateMode(priv->encoder.get(), priv->bitrateMode);
        }
        break;
    case PROP_LATENCY_MODE:
        priv->latencyMode = static_cast<LatencyMode>(g_value_get_enum(value));
        if (priv->encoder) {
            auto encoder = Encoders::definition(priv->encoderId);
            encoder->setLatencyMode(priv->encoder.get(), priv->latencyMode);
        }
        break;
    case PROP_SCALE_RESOLUTION_DOWN_BY:
        videoEncoderScaleResolutionDownBy(self, g_value_get_double(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void setBitrateKbitPerSec(GObject* encoder, ASCIILiteral propertyName, int bitrate)
{
    GST_INFO_OBJECT(encoder, "Setting bitrate to %d Kbits/sec", bitrate);
    g_object_set(encoder, propertyName.characters(), bitrate, nullptr);
}

static void setBitrateBitPerSec(GObject* encoder, ASCIILiteral propertyName, int bitrate)
{
    GST_INFO_OBJECT(encoder, "Setting bitrate to %d bits/sec", bitrate);
    g_object_set(encoder, propertyName.characters(), bitrate * KBIT_TO_BIT, nullptr);
}

static GRefPtr<GstCaps> createSrcPadTemplateCaps()
{
    auto* caps = gst_caps_new_empty();

    for (auto& [id, encoder] : Encoders::singleton()) {
        if (encoder.encodedFormat)
            caps = gst_caps_merge(caps, encoder.encodedFormat.ref());
        else
            caps = gst_caps_merge(caps, encoder.caps.ref());
    }

    GST_DEBUG("Source pad template caps: %" GST_PTR_FORMAT, caps);
    return caps;
}

static void videoEncoderConstructed(GObject* encoder)
{
    G_OBJECT_CLASS(webkit_video_encoder_parent_class)->constructed(encoder);

    auto* self = WEBKIT_VIDEO_ENCODER(encoder);
    self->priv->encoderId = None;

    self->priv->bitrateMode = CONSTANT_BITRATE_MODE;
    self->priv->latencyMode = REALTIME_LATENCY_MODE;

    auto* sinkPad = webkitGstGhostPadFromStaticTemplate(&encoderSinkTemplate, "sink"_s, nullptr);
    GST_OBJECT_FLAG_SET(sinkPad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_event_function(sinkPad, reinterpret_cast<GstPadEventFunction>(+[](GstPad* pad, GstObject* parent, GstEvent* event) -> gboolean {
        if (GST_EVENT_TYPE(event) == GST_EVENT_CUSTOM_DOWNSTREAM_OOB) {
            const auto* structure = gst_event_get_structure(event);
            if (gst_structure_has_name(structure, "encoder-bitrate-change-request")) {
                auto bitrate = gstStructureGet<unsigned>(structure, "bitrate"_s);
                RELEASE_ASSERT(bitrate);
                g_object_set(parent, "bitrate", static_cast<uint32_t>(*bitrate), nullptr);
                return TRUE;
            }
        }

        if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
            auto self = WEBKIT_VIDEO_ENCODER(parent);
            auto scaleResolutionDownBy = self->priv->scaleResolutionDownBy;
            if (scaleResolutionDownBy > 1.0) {
                GST_DEBUG_OBJECT(self, "Applying scale factor: %f", scaleResolutionDownBy);
                GstCaps* caps;
                gst_event_parse_caps(event, &caps);
                if (caps && gst_caps_get_size(caps)) {
                    auto writableCaps = adoptGRef(gst_caps_copy(caps));
                    auto structure = gst_caps_get_structure(writableCaps.get(), 0);
                    auto width = gstStructureGet<int>(structure, "width"_s);
                    auto height = gstStructureGet<int>(structure, "height"_s);
                    if (width && height) {
                        int newWidth = *width / scaleResolutionDownBy;
                        int newHeight = *height / scaleResolutionDownBy;
                        gst_structure_set(structure, "width", G_TYPE_INT, newWidth, "height", G_TYPE_INT, newHeight, nullptr);
                        GST_DEBUG_OBJECT(self, "Modified caps: %" GST_PTR_FORMAT, writableCaps.get());
                        auto newCapsEvent = adoptGRef(gst_event_new_caps(writableCaps.get()));
                        gst_event_replace(&event, newCapsEvent.get());
                    }
                }
            }
        }
        return gst_pad_event_default(pad, parent, event);
    }));
    gst_element_add_pad(GST_ELEMENT_CAST(self), sinkPad);

    gst_element_add_pad(GST_ELEMENT_CAST(self), webkitGstGhostPadFromStaticTemplate(&encoderSrcTemplate, "src"_s, nullptr));
}

static void setupVaEncoder(WebKitVideoEncoder* self)
{
    g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
}

static void setVaBitrateMode(GstElement* encoder, BitrateMode mode)
{
    switch (mode) {
    case CONSTANT_BITRATE_MODE:
        gst_util_set_object_arg(G_OBJECT(encoder), "rate-control", "cbr");
        break;
    case VARIABLE_BITRATE_MODE:
        gst_util_set_object_arg(G_OBJECT(encoder), "rate-control", "vbr");
        break;
    };
}

static void setVaLatencyMode(GstElement* encoder, LatencyMode mode)
{
    switch (mode) {
    case REALTIME_LATENCY_MODE:
        g_object_set(encoder, "target-usage", 1, nullptr);
        break;
    case QUALITY_LATENCY_MODE:
        g_object_set(encoder, "target-usage", 7, nullptr);
        gst_util_set_object_arg(G_OBJECT(encoder), "rate-control", "cqp");
        break;
    };
}

static void webkit_video_encoder_class_init(WebKitVideoEncoderClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = videoEncoderConstructed;
    objectClass->get_property = videoEncoderGetProperty;
    objectClass->set_property = videoEncoderSetProperty;

    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    gst_element_class_set_static_metadata(elementClass, "WebKit video encoder", "Codec/Encoder/Video", "Encodes video for streaming", "Igalia");
    gst_element_class_add_pad_template(elementClass, gst_static_pad_template_get(&encoderSinkTemplate));

    Encoders::registerEncoder(OmxH264, "omxh264enc"_s, "h264parse"_s, "video/x-h264"_s, "video/x-h264,alignment=au,stream-format=avc,profile=baseline"_s,
        [](WebKitVideoEncoder* self) {
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
        }, "target-bitrate"_s, setBitrateBitPerSec, "interval-intraframes"_s, [](GstElement* encoder, BitrateMode mode) {
            switch (mode) {
            case CONSTANT_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "control-rate", "constant");
                break;
            case VARIABLE_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "control-rate", "variable");
                break;
            };
        }, [](GstElement*, LatencyMode) {
            notImplemented();
        });
    Encoders::registerEncoder(X264, "x264enc"_s, "h264parse"_s, "video/x-h264"_s,
        "video/x-h264,alignment=au,stream-format=avc"_s,
        [](WebKitVideoEncoder* self) {
            g_object_set(self->priv->encoder.get(), "key-int-max", 15, "threads", NUMBER_OF_THREADS, "b-adapt", FALSE, "vbv-buf-capacity", 120, nullptr);
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);

            const auto& encodedCaps = self->priv->encodedCaps;
            if (!gst_caps_is_any(encodedCaps.get()) && !gst_caps_is_empty(encodedCaps.get())) [[likely]] {
                auto structure = gst_caps_get_structure(encodedCaps.get(), 0);
                auto profile = gstStructureGetString(structure, "profile"_s);

                if (profile.findIgnoringASCIICase("high"_s) != notFound)
                    gst_preset_load_preset(GST_PRESET(self->priv->encoder.get()), "Profile High");
                else if (profile.findIgnoringASCIICase("main"_s) != notFound)
                    gst_preset_load_preset(GST_PRESET(self->priv->encoder.get()), "Profile Main");
            }
        }, "bitrate"_s, setBitrateKbitPerSec, "key-int-max"_s, [](GstElement* encoder, BitrateMode mode) {
            switch (mode) {
            case CONSTANT_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "pass", "cbr");
                break;
            case VARIABLE_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "pass", "pass1");
                break;
            };
        }, [](GstElement* encoder, LatencyMode mode) {
            switch (mode) {
            case REALTIME_LATENCY_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "tune", "zerolatency");
                gst_util_set_object_arg(G_OBJECT(encoder), "speed-preset", "ultrafast");
                break;
            case QUALITY_LATENCY_MODE:
                g_object_set(encoder, "tune", 0, nullptr);
                gst_util_set_object_arg(G_OBJECT(encoder), "speed-preset", "None");
                gst_util_set_object_arg(G_OBJECT(encoder), "pass", "qual");
                break;
            };
        });
    Encoders::registerEncoder(OpenH264, "openh264enc"_s, "h264parse"_s, "video/x-h264"_s,
        "video/x-h264,alignment=au,stream-format=avc"_s,
        [](WebKitVideoEncoder* self) {
            g_object_set(self->priv->parser.get(), "config-interval", 1, nullptr);
            g_object_set(self->priv->outputCapsFilter.get(), "caps", self->priv->encodedCaps.get(), nullptr);
        }, "bitrate"_s, setBitrateBitPerSec, "gop-size"_s, [](GstElement*, BitrateMode) {
            notImplemented();
        }, [](GstElement*, LatencyMode) {
            notImplemented();
        });

    auto setVpxEncoderInputFormat = [](auto* self) {
        g_object_set(self->priv->encoder.get(), "buffer-initial-size", 100, "buffer-optimal-size", 120, "buffer-size" , 150, "max-intra-bitrate", 250, nullptr);
        gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "error-resilient", "default");
    };

    Encoders::registerEncoder(Vp8, "vp8enc"_s, nullptr, "video/x-vp8"_s, nullptr,
        [&](WebKitVideoEncoder* self) {
            gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "keyframe-mode", "disabled");
            setVpxEncoderInputFormat(self);
        }, "target-bitrate"_s, setBitrateBitPerSec, "keyframe-max-dist"_s, [](GstElement* encoder, BitrateMode mode) {
            switch (mode) {
            case CONSTANT_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "cbr");
                break;
            case VARIABLE_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "vbr");
                break;
            };
        }, [](GstElement* encoder, LatencyMode mode) {
            switch (mode) {
            case REALTIME_LATENCY_MODE:
                gst_preset_load_preset(GST_PRESET(encoder), "Profile Realtime");
                break;
            case QUALITY_LATENCY_MODE:
                g_object_set(encoder, "threads", NUMBER_OF_THREADS, "cpu-used", NUMBER_OF_THREADS, "deadline", 0, "lag-in-frames", 25, nullptr);
                gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "cq");
                break;
            };
        }, [](GstElement* encoder, const WebKitVideoEncoderBitRateAllocation& bitRateAllocation) {
            // Allow usage of deprecated GValueArray API.
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN;
            GUniquePtr<GValueArray> bitrates(g_value_array_new(3));
            GUniquePtr<GValueArray> layerIds(g_value_array_new(4));
            GUniquePtr<GValueArray> decimators(g_value_array_new(3));
            GValue intValue G_VALUE_INIT;
            GValue boolValue G_VALUE_INIT;
            unsigned numberLayers = 1;
            Vector<bool> layerSyncFlags;
            ASCIILiteral scalabilityString;
            ASCIILiteral layerFlags;

            g_value_init(&intValue, G_TYPE_INT);

            switch (bitRateAllocation.scalabilityMode()) {
            case WebCore::VideoEncoderScalabilityMode::L1T1:
                numberLayers = 1;
                scalabilityString = "L1T1"_s;
                if (auto value = bitRateAllocation.getBitRate(0, 0)) {
                    g_value_set_int(&intValue, *value);
                    g_value_array_append(bitrates.get(), &intValue);
                }
                for (unsigned i = 0; i < 3; i++) {
                    static const std::array<int, 3> decimatorValues = { 1, 1, 1 };
                    g_value_set_int(&intValue, decimatorValues[i]);
                    g_value_array_append(decimators.get(), &intValue);
                }
                break;
            case WebCore::VideoEncoderScalabilityMode::L1T2:
                numberLayers = 2;
                scalabilityString = "L1T2"_s;
                if (auto value = bitRateAllocation.getBitRate(0, 1)) {
                    g_value_set_int(&intValue, *value);
                    g_value_array_append(bitrates.get(), &intValue);
                }
                if (auto value = bitRateAllocation.getBitRate(0, 0)) {
                    g_value_set_int(&intValue, *value);
                    g_value_array_append(bitrates.get(), &intValue);
                }
                for (unsigned i = 0; i < 3; i++) {
                    static const std::array<int, 3> decimatorValues = { 1, 1, 1 };
                    g_value_set_int(&intValue, decimatorValues[i]);
                    g_value_array_append(decimators.get(), &intValue);
                }
                for (unsigned i = 0; i < 4; i++) {
                    static const std::array<int, 4> layerIdValues = { 0, 1, 0, 1 };
                    g_value_set_int(&intValue, layerIdValues[i]);
                    g_value_array_append(layerIds.get(), &intValue);
                }
                g_object_set(encoder, "temporal-scalability-layer-id", layerIds.get(), "temporal-scalability-periodicity", 2, nullptr);
                layerFlags = \
                    /* layer 0 */
                    "<no-ref-golden+no-upd-golden+no-upd-alt,"
                    /* layer 1 (sync) */
                    "no-ref-golden+no-upd-last+no-upd-alt,"
                    /* layer 0 */
                    "no-ref-golden+no-upd-golden+no-upd-alt,"
                    /* layer 1 */
                    "no-upd-last+no-upd-alt>"_s;
                layerSyncFlags = { false, true, false, false };
                break;
            case WebCore::VideoEncoderScalabilityMode::L1T3:
                numberLayers = 3;
                scalabilityString = "L1T3"_s;
                if (auto value = bitRateAllocation.getBitRate(0, 2)) {
                    g_value_set_int(&intValue, *value);
                    g_value_array_append(bitrates.get(), &intValue);
                }
                if (auto value = bitRateAllocation.getBitRate(0, 1)) {
                    g_value_set_int(&intValue, *value);
                    g_value_array_append(bitrates.get(), &intValue);
                }
                if (auto value = bitRateAllocation.getBitRate(0, 0)) {
                    g_value_set_int(&intValue, *value);
                    g_value_array_append(bitrates.get(), &intValue);
                }
                for (unsigned i = 0; i < 3; i++) {
                    static const std::array<int, 3> decimatorValues = { 4, 2, 1 };
                    g_value_set_int(&intValue, decimatorValues[i]);
                    g_value_array_append(decimators.get(), &intValue);
                }
                for (unsigned i = 0; i < 4; i++) {
                    static const std::array<int, 4> layerIdValues = { 0, 2, 1, 2 };
                    g_value_set_int(&intValue, layerIdValues[i]);
                    g_value_array_append(layerIds.get(), &intValue);
                }
                g_object_set(encoder, "temporal-scalability-layer-id", layerIds.get(), "temporal-scalability-periodicity", 4, nullptr);

                layerFlags = \
                    /* layer 0 */
                    "<no-ref-golden+no-upd-golden+no-upd-alt,"
                    /* layer 2 (sync) */
                    "no-ref-golden+no-upd-last+no-upd-golden+no-upd-alt+no-upd-entropy,"
                    /* layer 1 (sync) */
                    "no-ref-golden+no-upd-last+no-upd-alt,"
                    /* layer 2 */
                    "no-upd-last+no-upd-golden+no-upd-alt+no-upd-entropy,"
                    /* layer 0 */
                    "no-ref-golden+no-upd-golden+no-upd-alt,"
                    /* layer 2 */
                    "no-upd-last+no-upd-golden+no-upd-alt+no-upd-entropy,"
                    /* layer 1 */
                    "no-upd-last+no-upd-alt,"
                    /* layer 2 */
                    "no-upd-last+no-upd-golden+no-upd-alt+no-upd-entropy>"_s;
                layerSyncFlags = { false, true, true, false, false, false, false, false };
                break;
            }
            g_value_unset(&intValue);

            GST_DEBUG_OBJECT(encoder, "Configuring for %s scalability mode", scalabilityString.characters());
            g_object_set(encoder, "temporal-scalability-number-layers", numberLayers,
                "temporal-scalability-rate-decimator", decimators.get(),
                "temporal-scalability-target-bitrate", bitrates.get(), nullptr);

            if (layerFlags) {
                GValue layerSyncFlagsValue G_VALUE_INIT;

                g_value_init(&boolValue, G_TYPE_BOOLEAN);
                gst_value_array_init(&layerSyncFlagsValue, layerSyncFlags.size());
                for (auto& flag : layerSyncFlags) {
                    g_value_set_boolean(&boolValue, flag);
                    gst_value_array_append_value(&layerSyncFlagsValue, &boolValue);
                }

                g_object_set_property(G_OBJECT(encoder), "temporal-scalability-layer-sync-flags", &layerSyncFlagsValue);
                g_value_unset(&layerSyncFlagsValue);
                g_value_unset(&boolValue);
                gst_util_set_object_arg(G_OBJECT(encoder), "temporal-scalability-layer-flags", layerFlags.characters());
            }

            ALLOW_DEPRECATED_DECLARATIONS_END;
        });

    Encoders::registerEncoder(Vp9, "vp9enc"_s, nullptr, "video/x-vp9"_s, nullptr,
        [&](WebKitVideoEncoder* self) {
            setVpxEncoderInputFormat(self);
        }, "target-bitrate"_s, setBitrateBitPerSec, "keyframe-max-dist"_s, [](GstElement* encoder, BitrateMode mode) {
            switch (mode) {
            case CONSTANT_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "cbr");
                break;
            case VARIABLE_BITRATE_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "vbr");
                break;
            };
        }, [](GstElement* encoder, LatencyMode mode) {
            switch (mode) {
            case REALTIME_LATENCY_MODE: {
                int64_t deadline = 1000000;
                g_object_set(encoder, "threads", NUMBER_OF_THREADS, "cpu-used", NUMBER_OF_THREADS, "deadline", deadline, "lag-in-frames", 0, nullptr);
                break;
            }
            case QUALITY_LATENCY_MODE:
                g_object_set(encoder, "threads", NUMBER_OF_THREADS, "cpu-used", NUMBER_OF_THREADS, "deadline", 0, "lag-in-frames", 25, nullptr);
                gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "cq");
                break;
            };
        });

    Encoders::registerEncoder(VaapiH264LP, "vah264lpenc"_s, "h264parse"_s, "video/x-h264"_s, nullptr, setupVaEncoder,
        "bitrate"_s, setBitrateKbitPerSec, "key-int-max"_s, [](GstElement*, BitrateMode) {
            // Not supported.
        }, setVaLatencyMode);

    Encoders::registerEncoder(VaapiH264, "vah264enc"_s, "h264parse"_s, "video/x-h264"_s, nullptr,
        setupVaEncoder, "bitrate"_s, setBitrateKbitPerSec, "key-int-max"_s, setVaBitrateMode, setVaLatencyMode);

    Encoders::registerEncoder(VaapiH265, "vah265enc"_s, "h265parse"_s, "video/x-h265"_s, nullptr,
        setupVaEncoder, "bitrate"_s, setBitrateKbitPerSec, "key-int-max"_s, setVaBitrateMode, setVaLatencyMode);

    Encoders::registerEncoder(VaapiAv1, "vaav1enc"_s, "av1parse"_s, "video/x-av1"_s, nullptr,
        [](auto) { }, "bitrate"_s, setBitrateKbitPerSec, "key-int-max"_s, setVaBitrateMode, setVaLatencyMode);

    Encoders::registerEncoder(SvtAv1, "svtav1enc"_s, "av1parse"_s, "video/x-av1"_s, nullptr, [](auto self) {
        g_object_set(self->priv->encoder.get(), "logical-processors", NUMBER_OF_THREADS, nullptr);
    }, "target-bitrate"_s, setBitrateKbitPerSec, "intra-period-length"_s, [](GstElement*, BitrateMode) {
    }, [](GstElement* encoder, LatencyMode mode) {
        switch (mode) {
        case REALTIME_LATENCY_MODE:
            g_object_set(encoder, "preset", 10, nullptr);
            break;
        case QUALITY_LATENCY_MODE:
            g_object_set(encoder, "preset", 0, nullptr);
            break;
        }
    });

    if (webkitGstCheckVersion(1, 22, 0)) {
        Encoders::registerEncoder(Av1, "av1enc"_s, "av1parse"_s, "video/x-av1"_s, nullptr,
            [](WebKitVideoEncoder* self) {
                g_object_set(self->priv->encoder.get(), "threads", NUMBER_OF_THREADS, nullptr);
                gst_util_set_object_arg(G_OBJECT(self->priv->encoder.get()), "keyframe-mode", "disabled");
            }, "target-bitrate"_s, setBitrateKbitPerSec, "keyframe-max-dist"_s, [](GstElement* encoder, BitrateMode mode) {
                switch (mode) {
                case CONSTANT_BITRATE_MODE:
                    gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "cbr");
                    break;
                case VARIABLE_BITRATE_MODE:
                    gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "vbr");
                    break;
                }
            }, [](GstElement* encoder, LatencyMode mode) {
                if (!gstObjectHasProperty(encoder, "usage-profile"_s))
                    return;
                switch (mode) {
                case REALTIME_LATENCY_MODE:
                    gst_util_set_object_arg(G_OBJECT(encoder), "usage-profile", "realtime");
                    break;
                case QUALITY_LATENCY_MODE:
                    gst_util_set_object_arg(G_OBJECT(encoder), "usage-profile", "good-quality");
                    gst_util_set_object_arg(G_OBJECT(encoder), "end-usage", "q");
                    break;
                }
            });
    }

    static GQuark x265BitrateQuark = g_quark_from_static_string("x265-bitrate-mode");
    Encoders::registerEncoder(X265, "x265enc"_s, "h265parse"_s, "video/x-h265"_s,
        "video/x-h265,alignment=au,stream-format=byte-stream"_s,
        [](WebKitVideoEncoder* self) {
            g_object_set(self->priv->encoder.get(), "key-int-max", 15, nullptr);
        }, "bitrate"_s, [](GObject* object, ASCIILiteral propertyName, int bitrate) {
            if (!bitrate) [[unlikely]]
                return;
            setBitrateKbitPerSec(object, propertyName, bitrate);
            auto bitrateMode = GPOINTER_TO_INT(g_object_get_qdata(object, x265BitrateQuark));
            String options;
            switch (bitrateMode) {
            case CONSTANT_BITRATE_MODE:
                options = makeString("vbv-maxrate="_s, bitrate, ":vbv-bufsize="_s, bitrate / 2);
                break;
            case VARIABLE_BITRATE_MODE:
                options = "vbv-maxrate=0:vbv-bufsize=0"_s;
                break;
            };
            g_object_set(object, "option-string", options.ascii().data(), nullptr);
        }, "key-int-max"_s, [](GstElement* encoder, BitrateMode mode) {
            g_object_set_qdata(G_OBJECT(encoder), x265BitrateQuark, GINT_TO_POINTER(mode));
        }, [](GstElement* encoder, LatencyMode mode) {
            switch (mode) {
            case REALTIME_LATENCY_MODE:
                gst_util_set_object_arg(G_OBJECT(encoder), "tune", "zerolatency");
                gst_util_set_object_arg(G_OBJECT(encoder), "speed-preset", "ultrafast");
                break;
            case QUALITY_LATENCY_MODE:
                g_object_set(encoder, "tune", 0, nullptr);
                gst_util_set_object_arg(G_OBJECT(encoder), "speed-preset", "No preset");
                break;
            };
        });

    auto srcPadTemplateCaps = createSrcPadTemplateCaps();
    gst_element_class_add_pad_template(elementClass, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, srcPadTemplateCaps.get()));

    g_object_class_install_property(objectClass, PROP_ENCODER, g_param_spec_object("encoder", nullptr, nullptr, GST_TYPE_ELEMENT, WEBKIT_PARAM_READABLE));

    g_object_class_install_property(objectClass, PROP_BITRATE, g_param_spec_uint("bitrate", nullptr, nullptr, 0, G_MAXINT, 2048,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));

    g_object_class_install_property(objectClass, PROP_KEYFRAME_INTERVAL, g_param_spec_uint("keyframe-interval", nullptr, nullptr, 0, G_MAXINT, 0,
        static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));
    g_object_class_install_property(objectClass, PROP_BITRATE_MODE, g_param_spec_enum("bitrate-mode",
        nullptr, nullptr, VIDEO_ENCODER_TYPE_BITRATE_MODE, CONSTANT_BITRATE_MODE, WEBKIT_PARAM_READWRITE));
    g_object_class_install_property(objectClass, PROP_LATENCY_MODE, g_param_spec_enum("latency-mode",
        nullptr, nullptr, VIDEO_ENCODER_TYPE_LATENCY_MODE, REALTIME_LATENCY_MODE, WEBKIT_PARAM_READWRITE));
    g_object_class_install_property(objectClass, PROP_SCALE_RESOLUTION_DOWN_BY, g_param_spec_double("scale-resolution-down-by",
        nullptr, nullptr, 0, G_MAXDOUBLE, 1, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT)));
}

#undef NUMBER_OF_THREADS
#undef GST_CAT_DEFAULT

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
