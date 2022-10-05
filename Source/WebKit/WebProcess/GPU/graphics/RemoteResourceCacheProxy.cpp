/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "RemoteResourceCacheProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "RemoteRenderingBackendProxy.h"

namespace WebKit {
using namespace WebCore;

RemoteResourceCacheProxy::RemoteResourceCacheProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    : m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
{
}

RemoteResourceCacheProxy::~RemoteResourceCacheProxy()
{
    clearNativeImageMap();
    clearImageBufferBackends();
    clearDecomposedGlyphsMap();
}

void RemoteResourceCacheProxy::cacheImageBuffer(RemoteImageBufferProxy& imageBuffer)
{
    auto addResult = m_imageBuffers.add(imageBuffer.renderingResourceIdentifier(), imageBuffer);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

RemoteImageBufferProxy* RemoteResourceCacheProxy::cachedImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_imageBuffers.get(renderingResourceIdentifier).get();
}

void RemoteResourceCacheProxy::releaseImageBuffer(RemoteImageBufferProxy& imageBuffer)
{
    auto identifier = imageBuffer.renderingResourceIdentifier();
    auto iterator = m_imageBuffers.find(identifier);
    RELEASE_ASSERT(iterator != m_imageBuffers.end());

    auto success = m_imageBuffers.remove(iterator);
    ASSERT_UNUSED(success, success);

    m_remoteRenderingBackendProxy.releaseRemoteResource(identifier);
}

inline static RefPtr<ShareableBitmap> createShareableBitmapFromNativeImage(NativeImage& image)
{
    auto imageSize = image.size();

    auto bitmap = ShareableBitmap::create(image.size(), { image.colorSpace() });
    if (!bitmap)
        return nullptr;

    auto context = bitmap->createGraphicsContext();
    if (!context)
        return nullptr;

    context->drawNativeImage(image, imageSize, FloatRect({ }, imageSize), FloatRect({ }, imageSize), { WebCore::CompositeOperator::Copy });
    return bitmap;
}

void RemoteResourceCacheProxy::recordImageBufferUse(WebCore::ImageBuffer& imageBuffer)
{
    auto iterator = m_imageBuffers.find(imageBuffer.renderingResourceIdentifier());
    ASSERT_UNUSED(iterator, iterator != m_imageBuffers.end());
}

void RemoteResourceCacheProxy::recordNativeImageUse(NativeImage& image)
{
    auto iterator = m_nativeImages.find(image.renderingResourceIdentifier());
    if (iterator != m_nativeImages.end())
        return;

    auto bitmap = createShareableBitmapFromNativeImage(image);
    if (!bitmap)
        return;

    ShareableBitmapHandle handle;
    bitmap->createHandle(handle);
    if (handle.isNull())
        return;

    handle.takeOwnershipOfMemory(MemoryLedger::Graphics);
    m_nativeImages.add(image.renderingResourceIdentifier(), image);

    // Set itself as an observer to NativeImage, so releaseNativeImage()
    // gets called when NativeImage is being deleleted.
    image.addObserver(*this);

    // Tell the GPU process to cache this resource.
    m_remoteRenderingBackendProxy.cacheNativeImage(handle, image.renderingResourceIdentifier());
}

void RemoteResourceCacheProxy::recordFontUse(Font& font)
{
    auto result = m_fonts.add(font.renderingResourceIdentifier(), m_renderingUpdateID);

    if (result.isNewEntry) {
        m_remoteRenderingBackendProxy.cacheFont(font);
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
        return;
    }

    auto& currentState = result.iterator->value;
    if (currentState != m_renderingUpdateID) {
        currentState = m_renderingUpdateID;
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
    }
}

void RemoteResourceCacheProxy::recordDecomposedGlyphsUse(DecomposedGlyphs& decomposedGlyphs)
{
    if (m_decomposedGlyphs.add(decomposedGlyphs.renderingResourceIdentifier(), decomposedGlyphs).isNewEntry) {
        decomposedGlyphs.addObserver(*this);
        m_remoteRenderingBackendProxy.cacheDecomposedGlyphs(decomposedGlyphs);
    }
}

void RemoteResourceCacheProxy::releaseNativeImage(RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto iterator = m_nativeImages.find(renderingResourceIdentifier);
    RELEASE_ASSERT(iterator != m_nativeImages.end());

    auto success = m_nativeImages.remove(iterator);
    ASSERT_UNUSED(success, success);

    m_remoteRenderingBackendProxy.releaseRemoteResource(renderingResourceIdentifier);
}

void RemoteResourceCacheProxy::clearNativeImageMap()
{
    for (auto& nativeImage : m_nativeImages.values())
        nativeImage->removeObserver(*this);
    m_nativeImages.clear();
}

void RemoteResourceCacheProxy::prepareForNextRenderingUpdate()
{
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::releaseDecomposedGlyphs(RenderingResourceIdentifier renderingResourceIdentifier)
{
    bool removed = m_decomposedGlyphs.remove(renderingResourceIdentifier);
    RELEASE_ASSERT(removed);
    m_remoteRenderingBackendProxy.releaseRemoteResource(renderingResourceIdentifier);
}

void RemoteResourceCacheProxy::clearFontMap()
{
    m_fonts.clear();
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::clearImageBufferBackends()
{
    // Get a copy of m_imageBuffers.values() because clearBackend()
    // may release some of the cached ImageBuffers.
    for (auto& imageBuffer : copyToVector(m_imageBuffers.values())) {
        if (!imageBuffer)
            continue;
        imageBuffer->clearBackend();
    }
}

void RemoteResourceCacheProxy::clearDecomposedGlyphsMap()
{
    for (auto& decomposedGlyphs : m_decomposedGlyphs.values())
        decomposedGlyphs->removeObserver(*this);
    m_decomposedGlyphs.clear();
}

void RemoteResourceCacheProxy::finalizeRenderingUpdateForFonts()
{
    static constexpr unsigned minimumRenderingUpdateCountToKeepFontAlive = 4;

    unsigned totalFontCount = m_fonts.size();
    RELEASE_ASSERT(m_numberOfFontsUsedInCurrentRenderingUpdate <= totalFontCount);
    if (totalFontCount == m_numberOfFontsUsedInCurrentRenderingUpdate)
        return;

    HashSet<WebCore::RenderingResourceIdentifier> toRemove;
    auto renderingUpdateID = m_renderingUpdateID;
    for (auto& item : m_fonts) {
        if (renderingUpdateID - item.value >= minimumRenderingUpdateCountToKeepFontAlive) {
            toRemove.add(item.key);
            m_remoteRenderingBackendProxy.releaseRemoteResource(item.key);
        }
    }

    m_fonts.removeIf([&](const auto& bucket) {
        return toRemove.contains(bucket.key);
    });
}

void RemoteResourceCacheProxy::didPaintLayers()
{
    finalizeRenderingUpdateForFonts();
    prepareForNextRenderingUpdate();
    m_renderingUpdateID++;
}

void RemoteResourceCacheProxy::remoteResourceCacheWasDestroyed()
{
    clearNativeImageMap();
    clearFontMap();
    clearImageBufferBackends();
    clearDecomposedGlyphsMap();

    for (auto& imageBuffer : m_imageBuffers.values()) {
        if (!imageBuffer)
            continue;
        m_remoteRenderingBackendProxy.createRemoteImageBuffer(*imageBuffer);
    }
}

void RemoteResourceCacheProxy::releaseMemory()
{
    clearNativeImageMap();
    clearFontMap();
    clearDecomposedGlyphsMap();
    m_remoteRenderingBackendProxy.releaseAllRemoteResources();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
