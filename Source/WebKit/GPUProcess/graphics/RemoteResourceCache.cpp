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

#include "RemoteImageBuffer.h"

namespace WebKit {
using namespace WebCore;

RemoteResourceCache::RemoteResourceCache(ProcessIdentifier webProcessIdentifier)
    : m_resourceHeap(webProcessIdentifier)
{
}

void RemoteResourceCache::cacheImageBuffer(Ref<RemoteImageBuffer>&& imageBuffer, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(renderingResourceIdentifier.object() == imageBuffer->renderingResourceIdentifier());
    Ref<ImageBuffer> buffer = WTFMove(imageBuffer);
    m_resourceHeap.add(renderingResourceIdentifier, WTFMove(buffer));
}

RemoteImageBuffer* RemoteResourceCache::cachedImageBuffer(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return static_cast<RemoteImageBuffer*>(m_resourceHeap.getImageBuffer(renderingResourceIdentifier)); 
}

RefPtr<RemoteImageBuffer> RemoteResourceCache::takeImageBuffer(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    RefPtr<RemoteImageBuffer> buffer = cachedImageBuffer(renderingResourceIdentifier);
    m_resourceHeap.removeImageBuffer(renderingResourceIdentifier);
    ASSERT(buffer->hasOneRef());
    return buffer;
}

void RemoteResourceCache::cacheNativeImage(Ref<NativeImage>&& image, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(renderingResourceIdentifier.object() == image->renderingResourceIdentifier());
    m_resourceHeap.add(renderingResourceIdentifier, WTFMove(image));
}

void RemoteResourceCache::cacheDecomposedGlyphs(Ref<DecomposedGlyphs>&& decomposedGlyphs, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(renderingResourceIdentifier.object() == decomposedGlyphs->renderingResourceIdentifier());
    m_resourceHeap.add(renderingResourceIdentifier, WTFMove(decomposedGlyphs));
}

void RemoteResourceCache::cacheGradient(Ref<Gradient>&& gradient, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    m_resourceHeap.add(renderingResourceIdentifier, WTFMove(gradient));
}

void RemoteResourceCache::cacheFilter(Ref<Filter>&& filter, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    m_resourceHeap.add(renderingResourceIdentifier, WTFMove(filter));
}

NativeImage* RemoteResourceCache::cachedNativeImage(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getNativeImage(renderingResourceIdentifier);
}

std::optional<WebCore::SourceImage> RemoteResourceCache::cachedSourceImage(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getSourceImage(renderingResourceIdentifier);
}

void RemoteResourceCache::cacheFont(Ref<Font>&& font, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(renderingResourceIdentifier.object() == font->renderingResourceIdentifier());
    m_resourceHeap.add(renderingResourceIdentifier, WTFMove(font));
}

Font* RemoteResourceCache::cachedFont(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getFont(renderingResourceIdentifier);
}

void RemoteResourceCache::cacheFontCustomPlatformData(Ref<FontCustomPlatformData>&& font, QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
{
    ASSERT(renderingResourceIdentifier.object() == font->m_renderingResourceIdentifier);
    m_resourceHeap.add(renderingResourceIdentifier, WTFMove(font));
}

FontCustomPlatformData* RemoteResourceCache::cachedFontCustomPlatformData(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getFontCustomPlatformData(renderingResourceIdentifier);
}

DecomposedGlyphs* RemoteResourceCache::cachedDecomposedGlyphs(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getDecomposedGlyphs(renderingResourceIdentifier);
}

Gradient* RemoteResourceCache::cachedGradient(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getGradient(renderingResourceIdentifier);
}

Filter* RemoteResourceCache::cachedFilter(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_resourceHeap.getFilter(renderingResourceIdentifier);
}

void RemoteResourceCache::releaseAllResources()
{
    m_resourceHeap.releaseAllResources();
}

void RemoteResourceCache::releaseAllImageResources()
{
    m_resourceHeap.releaseAllImageResources();
}

bool RemoteResourceCache::releaseRenderingResource(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
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
