/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "FloatSize.h"
#include "GeneratedImage.h"
#include "Image.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CrossfadeGeneratedImage final : public GeneratedImage {
public:
    static Ref<CrossfadeGeneratedImage> create(Image& fromImage, Image& toImage, float percentage, const FloatSize& crossfadeSize, const FloatSize& size)
    {
        return adoptRef(*new CrossfadeGeneratedImage(fromImage, toImage, percentage, crossfadeSize, size));
    }

    void setContainerSize(const FloatSize&) override { }
    bool usesContainerSize() const override { return false; }
    bool hasRelativeWidth() const override { return false; }
    bool hasRelativeHeight() const override { return false; }

    FloatSize size(ImageOrientation = ImageOrientation::FromImage) const override { return m_crossfadeSize; }

private:
    ImageDrawResult draw(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, const ImagePaintingOptions& = { }) override;
    void drawPattern(GraphicsContext&, const FloatRect& dstRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, const ImagePaintingOptions& = { }) override;

    CrossfadeGeneratedImage(Image& fromImage, Image& toImage, float percentage, const FloatSize& crossfadeSize, const FloatSize&);

    bool isCrossfadeGeneratedImage() const override { return true; }
    void dump(WTF::TextStream&) const override;
    
    void drawCrossfade(GraphicsContext&);

    Ref<Image> m_fromImage;
    Ref<Image> m_toImage;

    float m_percentage;
    FloatSize m_crossfadeSize;
};

}

SPECIALIZE_TYPE_TRAITS_IMAGE(CrossfadeGeneratedImage)
