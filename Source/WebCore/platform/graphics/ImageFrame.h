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

#include "Color.h"
#include "ImageBackingStore.h"
#include "ImageOrientation.h"
#include "IntSize.h"
#include "NativeImage.h"
#include <wtf/Deque.h>

namespace WebCore {

class Color;

// There are four subsampling levels: 0 = 1x, 1 = 0.5x, 2 = 0.25x, 3 = 0.125x.
enum class SubsamplingLevel {
    First = 0,
    Default = First,
    Level0 = First,
    Level1,
    Level2,
    Level3,
    Last = Level3,
    Max
};

inline SubsamplingLevel& operator++(SubsamplingLevel& subsamplingLevel)
{
    subsamplingLevel = static_cast<SubsamplingLevel>(static_cast<int>(subsamplingLevel) + 1);
    ASSERT(subsamplingLevel <= SubsamplingLevel::Max);
    return subsamplingLevel;
}

typedef int RepetitionCount;

enum {
    RepetitionCountNone = 0,
    RepetitionCountOnce = 1,
    RepetitionCountInfinite = -1,
};

enum class AlphaOption {
    Premultiplied,
    NotPremultiplied
};

enum class GammaAndColorProfileOption {
    Applied,
    Ignored
};

class ImageFrame {
    friend class ImageFrameCache;
public:
    enum class Caching { Metadata, MetadataAndImage };
    enum class Decoding { None, Partial, Complete };

    ImageFrame();
    ImageFrame(const ImageFrame& other) { operator=(other); }

    ~ImageFrame();

    static const ImageFrame& defaultFrame();

    ImageFrame& operator=(const ImageFrame& other);

    unsigned clearImage();
    unsigned clear();

#if !USE(CG)
    bool initialize(const ImageBackingStore&);
    bool initialize(const IntSize&, bool premultiplyAlpha);
#endif

    void setDecoding(Decoding decoding) { m_decoding = decoding; }
    Decoding decoding() const { return m_decoding; }
    void enqueueSizeForDecoding(const IntSize& sizeForDecoding) { m_sizeForDecoding.append(sizeForDecoding); }
    void dequeueSizeForDecoding() { m_sizeForDecoding.removeFirst(); }
    void clearSizeForDecoding() { m_sizeForDecoding.clear(); }

    bool isEmpty() const { return m_decoding == Decoding::None; }
    bool isBeingDecoded(const std::optional<IntSize>& sizeForDrawing = { }) const;
    bool isPartial() const { return m_decoding == Decoding::Partial; }
    bool isComplete() const { return m_decoding == Decoding::Complete; }

    IntSize size() const;
    IntSize sizeRespectingOrientation() const { return !m_orientation.usesWidthAsHeight() ? size() : size().transposedSize(); }
    unsigned frameBytes() const { return hasNativeImage() ? (size().area() * sizeof(RGBA32)).unsafeGet() : 0; }
    SubsamplingLevel subsamplingLevel() const { return m_subsamplingLevel; }
    std::optional<IntSize> sizeForDrawing() const { return m_sizeForDrawing; }

#if !USE(CG)
    enum class DisposalMethod { Unspecified, DoNotDispose, RestoreToBackground, RestoreToPrevious };
    void setDisposalMethod(DisposalMethod method) { m_disposalMethod = method; }
    DisposalMethod disposalMethod() const { return m_disposalMethod; }
#endif

    NativeImagePtr nativeImage() const { return m_nativeImage; }

    void setOrientation(ImageOrientation orientation) { m_orientation = orientation; };
    ImageOrientation orientation() const { return m_orientation; }

    void setDuration(float duration) { m_duration = duration; }
    float duration() const { return m_duration; }

    void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    bool hasAlpha() const { return !hasMetadata() || m_hasAlpha; }

    bool hasNativeImage() const { return m_nativeImage; }
    bool hasValidNativeImage(const std::optional<SubsamplingLevel>&, const std::optional<IntSize>& sizeForDrawing) const;
    bool hasDecodedNativeImage() const { return hasNativeImage() && sizeForDrawing(); }
    bool hasMetadata() const { return !size().isEmpty(); }

#if !USE(CG)
    ImageBackingStore* backingStore() const { return m_backingStore ? m_backingStore.get() : nullptr; }
    bool hasBackingStore() const { return backingStore(); }
#endif

    Color singlePixelSolidColor() const;

private:
    Decoding m_decoding { Decoding::None };
    IntSize m_size;

#if !USE(CG)
    std::unique_ptr<ImageBackingStore> m_backingStore;
    DisposalMethod m_disposalMethod { DisposalMethod::Unspecified };
#endif

    NativeImagePtr m_nativeImage;
    SubsamplingLevel m_subsamplingLevel { SubsamplingLevel::Default };
    std::optional<IntSize> m_sizeForDrawing;
    Deque<IntSize, 4> m_sizeForDecoding;

    ImageOrientation m_orientation { DefaultImageOrientation };
    float m_duration { 0 };
    bool m_hasAlpha { true };
};

}
