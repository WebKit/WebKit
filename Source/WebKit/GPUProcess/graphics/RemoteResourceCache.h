/*
 * Copyright (C) 2020-2023 Apple Inc.  All rights reserved.
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

#include <WebCore/DisplayListResourceHeap.h>
#include <WebCore/RenderingResourceIdentifier.h>

namespace WebCore {
class ImageBuffer;
}
namespace WebKit {

class RemoteRenderingBackend;

class RemoteResourceCache {
public:
    RemoteResourceCache() = default;

    void cacheNativeImage(Ref<WebCore::NativeImage>&&);
    void cacheFont(Ref<WebCore::Font>&&);
    void cacheDecomposedGlyphs(Ref<WebCore::DecomposedGlyphs>&&);
    void cacheGradient(Ref<WebCore::Gradient>&&);
    void cacheFilter(Ref<WebCore::Filter>&&);
    void cacheFontCustomPlatformData(Ref<WebCore::FontCustomPlatformData>&&);

    const WebCore::DisplayList::ResourceHeap& resourceHeap() const { return m_resourceHeap; }

    RefPtr<WebCore::NativeImage> cachedNativeImage(WebCore::RenderingResourceIdentifier) const;
    RefPtr<WebCore::Font> cachedFont(WebCore::RenderingResourceIdentifier) const;
    RefPtr<WebCore::DecomposedGlyphs> cachedDecomposedGlyphs(WebCore::RenderingResourceIdentifier) const;
    RefPtr<WebCore::Gradient> cachedGradient(WebCore::RenderingResourceIdentifier) const;
    RefPtr<WebCore::Filter> cachedFilter(WebCore::RenderingResourceIdentifier) const;
    RefPtr<WebCore::FontCustomPlatformData> cachedFontCustomPlatformData(WebCore::RenderingResourceIdentifier) const;

    void releaseAllResources();
    void releaseAllDrawingResources();
    void releaseAllImageResources();
    bool releaseRenderingResource(WebCore::RenderingResourceIdentifier);

private:
    WebCore::DisplayList::ResourceHeap m_resourceHeap;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
