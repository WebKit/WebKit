/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
#include "ImageDrawingExtras.h"
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class CrossfadeGeneratedImage final : public GeneratedImage {
public:
    static Ref<CrossfadeGeneratedImage> create(Image& fromImage, Image& toImage, float percentage, const FloatSize& crossfadeSize, std::unique_ptr<ImageDrawingExtras> fromExtras = nullptr, std::unique_ptr<ImageDrawingExtras> toExtras = nullptr)
    {
        return adoptRef(*new CrossfadeGeneratedImage(fromImage, toImage, percentage, crossfadeSize, WTF::move(fromExtras), WTF::move(toExtras)));
    }

    NaturalDimensions unorientedNaturalDimensions() const final
    {
        if (constructedSize().isEmpty())
            return NaturalDimensions::none();
        return NaturalDimensions::fixed(constructedSize());
    }

private:
    ImageDrawResult draw(GraphicsContext&, ConcreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr) override;
    void drawPattern(GraphicsContext&, ConcreteObjectSize, const FloatRect& dstRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions = { }, const ImageDrawingExtras* = nullptr) override;

    CrossfadeGeneratedImage(Image& fromImage, Image& toImage, float percentage, const FloatSize& crossfadeSize, std::unique_ptr<ImageDrawingExtras> fromExtras, std::unique_ptr<ImageDrawingExtras> toExtras);

    bool isCrossfadeGeneratedImage() const override { return true; }
    void dump(WTF::TextStream&) const override;
    
    void drawCrossfade(GraphicsContext&);

    const Ref<Image> m_fromImage;
    const Ref<Image> m_toImage;

    const std::unique_ptr<ImageDrawingExtras> m_fromExtras;
    const std::unique_ptr<ImageDrawingExtras> m_toExtras;

    float m_percentage;
};

}

SPECIALIZE_TYPE_TRAITS_IMAGE(CrossfadeGeneratedImage)
