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
#include "Font.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "RenderingResourceIdentifier.h"
#include "SVGFilter.h"
#include "SourceImage.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace DisplayList {

class ResourceHeap {
public:
    virtual ~ResourceHeap() = default;

    virtual ImageBuffer* getImageBuffer(RenderingResourceIdentifier) const = 0;
    virtual NativeImage* getNativeImage(RenderingResourceIdentifier) const = 0;
    virtual std::optional<SourceImage> getSourceImage(RenderingResourceIdentifier) const = 0;
    virtual Font* getFont(RenderingResourceIdentifier) const = 0;
    virtual DecomposedGlyphs* getDecomposedGlyphs(RenderingResourceIdentifier) const = 0;
    virtual Gradient* getGradient(RenderingResourceIdentifier) const = 0;
    virtual SVGFilter* getSVGFilter(RenderingResourceIdentifier) const = 0;
};

class LocalResourceHeap : public ResourceHeap {
public:
    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<ImageBuffer>&& imageBuffer)
    {
        m_resources.add(renderingResourceIdentifier, WTFMove(imageBuffer));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<NativeImage>&& image)
    {
        m_resources.add(renderingResourceIdentifier, WTFMove(image));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<Font>&& font)
    {
        m_resources.add(renderingResourceIdentifier, WTFMove(font));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<DecomposedGlyphs>&& decomposedGlyphs)
    {
        m_resources.add(renderingResourceIdentifier, WTFMove(decomposedGlyphs));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<Gradient>&& gradient)
    {
        m_resources.add(renderingResourceIdentifier, WTFMove(gradient));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<SVGFilter>&& svgFilter)
    {
        m_resources.add(renderingResourceIdentifier, WTFMove(svgFilter));
    }

    ImageBuffer* getImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<ImageBuffer>(renderingResourceIdentifier);
    }

    NativeImage* getNativeImage(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<NativeImage>(renderingResourceIdentifier);
    }

    std::optional<SourceImage> getSourceImage(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        if (!renderingResourceIdentifier)
            return std::nullopt;

        if (auto nativeImage = getNativeImage(renderingResourceIdentifier))
            return { { *nativeImage } };

        if (auto imageBuffer = getImageBuffer(renderingResourceIdentifier))
            return { { *imageBuffer } };

        return std::nullopt;
    }

    Font* getFont(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<Font>(renderingResourceIdentifier);
    }

    DecomposedGlyphs* getDecomposedGlyphs(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<DecomposedGlyphs>(renderingResourceIdentifier);
    }

    Gradient* getGradient(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<Gradient>(renderingResourceIdentifier);
    }

    SVGFilter* getSVGFilter(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<SVGFilter>(renderingResourceIdentifier);
    }

    void clear()
    {
        m_resources.clear();
    }

private:
    template <typename T>
    T* get(RenderingResourceIdentifier renderingResourceIdentifier) const
    {
        auto iterator = m_resources.find(renderingResourceIdentifier);
        if (iterator == m_resources.end())
            return nullptr;
        ASSERT(std::holds_alternative<Ref<T>>(iterator->value));
        return std::get<Ref<T>>(iterator->value).ptr();
    }

    using Resource = std::variant<
        std::monostate,
        Ref<ImageBuffer>,
        Ref<NativeImage>,
        Ref<Font>,
        Ref<DecomposedGlyphs>,
        Ref<Gradient>,
        Ref<SVGFilter>
    >;

    HashMap<RenderingResourceIdentifier, Resource> m_resources;
};

} // namespace DisplayList

} // namespace WebCore
