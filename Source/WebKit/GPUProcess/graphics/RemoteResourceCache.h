/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <WebCore/RenderingResourceIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DecomposedGlyphs;
class Filter;
class Font;
class Gradient;
class ImageBuffer;
class NativeImage;
struct FontCustomPlatformData;

}

namespace WebKit {

class RemoteResourceCache {
public:
    RemoteResourceCache();
    ~RemoteResourceCache();

    void cacheNativeImage(Ref<WebCore::NativeImage>&&);
    bool releaseNativeImage(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::NativeImage> cachedNativeImage(WebCore::RenderingResourceIdentifier) const;

    bool cacheGradient(WebCore::RenderingResourceIdentifier, Ref<WebCore::Gradient>&&);
    bool releaseGradient(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::Gradient> cachedGradient(WebCore::RenderingResourceIdentifier) const;

    void cacheDecomposedGlyphs(Ref<WebCore::DecomposedGlyphs>&&);
    bool releaseDecomposedGlyphs(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::DecomposedGlyphs> cachedDecomposedGlyphs(WebCore::RenderingResourceIdentifier) const;

    void cacheFilter(Ref<WebCore::Filter>&&);
    bool releaseFilter(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::Filter> cachedFilter(WebCore::RenderingResourceIdentifier) const;

    void cacheFont(Ref<WebCore::Font>&&);
    bool releaseFont(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::Font> cachedFont(WebCore::RenderingResourceIdentifier) const;

    void cacheFontCustomPlatformData(Ref<WebCore::FontCustomPlatformData>&&);
    bool releaseFontCustomPlatformData(WebCore::RenderingResourceIdentifier);
    RefPtr<WebCore::FontCustomPlatformData> cachedFontCustomPlatformData(WebCore::RenderingResourceIdentifier) const;

    void releaseAllResources();
    void releaseMemory();
    void releaseNativeImages();

private:
    HashMap<WebCore::RenderingResourceIdentifier, Ref<WebCore::ImageBuffer>> m_imageBuffers;
    HashMap<WebCore::RenderingResourceIdentifier, Ref<WebCore::NativeImage>> m_nativeImages;
    HashMap<WebCore::RenderingResourceIdentifier, Ref<WebCore::Gradient>> m_gradients;
    HashMap<WebCore::RenderingResourceIdentifier, Ref<WebCore::DecomposedGlyphs>> m_decomposedGlyphs;
    HashMap<WebCore::RenderingResourceIdentifier, Ref<WebCore::Filter>> m_filters;
    HashMap<WebCore::RenderingResourceIdentifier, Ref<WebCore::Font>> m_fonts;
    HashMap<WebCore::RenderingResourceIdentifier, Ref<WebCore::FontCustomPlatformData>> m_fontCustomPlatformDatas;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
