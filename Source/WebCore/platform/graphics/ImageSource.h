/*
 * Copyright (C) 2016-2023 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BitmapImageMetadata.h"
#include "ImageFrame.h"

#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class BitmapImage;
class GraphicsContext;
class ImageDecoder;
class FragmentedSharedBuffer;

class ImageSource final
    : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<ImageSource> {
    friend class BitmapImage;
    friend class BitmapImageMetadata;
public:
    ~ImageSource();

    static Ref<ImageSource> create(BitmapImage* image, AlphaOption alphaOption = AlphaOption::Premultiplied, GammaAndColorProfileOption gammaAndColorProfileOption = GammaAndColorProfileOption::Applied)
    {
        return adoptRef(*new ImageSource(image, alphaOption, gammaAndColorProfileOption));
    }

    static Ref<ImageSource> create(Ref<NativeImage>&& nativeImage)
    {
        return adoptRef(*new ImageSource(WTFMove(nativeImage)));
    }

    void setData(FragmentedSharedBuffer* data, bool allDataReceived);
    void resetData(FragmentedSharedBuffer* data);
    EncodedDataStatus dataChanged(FragmentedSharedBuffer* data, bool allDataReceived);
    bool isAllDataReceived();

    unsigned decodedSize() const { return m_decodedSize; }
    void destroyDecodedData(size_t begin, size_t end);
    void destroyIncompleteDecodedData();
    void clearFrameBufferCache(size_t beforeFrame);

    void growFrames();
    void clearImage() { m_image = nullptr; }
    URL sourceURL() const;
    String mimeType() const;
    long long expectedContentLength() const;

    // Asynchronous image decoding
    bool canUseAsyncDecoding();
    void startAsyncDecodingQueue();
    void requestFrameAsyncDecodingAtIndex(size_t, SubsamplingLevel, const std::optional<IntSize>& = { });
    void stopAsyncDecodingQueue();
    bool hasAsyncDecodingQueue() const { return m_decodingQueue; }
    bool isAsyncDecodingQueueIdle() const;
    void setFrameDecodingDurationForTesting(Seconds duration) { m_frameDecodingDurationForTesting = duration; }
    Seconds frameDecodingDurationForTesting() const { return m_frameDecodingDurationForTesting; }

    EncodedDataStatus encodedDataStatus() const;
    WEBCORE_EXPORT IntSize size(ImageOrientation = ImageOrientation::Orientation::FromImage) const;
    IntSize sourceSize(ImageOrientation = ImageOrientation::Orientation::FromImage) const;
    bool hasDensityCorrectedSize() const { return m_metadata.densityCorrectedSize().has_value(); }
    ImageOrientation orientation() const { return m_metadata.orientation(); }
    WEBCORE_EXPORT unsigned frameCount() const;
    RepetitionCount repetitionCount() const { return m_metadata.repetitionCount(); }
    DestinationColorSpace colorSpace() const { return m_metadata.colorSpace(); }
    bool notSolidColor() const;
    std::optional<Color> singlePixelSolidColor() const;

    String uti() const { return m_metadata.uti(); }
    String filenameExtension() const { return m_metadata.filenameExtension(); }
    String accessibilityDescription() const { return m_metadata.accessibilityDescription(); }
    std::optional<IntPoint> hotSpot() const { return m_metadata.hotSpot(); }
    SubsamplingLevel maximumSubsamplingLevel() { return m_metadata.maximumSubsamplingLevel(); }

    unsigned primaryFrameIndex() const { return m_metadata.primaryFrameIndex(); }

    // Image metadata which is calculated either by the ImageDecoder or directly
    // from the NativeImage if this class was created for a memory image.
    bool isSizeAvailable() { return encodedDataStatus() >= EncodedDataStatus::SizeAvailable; }

    // ImageFrame metadata which does not require caching the ImageFrame.
    bool frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(size_t, const DecodingOptions&);
    DecodingStatus frameDecodingStatusAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t);
    bool frameHasImageAtIndex(size_t);
    bool frameHasFullSizeNativeImageAtIndex(size_t, const std::optional<SubsamplingLevel>&);
    bool frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(size_t, const std::optional<SubsamplingLevel>&, const DecodingOptions&);
    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t);

    // ImageFrame metadata which forces caching or re-caching the ImageFrame.
    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    WEBCORE_EXPORT Seconds frameDurationAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t);

    RefPtr<NativeImage> createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    RefPtr<NativeImage> frameImageAtIndex(size_t);
    RefPtr<NativeImage> frameImageAtIndexCacheIfNeeded(size_t, SubsamplingLevel = SubsamplingLevel::Default, const DecodingOptions& = { });

private:
    ImageSource(BitmapImage*, AlphaOption = AlphaOption::Premultiplied, GammaAndColorProfileOption = GammaAndColorProfileOption::Applied);
    ImageSource(Ref<NativeImage>&&);

    bool ensureDecoderAvailable(FragmentedSharedBuffer* data);
    bool isDecoderAvailable() const { return m_decoder; }
    void decodedSizeChanged(long long decodedSize);
    void didDecodeProperties(unsigned decodedPropertiesSize);
    void decodedSizeIncreased(unsigned decodedSize);
    void decodedSizeDecreased(unsigned decodedSize);
    void decodedSizeReset(unsigned decodedSize);
    void encodedDataStatusChanged(EncodedDataStatus);

    void setNativeImage(Ref<NativeImage>&&);
    void cacheMetadataAtIndex(size_t, SubsamplingLevel);
    void cachePlatformImageAtIndex(PlatformImagePtr&&, size_t, SubsamplingLevel, const DecodingOptions&);
    void cachePlatformImageAtIndexAsync(PlatformImagePtr&&, size_t, SubsamplingLevel, const DecodingOptions&);

    struct ImageFrameRequest;
    static const int BufferSize = 8;
    WorkQueue& decodingQueue();
    SynchronizedFixedQueue<ImageFrameRequest, BufferSize>& frameRequestQueue();

    const ImageFrame& frameAtIndex(size_t index) { return index < m_frames.size() ? m_frames[index] : ImageFrame::defaultFrame(); }
    const ImageFrame& frameAtIndexCacheIfNeeded(size_t, ImageFrame::Caching, const std::optional<SubsamplingLevel>& = { }, const DecodingOptions& = { });

    void dump(TextStream&);

    BitmapImage* m_image { nullptr };
    RefPtr<ImageDecoder> m_decoder;
    AlphaOption m_alphaOption { AlphaOption::Premultiplied };
    GammaAndColorProfileOption m_gammaAndColorProfileOption { GammaAndColorProfileOption::Applied };

    BitmapImageMetadata m_metadata;

    unsigned m_decodedSize { 0 };
    unsigned m_decodedPropertiesSize { 0 };
    Vector<ImageFrame, 1> m_frames;

    // Asynchronous image decoding.
    struct ImageFrameRequest {
        size_t index;
        SubsamplingLevel subsamplingLevel;
        DecodingOptions decodingOptions;
        friend bool operator==(const ImageFrameRequest&, const ImageFrameRequest&) = default;
    };
    using FrameRequestQueue = SynchronizedFixedQueue<ImageFrameRequest, BufferSize>;
    using FrameCommitQueue = Deque<ImageFrameRequest, BufferSize>;
    RefPtr<FrameRequestQueue> m_frameRequestQueue;
    FrameCommitQueue m_frameCommitQueue;
    RefPtr<WorkQueue> m_decodingQueue;
    Seconds m_frameDecodingDurationForTesting;

    RunLoop& m_runLoop;
};

} // namespace WebCore
