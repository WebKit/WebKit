/*
 * Copyright (C) 2016-2017 Apple Inc.  All rights reserved.
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

#include "ImageFrame.h"

#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class BitmapImage;
class GraphicsContext;
class ImageDecoder;

class ImageSource : public ThreadSafeRefCounted<ImageSource> {
    friend class BitmapImage;
public:
    ~ImageSource();

    static Ref<ImageSource> create(BitmapImage* image, AlphaOption alphaOption = AlphaOption::Premultiplied, GammaAndColorProfileOption gammaAndColorProfileOption = GammaAndColorProfileOption::Applied)
    {
        return adoptRef(*new ImageSource(image, alphaOption, gammaAndColorProfileOption));
    }

    static Ref<ImageSource> create(NativeImagePtr&& nativeImage)
    {
        return adoptRef(*new ImageSource(WTFMove(nativeImage)));
    }

    void setData(SharedBuffer* data, bool allDataReceived);
    void resetData(SharedBuffer* data);
    EncodedDataStatus dataChanged(SharedBuffer* data, bool allDataReceived);
    bool isAllDataReceived();

    unsigned decodedSize() const { return m_decodedSize; }
    void destroyAllDecodedData() { destroyDecodedData(frameCount(), frameCount()); }
    void destroyAllDecodedDataExcludeFrame(size_t excludeFrame) { destroyDecodedData(frameCount(), excludeFrame); }
    void destroyDecodedDataBeforeFrame(size_t beforeFrame) { destroyDecodedData(beforeFrame, beforeFrame); }
    void destroyIncompleteDecodedData();
    void clearFrameBufferCache(size_t beforeFrame);

    void growFrames();
    void clearMetadata();
    void clearImage() { m_image = nullptr; }
    URL sourceURL() const;
    String mimeType() const;
    long long expectedContentLength() const;

    // Asynchronous image decoding
    bool canUseAsyncDecoding();
    void startAsyncDecodingQueue();
    void requestFrameAsyncDecodingAtIndex(size_t, SubsamplingLevel, const Optional<IntSize>& = { });
    void stopAsyncDecodingQueue();
    bool hasAsyncDecodingQueue() const { return m_decodingQueue; }
    bool isAsyncDecodingQueueIdle() const;
    void setFrameDecodingDurationForTesting(Seconds duration) { m_frameDecodingDurationForTesting = duration; }
    Seconds frameDecodingDurationForTesting() const { return m_frameDecodingDurationForTesting; }

    // Image metadata which is calculated either by the ImageDecoder or directly
    // from the NativeImage if this class was created for a memory image.
    EncodedDataStatus encodedDataStatus();
    bool isSizeAvailable() { return encodedDataStatus() >= EncodedDataStatus::SizeAvailable; }
    size_t frameCount();
    RepetitionCount repetitionCount();
    String uti();
    String filenameExtension();
    Optional<IntPoint> hotSpot();

    // Image metadata which is calculated from the first ImageFrame.
    WEBCORE_EXPORT IntSize size();
    IntSize sizeRespectingOrientation();
    Color singlePixelSolidColor();
    SubsamplingLevel maximumSubsamplingLevel();

    // ImageFrame metadata which does not require caching the ImageFrame.
    bool frameIsBeingDecodedAndIsCompatibleWithOptionsAtIndex(size_t, const DecodingOptions&);
    DecodingStatus frameDecodingStatusAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t);
    bool frameHasImageAtIndex(size_t);
    bool frameHasFullSizeNativeImageAtIndex(size_t, const Optional<SubsamplingLevel>&);
    bool frameHasDecodedNativeImageCompatibleWithOptionsAtIndex(size_t, const Optional<SubsamplingLevel>&, const DecodingOptions&);
    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t);

    // ImageFrame metadata which forces caching or re-caching the ImageFrame.
    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    Seconds frameDurationAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t);

#if USE(DIRECT2D)
    void setTargetContext(const GraphicsContext*);
#endif
    NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    NativeImagePtr frameImageAtIndex(size_t);
    NativeImagePtr frameImageAtIndexCacheIfNeeded(size_t, SubsamplingLevel = SubsamplingLevel::Default);

private:
    ImageSource(BitmapImage*, AlphaOption = AlphaOption::Premultiplied, GammaAndColorProfileOption = GammaAndColorProfileOption::Applied);
    ImageSource(NativeImagePtr&&);

    template<typename T, T (ImageDecoder::*functor)() const>
    T metadata(const T& defaultValue, Optional<T>* cachedValue = nullptr);

    template<typename T, typename... Args>
    T frameMetadataAtIndex(size_t, T (ImageFrame::*functor)(Args...) const, Args&&...);

    template<typename T, typename... Args>
    T frameMetadataAtIndexCacheIfNeeded(size_t, T (ImageFrame::*functor)() const,  Optional<T>* cachedValue, Args&&...);

    bool ensureDecoderAvailable(SharedBuffer* data);
    bool isDecoderAvailable() const { return m_decoder; }
    void destroyDecodedData(size_t frameCount, size_t excludeFrame);
    void decodedSizeChanged(long long decodedSize);
    void didDecodeProperties(unsigned decodedPropertiesSize);
    void decodedSizeIncreased(unsigned decodedSize);
    void decodedSizeDecreased(unsigned decodedSize);
    void decodedSizeReset(unsigned decodedSize);

    void setNativeImage(NativeImagePtr&&);
    void cacheMetadataAtIndex(size_t, SubsamplingLevel, DecodingStatus = DecodingStatus::Invalid);
    void cacheNativeImageAtIndex(NativeImagePtr&&, size_t, SubsamplingLevel, const DecodingOptions&, DecodingStatus = DecodingStatus::Invalid);
    void cacheNativeImageAtIndexAsync(NativeImagePtr&&, size_t, SubsamplingLevel, const DecodingOptions&, DecodingStatus);

    struct ImageFrameRequest;
    static const int BufferSize = 8;
    WorkQueue& decodingQueue();
    SynchronizedFixedQueue<ImageFrameRequest, BufferSize>& frameRequestQueue();

    const ImageFrame& frameAtIndexCacheIfNeeded(size_t, ImageFrame::Caching, const Optional<SubsamplingLevel>& = { });

    void dump(TextStream&);

    BitmapImage* m_image { nullptr };
    RefPtr<ImageDecoder> m_decoder;
    AlphaOption m_alphaOption { AlphaOption::Premultiplied };
    GammaAndColorProfileOption m_gammaAndColorProfileOption { GammaAndColorProfileOption::Applied };

    unsigned m_decodedSize { 0 };
    unsigned m_decodedPropertiesSize { 0 };
    Vector<ImageFrame, 1> m_frames;

    // Asynchronous image decoding.
    struct ImageFrameRequest {
        size_t index;
        SubsamplingLevel subsamplingLevel;
        DecodingOptions decodingOptions;
        DecodingStatus decodingStatus;
        bool operator==(const ImageFrameRequest& other) const
        {
            return index == other.index && subsamplingLevel == other.subsamplingLevel && decodingOptions == other.decodingOptions && decodingStatus == other.decodingStatus;
        }
    };
    using FrameRequestQueue = SynchronizedFixedQueue<ImageFrameRequest, BufferSize>;
    using FrameCommitQueue = Deque<ImageFrameRequest, BufferSize>;
    RefPtr<FrameRequestQueue> m_frameRequestQueue;
    FrameCommitQueue m_frameCommitQueue;
    RefPtr<WorkQueue> m_decodingQueue;
    Seconds m_frameDecodingDurationForTesting;

    // Image metadata.
    Optional<EncodedDataStatus> m_encodedDataStatus;
    Optional<size_t> m_frameCount;
    Optional<RepetitionCount> m_repetitionCount;
    Optional<String> m_uti;
    Optional<String> m_filenameExtension;
    Optional<Optional<IntPoint>> m_hotSpot;

    // Image metadata which is calculated from the first ImageFrame.
    Optional<IntSize> m_size;
    Optional<IntSize> m_sizeRespectingOrientation;
    Optional<Color> m_singlePixelSolidColor;
    Optional<SubsamplingLevel> m_maximumSubsamplingLevel;
};

}
