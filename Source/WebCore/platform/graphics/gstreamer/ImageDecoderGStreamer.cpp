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
#include "ImageDecoderGStreamer.h"

#if USE(GSTREAMER) && ENABLE(VIDEO)

#include "FloatSize.h"
#include "GStreamerCommon.h"
#include "GStreamerRegistryScanner.h"
#include "ImageGStreamer.h"
#include "MediaSampleGStreamer.h"
#include "NotImplemented.h"
#include <gst/app/gstappsink.h>
#include <wtf/MainThread.h>
#include <wtf/Optional.h>
#include <wtf/Threading.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_image_decoder_debug);
#define GST_CAT_DEFAULT webkit_image_decoder_debug

class ImageDecoderGStreamerSample final : public MediaSampleGStreamer {
public:
    static Ref<ImageDecoderGStreamerSample> create(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize)
    {
        return adoptRef(*new ImageDecoderGStreamerSample(WTFMove(sample), presentationSize));
    }

    NativeImagePtr image() const
    {
        if (!m_image)
            return nullptr;
        return m_image->image().nativeImage();
    }
    void dropImage() { m_image = nullptr; }

    SampleFlags flags() const override
    {
        return (SampleFlags)(MediaSampleGStreamer::flags() | (m_image && m_image->hasAlpha() ? HasAlpha : 0));
    }

private:
    ImageDecoderGStreamerSample(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize)
        : MediaSampleGStreamer(WTFMove(sample), presentationSize, { })
    {
        m_image = ImageGStreamer::createImage(platformSample().sample.gstSample);
    }

    RefPtr<ImageGStreamer> m_image;
};

static ImageDecoderGStreamerSample* toSample(const PresentationOrderSampleMap::value_type& pair)
{
    return (ImageDecoderGStreamerSample*)pair.second.get();
}

template <typename Iterator>
ImageDecoderGStreamerSample* toSample(Iterator iter)
{
    return (ImageDecoderGStreamerSample*)iter->second.get();
}

RefPtr<ImageDecoderGStreamer> ImageDecoderGStreamer::create(SharedBuffer& data, const String& mimeType, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    return adoptRef(*new ImageDecoderGStreamer(data, mimeType, alphaOption, gammaAndColorProfileOption));
}

ImageDecoderGStreamer::ImageDecoderGStreamer(SharedBuffer& data, const String& mimeType, AlphaOption, GammaAndColorProfileOption)
    : m_mimeType(mimeType)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_image_decoder_debug, "webkitimagedecoder", 0, "WebKit image decoder");
    });

    pushEncodedData(data);
}

bool ImageDecoderGStreamer::supportsContainerType(const String& type)
{
    return GStreamerRegistryScanner::singleton().isContainerTypeSupported(type);
}

bool ImageDecoderGStreamer::canDecodeType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;

    return GStreamerRegistryScanner::singleton().isContainerTypeSupported(mimeType);
}

EncodedDataStatus ImageDecoderGStreamer::encodedDataStatus() const
{
    if (m_eos)
        return EncodedDataStatus::Complete;
    if (m_size)
        return EncodedDataStatus::SizeAvailable;
    if (m_innerDecoder)
        return m_innerDecoder->encodedDataStatus();
    return EncodedDataStatus::Unknown;
}

IntSize ImageDecoderGStreamer::size() const
{
    if (m_size)
        return m_size.value();
    return { };
}

RepetitionCount ImageDecoderGStreamer::repetitionCount() const
{
    // In the absence of instructions to the contrary, assume all media formats repeat infinitely.
    return frameCount() > 1 ? RepetitionCountInfinite : RepetitionCountNone;
}

String ImageDecoderGStreamer::uti() const
{
    notImplemented();
    return { };
}

ImageOrientation ImageDecoderGStreamer::frameOrientationAtIndex(size_t) const
{
    notImplemented();
    return ImageOrientation::None;
}

Seconds ImageDecoderGStreamer::frameDurationAtIndex(size_t index) const
{
    auto* sampleData = sampleAtIndex(index);
    if (!sampleData)
        return { };

    return Seconds(sampleData->duration().toDouble());
}

bool ImageDecoderGStreamer::frameHasAlphaAtIndex(size_t index) const
{
    auto* sampleData = sampleAtIndex(index);
    return sampleData ? sampleData->hasAlpha() : false;
}

unsigned ImageDecoderGStreamer::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    if (!frameIsCompleteAtIndex(index))
        return 0;

    IntSize frameSize = frameSizeAtIndex(index, subsamplingLevel);
    return (frameSize.area() * 4).unsafeGet();
}

NativeImagePtr ImageDecoderGStreamer::createFrameImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&)
{
    LockHolder holder { m_sampleGeneratorLock };

    auto* sampleData = sampleAtIndex(index);
    if (!sampleData)
        return nullptr;

    if (auto image = sampleData->image())
        return image;

    return nullptr;
}

void ImageDecoderGStreamer::setData(SharedBuffer& data, bool)
{
    pushEncodedData(data);
}

void ImageDecoderGStreamer::clearFrameBufferCache(size_t index)
{
    size_t i = 0;
    for (auto& samplePair : m_sampleData.presentationOrder()) {
        toSample(samplePair)->dropImage();
        if (++i > index)
            break;
    }
}

const ImageDecoderGStreamerSample* ImageDecoderGStreamer::sampleAtIndex(size_t index) const
{
    if (index >= m_sampleData.presentationOrder().size())
        return nullptr;

    // FIXME: std::map is not random-accessible; this can get expensive if callers repeatedly call
    // with monotonically increasing indexes. Investigate adding an O(1) side structure to make this
    // style of access faster.
    auto iter = m_sampleData.presentationOrder().begin();
    for (size_t i = 0; i != index; ++i)
        ++iter;

    return toSample(iter);
}

void ImageDecoderGStreamer::InnerDecoder::decodebinPadAddedCallback(ImageDecoderGStreamer::InnerDecoder* decoder, GstPad* pad)
{
    decoder->connectDecoderPad(pad);
}

void ImageDecoderGStreamer::InnerDecoder::connectDecoderPad(GstPad* pad)
{
    auto padCaps = adoptGRef(gst_pad_get_current_caps(pad));
    GST_DEBUG_OBJECT(m_pipeline.get(), "New decodebin pad %" GST_PTR_FORMAT " caps: %" GST_PTR_FORMAT, pad, padCaps.get());
    RELEASE_ASSERT(doCapsHaveType(padCaps.get(), "video"));

    GstElement* sink = gst_element_factory_make("appsink", nullptr);
    static GstAppSinkCallbacks callbacks = {
        nullptr,
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto sample = adoptGRef(gst_app_sink_try_pull_preroll(sink, 0));
            static_cast<ImageDecoderGStreamer*>(userData)->handleSample(WTFMove(sample));
            return GST_FLOW_OK;
        },
        [](GstAppSink* sink, gpointer userData) -> GstFlowReturn {
            auto sample = adoptGRef(gst_app_sink_try_pull_sample(sink, 0));
            static_cast<ImageDecoderGStreamer*>(userData)->handleSample(WTFMove(sample));
            return GST_FLOW_OK;
        },
        { nullptr }
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(sink), &callbacks, &m_decoder, nullptr);

    GRefPtr<GstCaps> caps = adoptGRef(gst_caps_from_string("video/x-raw, format=(string)RGBA"));
    g_object_set(sink, "sync", false, "caps", caps.get(), nullptr);

    GstElement* videoconvert = gst_element_factory_make("videoconvert", nullptr);

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), videoconvert, sink, nullptr);
    gst_element_link(videoconvert, sink);
    auto sinkPad = adoptGRef(gst_element_get_static_pad(videoconvert, "sink"));
    gst_pad_link(pad, sinkPad.get());
    gst_element_sync_state_with_parent(videoconvert);
    gst_element_sync_state_with_parent(sink);
}

void ImageDecoderGStreamer::handleSample(GRefPtr<GstSample>&& sample)
{
    auto* caps = gst_sample_get_caps(sample.get());
    GST_DEBUG("Handling sample with caps %" GST_PTR_FORMAT, caps);
    auto presentationSize = getVideoResolutionFromCaps(caps);
    if (presentationSize && !presentationSize->isEmpty() && (!m_size || m_size != roundedIntSize(*presentationSize)))
        m_size = roundedIntSize(*presentationSize);
    m_sampleData.addSample(ImageDecoderGStreamerSample::create(WTFMove(sample), *m_size));
}

void ImageDecoderGStreamer::InnerDecoder::handleMessage(GstMessage* message)
{
    ASSERT(&m_runLoop == &RunLoop::current());

    GUniqueOutPtr<GError> error;
    GUniqueOutPtr<gchar> debug;

    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_EOS:
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        m_runLoop.stop();
        break;
    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(message, &error.outPtr(), &debug.outPtr());
        g_warning("Warning: %d, %s. Debug output: %s", error->code, error->message, debug.get());
        break;
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(message, &error.outPtr(), &debug.outPtr());
        g_warning("Error: %d, %s. Debug output: %s", error->code, error->message, debug.get());
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
        m_runLoop.stop();
        break;
    default:
        break;
    }
}

void ImageDecoderGStreamer::InnerDecoder::preparePipeline()
{
    static Atomic<uint32_t> pipelineId;
    m_pipeline = gst_pipeline_new(makeString("image-decoder-", pipelineId.exchangeAdd(1)).utf8().data());

    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    ASSERT(bus);

    gst_bus_set_sync_handler(bus.get(), [](GstBus*, GstMessage* message, gpointer userData) {
        auto& decoder = *static_cast<ImageDecoderGStreamer::InnerDecoder*>(userData);
        if (&decoder.m_runLoop == &RunLoop::current())
            decoder.handleMessage(message);
        else {
            GRefPtr<GstMessage> protectedMessage(message);
            auto weakThis = makeWeakPtr(decoder);
            decoder.m_runLoop.dispatch([weakThis, protectedMessage] {
                if (weakThis)
                    weakThis->handleMessage(protectedMessage.get());
            });
        }
        gst_message_unref(message);
        return GST_BUS_DROP;
    }, this, nullptr);

    GstElement* source = gst_element_factory_make("giostreamsrc", nullptr);
    g_object_set(source, "stream", m_memoryStream.get(), nullptr);

    GstElement* decoder = gst_element_factory_make("decodebin", nullptr);
    auto allowedCaps = adoptGRef(gst_caps_new_empty_simple("video/x-raw"));
    g_object_set(decoder, "caps", allowedCaps.get(), "expose-all-streams", false, nullptr);
    g_signal_connect_swapped(decoder, "pad-added", G_CALLBACK(decodebinPadAddedCallback), this);

    gst_bin_add_many(GST_BIN_CAST(m_pipeline.get()), source, decoder, nullptr);
    gst_element_link(source, decoder);
    gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
}

void ImageDecoderGStreamer::InnerDecoder::run()
{
    m_runLoop.dispatch([this]() {
        preparePipeline();
    });

    m_runLoop.run();
}

EncodedDataStatus ImageDecoderGStreamer::InnerDecoder::encodedDataStatus() const
{
    GstState state;
    gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
    if (state >= GST_STATE_READY)
        return EncodedDataStatus::TypeAvailable;
    return EncodedDataStatus::Unknown;
}

void ImageDecoderGStreamer::pushEncodedData(const SharedBuffer& buffer)
{
    m_eos = false;
    auto thread = Thread::create("ImageDecoderGStreamer", [this, data = buffer.data(), size = buffer.size()] {
        m_innerDecoder = ImageDecoderGStreamer::InnerDecoder::create(*this, data, size);
        m_innerDecoder->run();
    });
    thread->waitForCompletion();
    m_eos = true;
    m_innerDecoder = nullptr;
    callOnMainThread([this] {
        if (m_encodedDataStatusChangedCallback)
            m_encodedDataStatusChangedCallback(encodedDataStatus());
    });
}

}
#endif
