/*
 * Copyright (C) 2023 Igalia S.L
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
#include "VideoEncoderGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GStreamerElementHarness.h"
#include "GStreamerRegistryScanner.h"
#include "GUniquePtrGStreamer.h"
#include "VideoEncoderPrivateGStreamer.h"
#include "VideoFrameGStreamer.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/MakeString.h>

#if USE(GSTREAMER_GL)
#include <gst/gl/gstglmemory.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GStreamerVideoEncoder);

GST_DEBUG_CATEGORY(webkit_video_encoder_debug);
#define GST_CAT_DEFAULT webkit_video_encoder_debug

static WorkQueue& gstEncoderWorkQueue()
{
    static std::once_flag onceKey;
    static LazyNeverDestroyed<Ref<WorkQueue>> queue;
    std::call_once(onceKey, [] {
        queue.construct(WorkQueue::create("GStreamer VideoEncoder queue"_s));
    });
    return queue.get();
}

class GStreamerInternalVideoEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GStreamerInternalVideoEncoder, WTF::DestructionThread::Main> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GStreamerInternalVideoEncoder);
    WTF_MAKE_NONCOPYABLE(GStreamerInternalVideoEncoder);

public:
    static Ref<GStreamerInternalVideoEncoder> create(const VideoEncoder::Config& config, VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback) { return adoptRef(*new GStreamerInternalVideoEncoder(config, WTF::move(descriptionCallback), WTF::move(outputCallback))); }
    ~GStreamerInternalVideoEncoder();

    String initialize(const String& codecName);
    bool encode(VideoEncoder::RawFrame&&, bool shouldGenerateKeyFrame);
    void setRates(uint64_t bitRate, double frameRate);
    void setBitRateAllocation(RefPtr<WebKitVideoEncoderBitRateAllocation>&&, double frameRate);
    void applyRates();
    void flush();
    void close() { m_isClosed = true; }

    const RefPtr<GStreamerElementHarness> harness() const { return m_harness; }
    bool isClosed() const { return m_isClosed; }

private:
    GStreamerInternalVideoEncoder(const VideoEncoder::Config&, VideoEncoder::DescriptionCallback&&, VideoEncoder::OutputCallback&&);

    VideoEncoder::Config m_config;
    VideoEncoder::DescriptionCallback m_descriptionCallback;
    VideoEncoder::OutputCallback m_outputCallback;
    RefPtr<WebKitVideoEncoderBitRateAllocation> m_bitrateAllocation;
    int64_t m_timestamp { 0 };
    std::optional<uint64_t> m_duration;
    bool m_isClosed { false };
    bool m_isInitialized { false };
    RefPtr<GStreamerElementHarness> m_harness;
    GUniquePtr<GstVideoConverter> m_colorConvert;
    GRefPtr<GstCaps> m_colorConvertInputCaps;
    GRefPtr<GstCaps> m_colorConvertOutputCaps;
    bool m_hasMultipleTemporalLayers { false };
    String m_orientation;
};

void GStreamerVideoEncoder::create(const String& codecName, const VideoEncoder::Config& config, CreateCallback&& callback, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback)
{
    auto createResult = create(codecName, config, WTF::move(descriptionCallback), WTF::move(outputCallback));
    if (!createResult) {
        callback(makeUnexpected(WTF::move(createResult.error())));
        return;
    }

    gstEncoderWorkQueue().dispatch([callback = WTF::move(callback), encoder = WTF::move(*createResult)]() mutable {
        GST_DEBUG("Encoder created");
        callback(Ref<VideoEncoder> { WTF::move(encoder) });
    });
}

Expected<Ref<GStreamerVideoEncoder>, String> GStreamerVideoEncoder::create(const String& codecName, const VideoEncoder::Config& config, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback)
{
    static std::once_flag debugRegisteredFlag;
    std::call_once(debugRegisteredFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_video_encoder_debug, "webkitvideoencoder", 0, "WebKit WebCodecs Video Encoder");
    });
    registerWebKitGStreamerVideoEncoder();
    auto& scanner = GStreamerRegistryScanner::singleton();
    if (!scanner.isCodecSupported(GStreamerRegistryScanner::Configuration::Encoding, codecName))
        return makeUnexpected(makeString("No GStreamer encoder found for codec "_s, codecName));
    auto encoder = adoptRef(*new GStreamerVideoEncoder(config, WTF::move(descriptionCallback), WTF::move(outputCallback)));

    Ref internalEncoder = encoder->m_internalEncoder;
    auto error = internalEncoder->initialize(codecName);
    if (!error.isEmpty()) {
        GST_WARNING("Error creating encoder: %s", error.ascii().data());
        return makeUnexpected(makeString("GStreamer encoding initialization failed with error: "_s, error));
    }

    return encoder;
}

GStreamerVideoEncoder::GStreamerVideoEncoder(const VideoEncoder::Config& config, DescriptionCallback&& descriptionCallback, OutputCallback&& outputCallback)
    : m_internalEncoder(GStreamerInternalVideoEncoder::create(config, WTF::move(descriptionCallback), WTF::move(outputCallback)))
{
}

GStreamerVideoEncoder::~GStreamerVideoEncoder()
{
    GST_DEBUG_OBJECT(m_internalEncoder->harness()->element(), "Destroying");
    close();
}

Ref<VideoEncoder::EncodePromise> GStreamerVideoEncoder::encode(RawFrame&& frame, bool shouldGenerateKeyFrame)
{
    return invokeAsync(gstEncoderWorkQueue(), [frame = WTF::move(frame), shouldGenerateKeyFrame, encoder = m_internalEncoder]() mutable {
        auto result = encoder->encode(WTF::move(frame), shouldGenerateKeyFrame);
        if (!result)
            return EncodePromise::createAndReject("Encoding failed"_s);

        encoder->harness()->processOutputSamples();
        return EncodePromise::createAndResolve();
    });
}

Ref<GenericPromise> GStreamerVideoEncoder::flush()
{
    return invokeAsync(gstEncoderWorkQueue(), [encoder = m_internalEncoder] {
        encoder->flush();
        return GenericPromise::createAndResolve();
    });
}

Ref<GenericPromise> GStreamerVideoEncoder::setRates(uint64_t bitRate, double frameRate)
{
    return invokeAsync(gstEncoderWorkQueue(), [encoder = m_internalEncoder, bitRate, frameRate] {
        encoder->setRates(bitRate, frameRate);
        return GenericPromise::createAndResolve();
    });
}

Ref<GenericPromise> GStreamerVideoEncoder::setBitRateAllocation(RefPtr<WebKitVideoEncoderBitRateAllocation>&& allocation, double frameRate)
{
    return invokeAsync(gstEncoderWorkQueue(), [encoder = m_internalEncoder, bitrateAllocation = WTF::move(allocation), frameRate] mutable {
        encoder->setBitRateAllocation(WTF::move(bitrateAllocation), frameRate);
        return GenericPromise::createAndResolve();
    });
}

void GStreamerVideoEncoder::reset()
{
    GST_DEBUG_OBJECT(m_internalEncoder->harness()->element(), "Resetting");
    m_internalEncoder->close();
}

void GStreamerVideoEncoder::close()
{
    GST_DEBUG_OBJECT(m_internalEncoder->harness()->element(), "Closing");
    m_internalEncoder->close();
}

static std::optional<unsigned> retrieveTemporalIndex(const GRefPtr<GstSample>& sample)
{
#if GST_CHECK_VERSION(1, 20, 0)
    auto caps = gst_sample_get_caps(sample.get());
    auto structure = gst_caps_get_structure(caps, 0);
    auto buffer = gst_sample_get_buffer(sample.get());
    if (gst_structure_has_name(structure, "video/x-vp8")) {
        auto meta = gst_buffer_get_custom_meta(buffer, "GstVP8Meta");
        if (!meta) {
            GST_TRACE("VP8Meta not found in VP8 sample");
            return { };
        }

        auto metaStructure = gst_custom_meta_get_structure(meta);
        RELEASE_ASSERT(metaStructure);
        GST_TRACE("Looking-up layer id in %" GST_PTR_FORMAT, metaStructure);
        return gstStructureGet<unsigned>(metaStructure, "layer-id"_s);
    }
#ifndef GST_DISABLE_GST_DEBUG
    auto name = gstStructureGetName(structure);
    GST_TRACE("Retrieval of temporal index from encoded format %s is not yet supported.", name.utf8());
#endif
#endif
    return { };
}

GStreamerInternalVideoEncoder::GStreamerInternalVideoEncoder(const VideoEncoder::Config& config, VideoEncoder::DescriptionCallback&& descriptionCallback, VideoEncoder::OutputCallback&& outputCallback)
    : m_config(config)
    , m_descriptionCallback(WTF::move(descriptionCallback))
    , m_outputCallback(WTF::move(outputCallback))
{
    GRefPtr<GstElement> element = gst_element_factory_make("webkitvideoencoder", nullptr);

    auto pad = adoptGRef(gst_element_get_static_pad(element.get(), "src"));
    g_signal_connect_data(pad.get(), "notify::caps", G_CALLBACK(+[](GObject* pad, GParamSpec*, gpointer userData) {
        auto weakEncoder = static_cast<ThreadSafeWeakPtr<GStreamerInternalVideoEncoder>*>(userData);
        auto encoder = weakEncoder->get();
        if (!encoder)
            return;

        GRefPtr<GstCaps> caps;
        g_object_get(pad, "caps", &caps.outPtr(), nullptr);
        if (!caps)
            return;

        VideoEncoder::ActiveConfiguration configuration;
        configuration.colorSpace = videoColorSpaceFromCaps(caps.get());

        auto structure = gst_caps_get_structure(caps.get(), 0);
        GstBuffer* header = nullptr;
        if (auto streamHeader = gst_structure_get_value(structure, "streamheader")) {
            RELEASE_ASSERT(GST_VALUE_HOLDS_ARRAY(streamHeader));
            auto firstValue = gst_value_array_get_value(streamHeader, 0);
            RELEASE_ASSERT(GST_VALUE_HOLDS_BUFFER(firstValue));
            header = gst_value_get_buffer(firstValue);
        } else if (auto codecData = gst_structure_get_value(structure, "codec_data")) {
            RELEASE_ASSERT(GST_VALUE_HOLDS_BUFFER(codecData));
            header = gst_value_get_buffer(codecData);
        }

        if (header) {
            GstMappedBuffer buffer(header, GST_MAP_READ);
            configuration.description = buffer.createVector();
        }
        encoder->m_descriptionCallback(WTF::move(configuration));
    }), new ThreadSafeWeakPtr { *this }, [](void* data, GClosure*) {
        delete static_cast<ThreadSafeWeakPtr<GStreamerInternalVideoEncoder>*>(data);
    }, static_cast<GConnectFlags>(0));

    m_harness = GStreamerElementHarness::create(WTF::move(element), [weakThis = ThreadSafeWeakPtr { *this }, this](auto&, GRefPtr<GstSample>&& outputSample) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (m_isClosed)
            return;

        static std::once_flag onceFlag;
        std::call_once(onceFlag, [this] {
            m_harness->dumpGraph("video-encoder"_s);
        });

        std::optional<unsigned> temporalIndex;
        if (m_hasMultipleTemporalLayers)
            temporalIndex = retrieveTemporalIndex(outputSample);

        auto outputBuffer = gst_sample_get_buffer(outputSample.get());
        bool isKeyFrame = !GST_BUFFER_FLAG_IS_SET(outputBuffer, GST_BUFFER_FLAG_DELTA_UNIT);
        GST_TRACE_OBJECT(m_harness->element(), "Notifying encoded%s frame", isKeyFrame ? " key" : "");
        GstMappedBuffer encodedImage(outputBuffer, GST_MAP_READ);

        VideoEncoder::EncodedFrame encodedFrame { encodedImage.createVector(), isKeyFrame, m_timestamp, m_duration, temporalIndex };
        m_outputCallback({ WTF::move(encodedFrame) });
    });
}

GStreamerInternalVideoEncoder::~GStreamerInternalVideoEncoder()
{
    if (!m_harness)
        return;

    auto pad = adoptGRef(gst_element_get_static_pad(m_harness->element(), "src"));
    g_signal_handlers_disconnect_by_data(pad.get(), this);
}

String GStreamerInternalVideoEncoder::initialize(const String& codecName)
{
    GST_DEBUG_OBJECT(m_harness->element(), "Initializing encoder for codec %s", codecName.ascii().data());
    IntSize size { static_cast<int>(m_config.width), static_cast<int>(m_config.height) };
    if (!videoEncoderSetCodec(WEBKIT_VIDEO_ENCODER(m_harness->element()), { codecName, m_config.useAnnexB }, size, { }, true))
        return "Unable to set encoder format"_s;

    applyRates();

    m_isInitialized = true;
    return emptyString();
}

bool GStreamerInternalVideoEncoder::encode(VideoEncoder::RawFrame&& rawFrame, bool shouldGenerateKeyFrame)
{
    if (!m_isInitialized) {
        GST_WARNING_OBJECT(m_harness->element(), "Encoder not initialized");
        return true;
    }

    m_timestamp = rawFrame.timestamp;
    m_duration = rawFrame.duration;

    if (shouldGenerateKeyFrame) {
        GST_INFO_OBJECT(m_harness->element(), "Requesting key-frame!");
        m_harness->pushEvent(gst_video_event_new_downstream_force_key_unit(GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, FALSE, 1));
    }

    auto& gstVideoFrame = downcast<VideoFrameGStreamer>(rawFrame.frame.get());
    GRefPtr sample = gstVideoFrame.sample();

    auto orientation = makeString(gstVideoFrame.isMirrored() ? "flip-"_s : ""_s, "rotate-"_s, gstVideoFrame.rotation());
    if (orientation != m_orientation) {
        auto orientationCString = orientation.utf8();
        GST_DEBUG_OBJECT(m_harness->element(), "New orientation: %s", orientationCString.data());
        auto tags = adoptGRef(gst_tag_list_new(GST_TAG_IMAGE_ORIENTATION, orientationCString.data(), nullptr));
        auto event = adoptGRef(gst_event_new_tag(tags.leakRef()));
        m_harness->storeStickyEvent(event);
        m_orientation = WTF::move(orientation);
    }

    if (m_config.frameRate) {
        int framerateNumerator, framerateDenominator;
        gst_util_double_to_fraction(m_config.frameRate, &framerateNumerator, &framerateDenominator);

        GRefPtr caps = gst_sample_get_caps(sample.get());
        caps = adoptGRef(gst_caps_make_writable(caps.leakRef()));
        sample = adoptGRef(gst_sample_make_writable(sample.leakRef()));
        gst_caps_set_simple(caps.get(), "framerate", GST_TYPE_FRACTION, framerateNumerator, framerateDenominator, nullptr);
        gst_sample_set_caps(sample.get(), caps.get());
    }
    return m_harness->pushSample(WTF::move(sample));
}

void GStreamerInternalVideoEncoder::setRates(uint64_t bitRate, double frameRate)
{
    m_config.bitRate = bitRate;
    m_config.frameRate = frameRate;
    applyRates();
}

void GStreamerInternalVideoEncoder::setBitRateAllocation(RefPtr<WebKitVideoEncoderBitRateAllocation>&& allocation, double frameRate)
{
    auto encoder = WEBKIT_VIDEO_ENCODER(m_harness->element());
    if (frameRate)
        videoEncoderSetFrameRate(encoder, frameRate);
    if (m_config.bitRate > 1000)
        g_object_set(m_harness->element(), "bitrate", static_cast<uint32_t>(m_config.bitRate / 1000), nullptr);

    m_hasMultipleTemporalLayers = !!allocation->getBitRate(0, 1);
    videoEncoderSetBitRateAllocation(encoder, WTF::move(allocation));
}

void GStreamerInternalVideoEncoder::applyRates()
{
    auto bitRateAllocation = WebKitVideoEncoderBitRateAllocation::create(m_config.scalabilityMode);
    auto totalBitRate = m_config.bitRate ? m_config.bitRate : 3 * m_config.width * m_config.height;
    switch (m_config.scalabilityMode) {
    case VideoEncoder::ScalabilityMode::L1T1:
        bitRateAllocation->setBitRate(0, 0, totalBitRate);
        break;
    case VideoEncoder::ScalabilityMode::L1T2:
        bitRateAllocation->setBitRate(0, 0, totalBitRate * 0.6);
        bitRateAllocation->setBitRate(0, 1, totalBitRate * 0.4);
        break;
    case VideoEncoder::ScalabilityMode::L1T3:
        bitRateAllocation->setBitRate(0, 0, totalBitRate * 0.5);
        bitRateAllocation->setBitRate(0, 1, totalBitRate * 0.3);
        bitRateAllocation->setBitRate(0, 2, totalBitRate * 0.2);
        break;
    }
    setBitRateAllocation(WTF::move(bitRateAllocation), m_config.frameRate);
}

void GStreamerInternalVideoEncoder::flush()
{
    m_harness->flush();
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
