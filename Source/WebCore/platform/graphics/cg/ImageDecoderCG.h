/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "ImageSourceCG.h"
#include "IntSize.h"

#include <wtf/Optional.h>

typedef struct CGImageSource* CGImageSourceRef;
typedef const struct __CFData* CFDataRef;

namespace WebCore {

class ImageDecoder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ImageDecoder(AlphaOption, GammaAndColorProfileOption);

    static std::unique_ptr<ImageDecoder> create(const SharedBuffer&, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    {
        return std::make_unique<ImageDecoder>(alphaOption, gammaAndColorProfileOption);
    }
    
    static size_t bytesDecodedToDetermineProperties();

    bool isSizeAvailable() const;
    size_t frameCount() const;
    RepetitionCount repetitionCount() const;
    String filenameExtension() const;
    std::optional<IntPoint> hotSpot() const;

    IntSize frameSizeAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const;
    bool frameIsCompleteAtIndex(size_t) const;
    ImageOrientation frameOrientationAtIndex(size_t) const;
    
    float frameDurationAtIndex(size_t) const;
    bool frameHasAlphaAtIndex(size_t) const;
    bool frameAllowSubsamplingAtIndex(size_t) const;
    unsigned frameBytesAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default) const;
    
    NativeImagePtr createFrameImageAtIndex(size_t, SubsamplingLevel = SubsamplingLevel::Default, const std::optional<IntSize>& sizeForDrawing = { }) const;
    
    void setData(SharedBuffer&, bool allDataReceived);
    bool isAllDataReceived() const { return m_isAllDataReceived; }
    void clearFrameBufferCache(size_t) { }
    
protected:
    bool m_isAllDataReceived { false };
    RetainPtr<CGImageSourceRef> m_nativeDecoder;
};

}
