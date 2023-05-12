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
#include "Filter.h"
#include "Font.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "RenderingResourceIdentifier.h"
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
    virtual DecomposedGlyphs* getDecomposedGlyphs(RenderingResourceIdentifier) const = 0;
    virtual Gradient* getGradient(RenderingResourceIdentifier) const = 0;
    virtual Filter* getFilter(RenderingResourceIdentifier) const = 0;
    virtual Font* getFont(RenderingResourceIdentifier) const = 0;
};

class LocalResourceHeap : public ResourceHeap {
public:
    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<ImageBuffer>&& imageBuffer)
    {
        add<ImageBuffer>(renderingResourceIdentifier, WTFMove(imageBuffer));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<NativeImage>&& image)
    {
        add<RenderingResource>(renderingResourceIdentifier, WTFMove(image));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<DecomposedGlyphs>&& decomposedGlyphs)
    {
        add<RenderingResource>(renderingResourceIdentifier, WTFMove(decomposedGlyphs));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<Gradient>&& gradient)
    {
        add<RenderingResource>(renderingResourceIdentifier, WTFMove(gradient));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<Filter>&& filter)
    {
        add<RenderingResource>(renderingResourceIdentifier, WTFMove(filter));
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<Font>&& font)
    {
        add<Font>(renderingResourceIdentifier, WTFMove(font));
    }

    ImageBuffer* getImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<ImageBuffer>(renderingResourceIdentifier);
    }

    NativeImage* getNativeImage(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
        return dynamicDowncast<NativeImage>(renderingResource);
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

    DecomposedGlyphs* getDecomposedGlyphs(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
        return dynamicDowncast<DecomposedGlyphs>(renderingResource);
    }

    Gradient* getGradient(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
        return dynamicDowncast<Gradient>(renderingResource);
    }

    Filter* getFilter(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
        return dynamicDowncast<Filter>(renderingResource);
    }

    Font* getFont(RenderingResourceIdentifier renderingResourceIdentifier) const final
    {
        return get<Font>(renderingResourceIdentifier);
    }

    void clear()
    {
        m_resources.clear();
    }

private:
    template <typename T>
    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<T>&& object)
    {
        m_resources.add(renderingResourceIdentifier, WTFMove(object));
    }

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
        Ref<RenderingResource>,
        Ref<Font>
    >;

    HashMap<RenderingResourceIdentifier, Resource> m_resources;
};

} // namespace DisplayList

} // namespace WebCore
