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
#include "TextStream.h"

#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/SynchronizedFixedQueue.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

class GraphicsContext;
class Image;
class ImageDecoder;

class ImageFrameCache : public RefCounted<ImageFrameCache> {
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
    
    // Asynchronous image decoding
    void startAsyncDecodingQueue();
    bool requestFrameAsyncDecodingAtIndex(size_t, SubsamplingLevel, const IntSize&);
    void stopAsyncDecodingQueue();
    bool hasDecodingQueue() { return m_decodingQueue; }

    // Image metadata which is calculated either by the ImageDecoder or directly
    // from the NativeImage if this class was created for a memory image.
    bool isSizeAvailable();
    size_t frameCount();
    RepetitionCount repetitionCount();
    String filenameExtension();
    std::optional<IntPoint> hotSpot();
    
    // Image metadata which is calculated from the first ImageFrame.
    IntSize size();
    IntSize sizeRespectingOrientation();

    Color singlePixelSolidColor();

    // ImageFrame metadata which does not require caching the ImageFrame.
    bool frameIsBeingDecodedAtIndex(size_t, const std::optional<IntSize>& sizeForDrawing);
    bool frameIsCompleteAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t);
    bool frameHasImageAtIndex(size_t);
    bool frameHasValidNativeImageAtIndex(size_t, const std::optional<SubsamplingLevel>&, const std::optional<IntSize>& sizeForDrawing);
    bool frameHasDecodedNativeImage(size_t);
    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t);
    
    // ImageFrame metadata which forces caching or re-caching the ImageFrame.
    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    float frameDurationAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t);
    NativeImagePtr frameImageAtIndex(size_t, const std::optional<SubsamplingLevel>&, const std::optional<IntSize>& sizeForDrawing);

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
    void setFrameNativeImageAtIndex(NativeImagePtr&&, size_t, SubsamplingLevel, const std::optional<IntSize>& sizeForDrawing);
    void setFrameMetadataAtIndex(size_t, SubsamplingLevel, const std::optional<IntSize>& sizeForDrawing);
    void replaceFrameNativeImageAtIndex(NativeImagePtr&&, size_t, SubsamplingLevel, const std::optional<IntSize>& sizeForDrawing);
    void cacheFrameNativeImageAtIndex(NativeImagePtr&&, size_t, SubsamplingLevel, const IntSize& sizeForDrawing);

    Ref<WorkQueue> decodingQueue();

    const ImageFrame& frameAtIndexCacheIfNeeded(size_t, ImageFrame::Caching, const std::optional<SubsamplingLevel>& = { }, const std::optional<IntSize>& sizeForDrawing = { });

    Image* m_image { nullptr };
    RefPtr<ImageDecoder> m_decoder;
    unsigned m_decodedSize { 0 };
    unsigned m_decodedPropertiesSize { 0 };

    Vector<ImageFrame, 1> m_frames;

    // Asynchronous image decoding.
    struct ImageFrameRequest {
        size_t index;
        SubsamplingLevel subsamplingLevel;
        IntSize sizeForDrawing;
    };
    static const int BufferSize = 8;
    using FrameRequestQueue = SynchronizedFixedQueue<ImageFrameRequest, BufferSize>;
    FrameRequestQueue m_frameRequestQueue;
    RefPtr<WorkQueue> m_decodingQueue;

    // Image metadata.
    std::optional<bool> m_isSizeAvailable;
    std::optional<size_t> m_frameCount;
    std::optional<RepetitionCount> m_repetitionCount;
    std::optional<String> m_filenameExtension;
    std::optional<std::optional<IntPoint>> m_hotSpot;

    // Image metadata which is calculated from the first ImageFrame.
    std::optional<IntSize> m_size;
    std::optional<IntSize> m_sizeRespectingOrientation;
    std::optional<Color> m_singlePixelSolidColor;
};
    
}
