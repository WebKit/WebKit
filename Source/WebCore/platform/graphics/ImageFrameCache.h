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

namespace WebCore {

class GraphicsContext;
class Image;
class ImageDecoder;

class ImageFrameCache {
    friend class ImageSource;
public:
    ImageFrameCache(Image*);
    ImageFrameCache(NativeImagePtr&&);

    void setDecoder(ImageDecoder* decoder) { m_decoder = decoder; }
    ImageDecoder* decoder() const { return m_decoder; }

    unsigned decodedSize() const { return m_decodedSize; }
    void destroyDecodedData(bool destroyAll = true, size_t count = 0);
    bool destroyDecodedDataIfNecessary(bool destroyAll = true, size_t count = 0);
    void destroyIncompleteDecodedData();

    void growFrames();
    void clearMetadata();

    // Image metadata which is calculated either by the ImageDecoder or directly
    // from the NativeImage if this class was created for a memory image.
    bool isSizeAvailable();
    size_t frameCount();
    RepetitionCount repetitionCount();
    String filenameExtension();
    Optional<IntPoint> hotSpot();
    
    // Image metadata which is calculated from the first ImageFrame.
    IntSize size();
    IntSize sizeRespectingOrientation();

    Color singlePixelSolidColor();

    // ImageFrame metadata which does not require caching the ImageFrame.
    bool frameIsCompleteAtIndex(size_t);
    bool frameHasAlphaAtIndex(size_t);
    bool frameHasImageAtIndex(size_t);
    bool frameHasInvalidNativeImageAtIndex(size_t, SubsamplingLevel);
    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t);
    
    // ImageFrame metadata which forces caching or re-caching the ImageFrame.
    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);
    float frameDurationAtIndex(size_t);
    ImageOrientation frameOrientationAtIndex(size_t);
    NativeImagePtr frameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);

private:
    template<typename T, T (ImageDecoder::*functor)() const>
    T metadata(const T& defaultValue, Optional<T>* cachedValue = nullptr);

    template<typename T, T (ImageFrame::*functor)() const>
    T frameMetadataAtIndex(size_t index, SubsamplingLevel = SubsamplingLevel::Undefinded, ImageFrame::Caching = ImageFrame::Caching::Empty, Optional<T>* = nullptr);

    bool isDecoderAvailable() const { return m_decoder; }
    void decodedSizeChanged(long long decodedSize);
    void didDecodeProperties(unsigned decodedPropertiesSize);
    void decodedSizeIncreased(unsigned decodedSize);
    void decodedSizeDecreased(unsigned decodedSize);
    void decodedSizeReset(unsigned decodedSize);

    void setNativeImage(NativeImagePtr&&);
    void setFrameNativeImage(NativeImagePtr&&, size_t, SubsamplingLevel);
    void setFrameMetadata(size_t, SubsamplingLevel);
    const ImageFrame& frameAtIndex(size_t, SubsamplingLevel, ImageFrame::Caching);

    // Animated images over a certain size are considered large enough that we'll only hang on to one frame at a time.
#if !PLATFORM(IOS)
    static const unsigned LargeAnimationCutoff = 5242880;
#else
    static const unsigned LargeAnimationCutoff = 2097152;
#endif

    Image* m_image { nullptr };
    ImageDecoder* m_decoder { nullptr };
    unsigned m_decodedSize { 0 };
    unsigned m_decodedPropertiesSize { 0 };

    Vector<ImageFrame, 1> m_frames;

    // Image metadata.
    Optional<bool> m_isSizeAvailable;
    Optional<size_t> m_frameCount;
    Optional<RepetitionCount> m_repetitionCount;
    Optional<String> m_filenameExtension;
    Optional<Optional<IntPoint>> m_hotSpot;

    // Image metadata which is calculated from the first ImageFrame.
    Optional<IntSize> m_size;
    Optional<IntSize> m_sizeRespectingOrientation;
    Optional<SubsamplingLevel> m_maximumSubsamplingLevel;
    Optional<Color> m_singlePixelSolidColor;
};
    
}
