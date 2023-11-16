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

#include "Font.h"
#include "FontCustomPlatformData.h"
#include "ImageBuffer.h"
#include "RenderingResource.h"
#include <wtf/HashMap.h>

namespace WebCore {

class DecomposedGlyphs;
class Filter;
class Gradient;
class NativeImage;
class SourceImage;

namespace DisplayList {

class DisplayList;

class ResourceHeap {
public:
    using Resource = std::variant<
        std::monostate,
        Ref<ImageBuffer>,
        Ref<RenderingResource>,
        Ref<Font>,
        Ref<FontCustomPlatformData>
    >;

    ResourceHeap() = default;
    WEBCORE_EXPORT ResourceHeap(const ResourceHeap& other, const Vector<RenderingResourceIdentifier>&);

    WEBCORE_EXPORT void add(Ref<ImageBuffer>&&);
    WEBCORE_EXPORT void add(Ref<RenderingResource>&&);
    WEBCORE_EXPORT void add(Ref<NativeImage>&&);
    WEBCORE_EXPORT void add(Ref<DecomposedGlyphs>&&);
    WEBCORE_EXPORT void add(Ref<Gradient>&&);
    WEBCORE_EXPORT void add(Ref<Filter>&&);
    WEBCORE_EXPORT void add(Ref<DisplayList>&&);
    WEBCORE_EXPORT void add(Ref<Font>&&);
    WEBCORE_EXPORT void add(Ref<FontCustomPlatformData>&&);

    WEBCORE_EXPORT ImageBuffer* getImageBuffer(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT NativeImage* getNativeImage(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT std::optional<SourceImage> getSourceImage(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT DecomposedGlyphs* getDecomposedGlyphs(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT Gradient* getGradient(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT Filter* getFilter(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT DisplayList* getDisplayList(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT Font* getFont(RenderingResourceIdentifier) const;
    WEBCORE_EXPORT FontCustomPlatformData* getFontCustomPlatformData(RenderingResourceIdentifier) const;

    WEBCORE_EXPORT bool removeImageBuffer(RenderingResourceIdentifier);
    WEBCORE_EXPORT bool removeRenderingResource(RenderingResourceIdentifier);
    WEBCORE_EXPORT bool removeFont(RenderingResourceIdentifier);
    WEBCORE_EXPORT bool removeFontCustomPlatformData(RenderingResourceIdentifier);

    WEBCORE_EXPORT void clearAllResources();
    WEBCORE_EXPORT void clearAllImageResources();
    WEBCORE_EXPORT void clearAllDrawingResources();

    auto resourceIdentifiers() const { return m_resources.keys(); }
    auto resources() const { return m_resources.values(); }

private:
    template <typename T>
    void add(RenderingResourceIdentifier, Ref<T>&&, unsigned& counter);

    template <typename T>
    T* get(RenderingResourceIdentifier) const;

    template <typename T>
    bool remove(RenderingResourceIdentifier, unsigned& counter);

    void checkInvariants() const;

    HashMap<RenderingResourceIdentifier, Resource> m_resources;
    unsigned m_imageBufferCount { 0 };
    unsigned m_renderingResourceCount { 0 };
    unsigned m_fontCount { 0 };
    unsigned m_customPlatformDataCount { 0 };
};

} // namespace DisplayList
} // namespace WebCore
