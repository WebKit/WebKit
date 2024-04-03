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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteResourceCache.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "RemoteImageBuffer.h"
#include <WebCore/ImageBuffer.h>

namespace WebKit {
using namespace WebCore;

void RemoteResourceCache::cacheNativeImage(Ref<NativeImage>&& image)
{
    m_resourceHeap.add(WTFMove(image));
}

void RemoteResourceCache::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    m_resourceHeap.add(WTFMove(decomposedGlyphs));
}

void RemoteResourceCache::cacheGradient(Ref<Gradient>&& gradient)
{
    m_resourceHeap.add(WTFMove(gradient));
}

void RemoteResourceCache::cacheFilter(Ref<Filter>&& filter)
{
    m_resourceHeap.add(WTFMove(filter));
}

RefPtr<NativeImage> RemoteResourceCache::cachedNativeImage(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getNativeImage(renderingResourceIdentifier);
}

void RemoteResourceCache::cacheFont(Ref<Font>&& font)
{
    m_resourceHeap.add(WTFMove(font));
}

RefPtr<Font> RemoteResourceCache::cachedFont(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getFont(renderingResourceIdentifier);
}

void RemoteResourceCache::cacheFontCustomPlatformData(Ref<FontCustomPlatformData>&& customPlatformData)
{
    m_resourceHeap.add(WTFMove(customPlatformData));
}

RefPtr<FontCustomPlatformData> RemoteResourceCache::cachedFontCustomPlatformData(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getFontCustomPlatformData(renderingResourceIdentifier);
}

RefPtr<DecomposedGlyphs> RemoteResourceCache::cachedDecomposedGlyphs(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getDecomposedGlyphs(renderingResourceIdentifier);
}

RefPtr<Gradient> RemoteResourceCache::cachedGradient(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getGradient(renderingResourceIdentifier);
}

RefPtr<Filter> RemoteResourceCache::cachedFilter(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getFilter(renderingResourceIdentifier);
}

void RemoteResourceCache::releaseAllResources()
{
    m_resourceHeap.clearAllResources();
}

void RemoteResourceCache::releaseAllDrawingResources()
{
    m_resourceHeap.clearAllDrawingResources();
}

void RemoteResourceCache::releaseAllImageResources()
{
    m_resourceHeap.clearAllImageResources();
}

bool RemoteResourceCache::releaseRenderingResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (m_resourceHeap.removeImageBuffer(renderingResourceIdentifier)
        || m_resourceHeap.removeRenderingResource(renderingResourceIdentifier)
        || m_resourceHeap.removeFont(renderingResourceIdentifier)
        || m_resourceHeap.removeFontCustomPlatformData(renderingResourceIdentifier))
        return true;

    // Caching the remote resource should have happened before releasing it.
    return false;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
