/*
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "FEImageSoftwareApplier.h"

#include "FEImage.h"
#include "Filter.h"
#include "GraphicsContext.h"

namespace WebCore {

bool FEImageSoftwareApplier::apply(const Filter& filter, const FilterImageVector&, FilterImage& result) const
{
    auto resultImage = result.imageBuffer();
    if (!resultImage)
        return false;

    auto& sourceImage = m_effect.sourceImage();
    auto primitiveSubregion = result.primitiveSubregion();
    auto& context = resultImage->context();

    if (auto nativeImage = sourceImage.nativeImageIfExists()) {
        auto imageRect = primitiveSubregion;
        auto srcRect = m_effect.sourceImageRect();
        m_effect.preserveAspectRatio().transformRect(imageRect, srcRect);
        imageRect.scale(filter.filterScale());
        imageRect = IntRect(imageRect) - result.absoluteImageRect().location();
        context.drawNativeImage(*nativeImage, srcRect.size(), imageRect, srcRect);
        return true;
    }

    if (auto imageBuffer = sourceImage.imageBufferIfExists()) {
        auto imageRect = primitiveSubregion;
        imageRect.moveBy(m_effect.sourceImageRect().location());
        imageRect.scale(filter.filterScale());
        imageRect = IntRect(imageRect) - result.absoluteImageRect().location();
        context.drawImageBuffer(*imageBuffer, imageRect.location());
        return true;
    }
    
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
