/*
 * Copyright (C) 2004-2008, 2010, 2012, 2014, 2016 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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
#include "ImageFrameCache.h"
#include "ImageOrientation.h"
#include "IntPoint.h"
#include "NativeImage.h"
#include "TextStream.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>

namespace WebCore {

class GraphicsContext;
class ImageDecoder;
class ImageOrientation;
class IntPoint;
class IntSize;
class SharedBuffer;

class ImageSource {
    WTF_MAKE_NONCOPYABLE(ImageSource);
    friend class BitmapImage;
public:
    ImageSource(NativeImagePtr&&);
    ImageSource(Image*, AlphaOption = AlphaOption::Premultiplied, GammaAndColorProfileOption = GammaAndColorProfileOption::Applied);
    ~ImageSource();

    void destroyDecodedData(SharedBuffer* data, bool destroyAll = true, size_t count = 0);
    bool destroyDecodedDataIfNecessary(SharedBuffer* data, bool destroyAll = true, size_t count = 0);

    bool ensureDecoderAvailable(SharedBuffer*);
    bool isDecoderAvailable() const { return m_decoder.get(); }

    void setData(SharedBuffer* data, bool allDataReceived);
    bool dataChanged(SharedBuffer* data, bool allDataReceived);

    unsigned decodedSize() const { return m_frameCache.decodedSize(); }
    bool isAllDataReceived();

    // Image metadata which is calculated by the decoder or can deduced by the case of the memory NativeImage.
    bool isSizeAvailable() { return m_frameCache.isSizeAvailable(); }
    size_t frameCount() { return m_frameCache.frameCount(); }
    RepetitionCount repetitionCount() { return m_frameCache.repetitionCount(); }
    String filenameExtension() { return m_frameCache.filenameExtension(); }
    Optional<IntPoint> hotSpot() { return m_frameCache.hotSpot(); }

    // Image metadata which is calculated from the first ImageFrame.
    IntSize size() { return m_frameCache.size(); }
    IntSize sizeRespectingOrientation() { return m_frameCache.sizeRespectingOrientation(); }
    Color singlePixelSolidColor() { return m_frameCache.singlePixelSolidColor(); }

    // ImageFrame metadata which does not require caching the ImageFrame.
    bool frameIsCompleteAtIndex(size_t index) { return m_frameCache.frameIsCompleteAtIndex(index); }
    bool frameHasAlphaAtIndex(size_t index) { return m_frameCache.frameHasAlphaAtIndex(index); }
    bool frameHasImageAtIndex(size_t index) { return m_frameCache.frameHasImageAtIndex(index); }
    bool frameHasInvalidNativeImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel) { return m_frameCache.frameHasInvalidNativeImageAtIndex(index, subsamplingLevel); }
    SubsamplingLevel frameSubsamplingLevelAtIndex(size_t index) { return m_frameCache.frameSubsamplingLevelAtIndex(index); }

    // ImageFrame metadata which forces caching or re-caching the ImageFrame.
    IntSize frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel = SubsamplingLevel::Default) { return m_frameCache.frameSizeAtIndex(index, subsamplingLevel); }
    unsigned frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel = SubsamplingLevel::Default) { return m_frameCache.frameBytesAtIndex(index, subsamplingLevel); }
    float frameDurationAtIndex(size_t index) { return m_frameCache.frameDurationAtIndex(index); }
    ImageOrientation frameOrientationAtIndex(size_t index) { return m_frameCache.frameOrientationAtIndex(index); }
    NativeImagePtr frameImageAtIndex(size_t index, SubsamplingLevel = SubsamplingLevel::Default, const GraphicsContext* targetContext = nullptr);

    SubsamplingLevel maximumSubsamplingLevel();
    SubsamplingLevel subsamplingLevelForScale(float);
    void setAllowSubsampling(bool allowSubsampling) { m_allowSubsampling = allowSubsampling; }
    NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default);

private:
    void clearFrameBufferCache(size_t);
    void clear(bool destroyAll, size_t count, SharedBuffer* data);
    void dump(TextStream&);

    void setDecoderTargetContext(const GraphicsContext*);

    ImageFrameCache m_frameCache;
    std::unique_ptr<ImageDecoder> m_decoder;

    Optional<SubsamplingLevel> m_maximumSubsamplingLevel;

    // The default value of m_allowSubsampling should be the same as defaultImageSubsamplingEnabled in Settings.cpp
#if PLATFORM(IOS)
    bool m_allowSubsampling { true };
#else
    bool m_allowSubsampling { false };
#endif

#if PLATFORM(IOS)
    // FIXME: We should expose a setting to enable/disable progressive loading so that we can remove the PLATFORM(IOS)-guard.
    double m_progressiveLoadChunkTime { 0 };
    uint16_t m_progressiveLoadChunkCount { 0 };
#endif

    AlphaOption m_alphaOption { AlphaOption::Premultiplied };
    GammaAndColorProfileOption m_gammaAndColorProfileOption { GammaAndColorProfileOption::Applied };
};

}
