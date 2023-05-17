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
#include "GStreamerRegistryScanner.h"
#include "ImageGStreamer.h"
#include "MediaSampleGStreamer.h"
#include "NotImplemented.h"
#include "RuntimeApplicationChecks.h"
#include "VideoFrameGStreamer.h"
#include <gst/base/gsttypefindhelper.h>
#include <wtf/MainThread.h>
#include <wtf/Scope.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_image_decoder_debug);
#define GST_CAT_DEFAULT webkit_image_decoder_debug

class ImageDecoderGStreamerSample final : public MediaSampleGStreamer {
public:
    static Ref<ImageDecoderGStreamerSample> create(GRefPtr<GstSample>&& sample, const FloatSize& presentationSize)
    {
        return adoptRef(*new ImageDecoderGStreamerSample(WTFMove(sample), presentationSize));
    }

    PlatformImagePtr image() const
    {
        if (!m_image)
            return nullptr;
        return m_image->image().nativeImage()->platformImage();
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
        m_frame = VideoFrameGStreamer::createWrappedSample(platformSample().sample.gstSample, MediaTime::invalidTime());
        m_image = m_frame->convertToImage();
    }

    RefPtr<VideoFrameGStreamer> m_frame;
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

RefPtr<ImageDecoderGStreamer> ImageDecoderGStreamer::create(FragmentedSharedBuffer& data, const String& mimeType, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
{
    return adoptRef(*new ImageDecoderGStreamer(data, mimeType, alphaOption, gammaAndColorProfileOption));
}

ImageDecoderGStreamer::ImageDecoderGStreamer(FragmentedSharedBuffer& data, const String& mimeType, AlphaOption, GammaAndColorProfileOption)
    : m_mimeType(mimeType)
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_image_decoder_debug, "webkitimagedecoder", 0, "WebKit image decoder");
    });

    static Atomic<uint32_t> decoderId;
    GRefPtr<GstElement> parsebin = gst_element_factory_make("parsebin", makeString("image-decoder-parser-", decoderId.exchangeAdd(1)).utf8().data());
    m_parserHarness = GStreamerElementHarness::create(WTFMove(parsebin), [](auto&, const auto&) { }, [this](auto& pad) -> RefPtr<GStreamerElementHarness> {
        auto caps = adoptGRef(gst_pad_query_caps(pad.get(), nullptr));
        auto identityHarness = GStreamerElementHarness::create(GRefPtr<GstElement>(gst_element_factory_make("identity", nullptr)), [](auto&, const auto&) { });
        GST_DEBUG_OBJECT(pad.get(), "Caps on parser source pad: %" GST_PTR_FORMAT, caps.get());
        if (!caps || !doCapsHaveType(caps.get(), "video")) {
            GST_WARNING_OBJECT(m_decoderHarness->element(), "Ignoring non-video track");
            return identityHarness;
        }

        if (m_decoderHarness) {
            GST_WARNING_OBJECT(m_decoderHarness->element(), "Decoder already configured, ignoring additional video track");
            return identityHarness;
        }

        auto& scanner = GStreamerRegistryScanner::singleton();
        auto lookupResult = scanner.areCapsSupported(GStreamerRegistryScanner::Configuration::Decoding, caps, false);
        if (!lookupResult) {
            GST_WARNING_OBJECT(m_parserHarness->element(), "No decoder found for caps %" GST_PTR_FORMAT, caps.get());
            return identityHarness;
        }

        GRefPtr<GstElement> element = gst_element_factory_create(lookupResult.factory.get(), nullptr);
        configureVideoDecoderForHarnessing(element);
        m_decoderHarness = GStreamerElementHarness::create(WTFMove(element), [this](auto& stream, const auto& outputBuffer) {
            auto outputCaps = stream.outputCaps();
            storeDecodedSample(adoptGRef(gst_sample_new(outputBuffer.get(), outputCaps.get(), nullptr, nullptr)));
        }, { });
        return m_decoderHarness;
    });

    pushEncodedData(data);
}

bool ImageDecoderGStreamer::supportsContainerType(const String& type)
{
    // Ideally this decoder should operate only from the WebProcess (or from the GPUProcess) which
    // should be the only process where GStreamer has been runtime initialized.
    if (!isInWebProcess())
        return false;

    if (!type.startsWith("video/"_s))
        return false;

    return GStreamerRegistryScanner::singleton().isContainerTypeSupported(GStreamerRegistryScanner::Configuration::Decoding, type);
}

bool ImageDecoderGStreamer::canDecodeType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;

    if (!mimeType.startsWith("video/"_s))
        return false;

    // Ideally this decoder should operate only from the WebProcess (or from the GPUProcess) which
    // should be the only process where GStreamer has been runtime initialized.
    if (!isInWebProcess())
        return false;

    return GStreamerRegistryScanner::singleton().isContainerTypeSupported(GStreamerRegistryScanner::Configuration::Decoding, mimeType);
}

EncodedDataStatus ImageDecoderGStreamer::encodedDataStatus() const
{
    if (m_error)
        return EncodedDataStatus::Error;

    if (m_eos)
        return EncodedDataStatus::Complete;
    if (m_size)
        return EncodedDataStatus::SizeAvailable;
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

ImageDecoder::FrameMetadata ImageDecoderGStreamer::frameMetadataAtIndex(size_t) const
{
    notImplemented();
    return { };
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

    return frameSizeAtIndex(index, subsamplingLevel).area() * 4;
}

PlatformImagePtr ImageDecoderGStreamer::createFrameImageAtIndex(size_t index, SubsamplingLevel, const DecodingOptions&)
{
    Locker locker { m_sampleGeneratorLock };

    auto* sampleData = sampleAtIndex(index);
    if (!sampleData)
        return nullptr;

    if (auto image = sampleData->image())
        return image;

    return nullptr;
}

void ImageDecoderGStreamer::setData(const FragmentedSharedBuffer& data, bool)
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

void ImageDecoderGStreamer::storeDecodedSample(GRefPtr<GstSample>&& sample)
{
    auto presentationSize = getVideoResolutionFromCaps(gst_sample_get_caps(sample.get()));
    if (presentationSize && !presentationSize->isEmpty() && (!m_size || m_size != roundedIntSize(*presentationSize)))
        m_size = roundedIntSize(*presentationSize);
    m_sampleData.addSample(ImageDecoderGStreamerSample::create(WTFMove(sample), *m_size));
}

void ImageDecoderGStreamer::pushEncodedData(const FragmentedSharedBuffer& sharedBuffer)
{
    auto data = sharedBuffer.makeContiguous();
    auto bytes = data->createGBytes();
    auto buffer = adoptGRef(gst_buffer_new_wrapped_bytes(bytes.get()));
    m_eos = false;
    m_error = false;

    auto scopeExit = makeScopeExit([&] {
        callOnMainThreadAndWait([&] {
            if (m_encodedDataStatusChangedCallback)
                m_encodedDataStatusChangedCallback(encodedDataStatus());
        });
    });

    auto caps = adoptGRef(gst_type_find_helper_for_buffer(GST_OBJECT_CAST(m_parserHarness->element()), buffer.get(), nullptr));
    GST_DEBUG_OBJECT(m_parserHarness->element(), "Caps typefind result: %" GST_PTR_FORMAT, caps.get());
    if (!caps) {
        GST_WARNING_OBJECT(m_parserHarness->element(), "Typefinding failed");
        m_error = true;
        return;
    }

    if (!m_parserHarness->pushSample(adoptGRef(gst_sample_new(buffer.get(), caps.get(), nullptr, nullptr)))) {
        GST_WARNING_OBJECT(m_parserHarness->element(), "Parser or downstream decoder failed to process data");
        m_error = true;
        return;
    }

    if (!m_decoderHarness) {
        GST_WARNING_OBJECT(m_parserHarness->element(), "Parsing failed");
        m_error = true;
        return;
    }

    for (auto& stream : m_parserHarness->outputStreams()) {
        while (auto event = stream->pullEvent())
            m_decoderHarness->pushEvent(WTFMove(event));
    }

    for (auto& stream : m_decoderHarness->outputStreams()) {
        while (auto event = stream->pullEvent()) {
            if (GST_EVENT_TYPE(event.get()) == GST_EVENT_EOS)
                m_eos = true;
        }
    }

    m_decoderHarness->flush();
}

} // namespace WebCore

#endif // USE(GSTREAMER) && ENABLE(VIDEO)
