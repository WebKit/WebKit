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

#pragma once

#if USE(GSTREAMER) && ENABLE(VIDEO)

#include "GStreamerCommon.h"
#include "GStreamerElementHarness.h"
#include "ImageDecoder.h"
#include "MIMETypeRegistry.h"
#include "SampleMap.h"
#include "SharedBuffer.h"
#include <wtf/Forward.h>
#include <wtf/Lock.h>

namespace WebCore {

class ContentType;
class ImageDecoderGStreamerSample;

class ImageDecoderGStreamer final : public ImageDecoder {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(ImageDecoderGStreamer);
public:
    static RefPtr<ImageDecoderGStreamer> create(FragmentedSharedBuffer&, const String& mimeType, AlphaOption, GammaAndColorProfileOption);
    ImageDecoderGStreamer(FragmentedSharedBuffer&, const String& mimeType, AlphaOption, GammaAndColorProfileOption);
    virtual ~ImageDecoderGStreamer() = default;

    static bool supportsMediaType(MediaType type) { return type == MediaType::Video; }
    static bool supportsContainerType(const String&);

    size_t bytesDecodedToDetermineProperties() const override { return 0; }
    static bool canDecodeType(const String& mimeType);

    void setEncodedDataStatusChangeCallback(Function<void(EncodedDataStatus)>&& callback) final { m_encodedDataStatusChangedCallback = WTFMove(callback); }
    EncodedDataStatus encodedDataStatus() const final;
    IntSize size() const final;
    size_t frameCount() const final { return m_sampleData.size(); }
    RepetitionCount repetitionCount() const final;
    String uti() const final;
    String filenameExtension() const final { return MIMETypeRegistry::preferredExtensionForMIMEType(m_mimeType); }
    std::optional<IntPoint> hotSpot() const final { return std::nullopt; }

    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const final { return size(); }
    bool frameIsCompleteAtIndex(size_t index) const final { return sampleAtIndex(index); }
    ImageDecoder::FrameMetadata frameMetadataAtIndex(size_t) const final;

    Seconds frameDurationAtIndex(size_t) const final;
    bool frameHasAlphaAtIndex(size_t) const final;
    bool frameAllowSubsamplingAtIndex(size_t index) const final { return index <= m_sampleData.size(); }
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const final;

    PlatformImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, const DecodingOptions& = DecodingOptions(DecodingMode::Synchronous)) final;

    void setExpectedContentSize(long long) final { }
    void setData(const FragmentedSharedBuffer&, bool allDataReceived) final;
    bool isAllDataReceived() const final { return m_eos; }
    void clearFrameBufferCache(size_t) final;

private:
    void pushEncodedData(const FragmentedSharedBuffer&);
    void storeDecodedSample(GRefPtr<GstSample>&&);
    const ImageDecoderGStreamerSample* sampleAtIndex(size_t) const;

    Function<void(EncodedDataStatus)> m_encodedDataStatusChangedCallback;
    SampleMap m_sampleData;
    DecodeOrderSampleMap::iterator m_cursor;
    Lock m_sampleGeneratorLock;
    bool m_eos { false };
    bool m_error { false };
    std::optional<IntSize> m_size;
    String m_mimeType;

    RefPtr<GStreamerElementHarness> m_parserHarness;
    RefPtr<GStreamerElementHarness> m_decoderHarness;
};

} // namespace WebCore

#endif // USE(GSTREAMER) && ENABLE(VIDEO)
