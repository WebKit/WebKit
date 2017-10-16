/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

namespace WebCore {

class GraphicsContext;
class Image;
class ImageDecoder;
class URL;

class ImageFrameCache : public ThreadSafeRefCounted<ImageFrameCache> {
    friend class ImageSource;
public:
    static Ref<ImageFrameCache> create(Image* image)
    {
        return adoptRef(*new ImageFrameCache(image));
    }

    static Ref<ImageFrameCache> create(NativeImagePtr&& nativeImage)
    {
        return adoptRef(*new ImageFrameCache(WTFMove(nativeImage)));
    }

    ~ImageFrameCache();

    void setDecoder(ImageDecoder*);
    ImageDecoder* decoder() const;

    unsigned decodedSize() const { return m_decodedSize; }
    void destroyAllDecodedData() { destroyDecodedData(frameCount(), frameCount()); }
    void destroyAllDecodedDataExcludeFrame(size_t excludeFrame) { destroyDecodedData(frameCount(), excludeFrame); }
    void destroyDecodedDataBeforeFrame(size_t beforeFrame) { destroyDecodedData(beforeFrame, beforeFrame); }
    void destroyIncompleteDecodedData();

    void growFrames();
    void clearMetadata();
    void clearImage() { m_image = nullptr; }
    URL sourceURL() const;

    // Asynchronous image decoding
    void startAsyncDecodingQueue();
    void requestFrameAsyncDecodingAtIndex(size_t, SubsamplingLevel, const std::optional<IntSize>&);
    void stopAsyncDecodingQueue();
    bool hasAsyncDecodingQueue() const { return m_decodingQueue; }
    bool isAsyncDecodingQueueIdle() const;

    // Image metadata which is calculated either by the ImageDecoder or directly
    // from the NativeImage if this class was created for a memory image.
    EncodedDataStatus encodedDataStatus();
    bool isSizeAvailable() { return encodedDataStatus() >= EncodedDataStatus::SizeAvailable; }
    size_t frameCount();
    RepetitionCount repetitionCount();
    String uti();
    String filenameExtension();
    std::optional<IntPoint> hotSpot();

    // Image metadata which is calculated from the first ImageFrame.
    IntSize size();
    IntSize sizeRespectingOrientation();

    Color singlePixelSolidColor();

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
    float frameDurationAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t);

    NativeImagePtr frameImageAtIndex(size_t);
    NativeImagePtr frameImageAtIndexCacheIfNeeded(size_t, SubsamplingLevel);

private:
    ImageFrameCache(Image*);
    ImageFrameCache(NativeImagePtr&&);

    template<typename T, T (ImageDecoder::*functor)() const>
    T metadata(const T& defaultValue, std::optional<T>* cachedValue = nullptr);

    template<typename T, typename... Args>
    T frameMetadataAtIndex(size_t, T (ImageFrame::*functor)(Args...) const, Args&&...);

    template<typename T, typename... Args>
    T frameMetadataAtIndexCacheIfNeeded(size_t, T (ImageFrame::*functor)() const,  std::optional<T>* cachedValue, Args&&...);

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

    const ImageFrame& frameAtIndexCacheIfNeeded(size_t, ImageFrame::Caching, const std::optional<SubsamplingLevel>& = { });

    Image* m_image { nullptr };
    RefPtr<ImageDecoder> m_decoder;
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

    // Image metadata.
    std::optional<EncodedDataStatus> m_encodedDataStatus;
    std::optional<size_t> m_frameCount;
    std::optional<RepetitionCount> m_repetitionCount;
    std::optional<String> m_uti;
    std::optional<String> m_filenameExtension;
    std::optional<std::optional<IntPoint>> m_hotSpot;

    // Image metadata which is calculated from the first ImageFrame.
    std::optional<IntSize> m_size;
    std::optional<IntSize> m_sizeRespectingOrientation;
    std::optional<Color> m_singlePixelSolidColor;
};
    
}
