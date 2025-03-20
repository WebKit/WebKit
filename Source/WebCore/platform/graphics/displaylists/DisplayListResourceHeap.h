/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "DecomposedGlyphs.h"
#include "DisplayListItem.h"
#include "Filter.h"
#include "Font.h"
#include "FontCustomPlatformData.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "RenderingResourceIdentifier.h"
#include "SourceImage.h"
#include <wtf/HashMap.h>

namespace WebCore {
namespace DisplayList {

class ResourceHeap {
public:
    void add(Ref<ImageBuffer>&& imageBuffer)
    {
        auto identifier = imageBuffer->renderingResourceIdentifier();
        m_imageBuffers.add(identifier, WTFMove(imageBuffer));
    }

    void add(Ref<NativeImage>&& image)
    {
        auto identifier = image->renderingResourceIdentifier();
        m_nativeImages.add(identifier, WTFMove(image));
    }

    void add(Ref<DecomposedGlyphs>&& decomposedGlyphs)
    {
        auto identifier = decomposedGlyphs->renderingResourceIdentifier();
        m_decomposedGlyphs.add(identifier, WTFMove(decomposedGlyphs));
    }

    void add(Ref<Gradient>&& gradient)
    {
        auto identifier = gradient->renderingResourceIdentifier();
        m_gradients.add(identifier, WTFMove(gradient));
    }

    void add(Ref<Filter>&& filter)
    {
        auto identifier = filter->renderingResourceIdentifier();
        m_filters.add(identifier, WTFMove(filter));
    }

    void add(Ref<Font>&& font)
    {
        auto identifier = font->renderingResourceIdentifier();
        m_fonts.add(identifier, WTFMove(font));
    }

    RefPtr<ImageBuffer> getImageBuffer(RenderingResourceIdentifier identifier, OptionSet<ReplayOption> options = { }) const
    {
        RefPtr imageBuffer = m_imageBuffers.get(identifier);

#if USE(SKIA)
        if (imageBuffer && options.contains(ReplayOption::FlushAcceleratedImagesAndWaitForCompletion))
            imageBuffer->waitForAcceleratedRenderingFenceCompletion();
#else
        UNUSED_PARAM(options);
#endif

        return imageBuffer;
    }

    RefPtr<NativeImage> getNativeImage(RenderingResourceIdentifier identifier, OptionSet<ReplayOption> options = { }) const
    {
        RefPtr nativeImage = m_nativeImages.get(identifier);

#if USE(SKIA)
        if (nativeImage && options.contains(ReplayOption::FlushAcceleratedImagesAndWaitForCompletion))
            nativeImage->backend().waitForAcceleratedRenderingFenceCompletion();
#else
        UNUSED_PARAM(options);
#endif

        return nativeImage;
    }

    std::optional<SourceImage> getSourceImage(RenderingResourceIdentifier identifier, OptionSet<ReplayOption> options = { }) const
    {
        if (RefPtr nativeImage = getNativeImage(identifier, options))
            return { { *nativeImage } };

        if (RefPtr imageBuffer = getImageBuffer(identifier, options))
            return { { *imageBuffer } };

        return std::nullopt;
    }

    RefPtr<DecomposedGlyphs> getDecomposedGlyphs(RenderingResourceIdentifier identifier) const
    {
        return m_decomposedGlyphs.get(identifier);
    }

    RefPtr<Gradient> getGradient(RenderingResourceIdentifier identifier) const
    {
        return m_gradients.get(identifier);
    }

    RefPtr<Filter> getFilter(RenderingResourceIdentifier identifier) const
    {
        return m_filters.get(identifier);
    }

    Font* getFont(RenderingResourceIdentifier identifier) const
    {
        return m_fonts.get(identifier);
    }

    void clearAllResources()
    {
        m_imageBuffers.clear();
        m_nativeImages.clear();
        m_gradients.clear();
        m_decomposedGlyphs.clear();
        m_filters.clear();
        m_fonts.clear();
    }

private:
    UncheckedKeyHashMap<RenderingResourceIdentifier, Ref<WebCore::ImageBuffer>> m_imageBuffers;
    UncheckedKeyHashMap<RenderingResourceIdentifier, Ref<WebCore::NativeImage>> m_nativeImages;
    UncheckedKeyHashMap<RenderingResourceIdentifier, Ref<WebCore::Gradient>> m_gradients;
    UncheckedKeyHashMap<RenderingResourceIdentifier, Ref<WebCore::DecomposedGlyphs>> m_decomposedGlyphs;
    UncheckedKeyHashMap<RenderingResourceIdentifier, Ref<WebCore::Filter>> m_filters;
    UncheckedKeyHashMap<RenderingResourceIdentifier, Ref<WebCore::Font>> m_fonts;
};

} // namespace DisplayList
} // namespace WebCore
