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

    void setDecoder(ImageDecoder* decoder) { m_decoder = decoder; }
    ImageDecoder* decoder() const { return m_decoder; }

    unsigned decodedSize() const { return m_decodedSize; }
    void destroyAllDecodedData() { destroyDecodedData(frameCount(), frameCount()); }
    void destroyAllDecodedDataExcludeFrame(size_t excludeFrame) { destroyDecodedData(frameCount(), excludeFrame); }
    void destroyDecodedDataBeforeFrame(size_t beforeFrame) { destroyDecodedData(beforeFrame, beforeFrame); }
    void destroyIncompleteDecodedData();

    void growFrames();
    void clearMetadata();
    
    // Asynchronous image decoding
    void startAsyncDecodingQueue();
    bool requestFrameAsyncDecodingAtIndex(size_t, SubsamplingLevel);
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
    bool frameIsBeingDecodedAtIndex(size_t);
    bool frameIsCompleteAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t);
    bool frameHasImageAtIndex(size_t);
    bool frameHasValidNativeImageAtIndex(size_t, SubsamplingLevel);
    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t);
    
    // ImageFrame metadata which forces caching or re-caching the ImageFrame.
    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    float frameDurationAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t);
    NativeImagePtr frameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);

private:
    ImageFrameCache(Image*);
    ImageFrameCache(NativeImagePtr&&);

    template<typename T, T (ImageDecoder::*functor)() const>
    T metadata(const T& defaultValue, std::optional<T>* cachedValue = nullptr);

    template<typename T, T (ImageFrame::*functor)() const>
    T frameMetadataAtIndex(size_t index, SubsamplingLevel = SubsamplingLevel::Undefinded, ImageFrame::Caching = ImageFrame::Caching::Empty, std::optional<T>* = nullptr);

    bool isDecoderAvailable() const { return m_decoder; }
    void destroyDecodedData(size_t frameCount, size_t excludeFrame);
    void decodedSizeChanged(long long decodedSize);
    void didDecodeProperties(unsigned decodedPropertiesSize);
    void decodedSizeIncreased(unsigned decodedSize);
    void decodedSizeDecreased(unsigned decodedSize);
    void decodedSizeReset(unsigned decodedSize);

    void setNativeImage(NativeImagePtr&&);
    void setFrameNativeImageAtIndex(NativeImagePtr&&, size_t, SubsamplingLevel);
    void setFrameMetadataAtIndex(size_t, SubsamplingLevel);
    void replaceFrameNativeImageAtIndex(NativeImagePtr&&, size_t, SubsamplingLevel);
    void cacheFrameNativeImageAtIndex(NativeImagePtr&&, size_t, SubsamplingLevel);

    Ref<WorkQueue> decodingQueue();

    const ImageFrame& frameAtIndex(size_t, SubsamplingLevel, ImageFrame::Caching);

    Image* m_image { nullptr };
    ImageDecoder* m_decoder { nullptr };
    unsigned m_decodedSize { 0 };
    unsigned m_decodedPropertiesSize { 0 };

    Vector<ImageFrame, 1> m_frames;

    // Asynchronous image decoding.
    struct ImageFrameRequest {
        size_t index;
        SubsamplingLevel subsamplingLevel;
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
