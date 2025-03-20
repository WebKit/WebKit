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
#include "RemoteResourceCacheProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "Logging.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteNativeImageBackendProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "WebProcess.h"
#include <WebCore/FontCustomPlatformData.h>

namespace WebKit {
using namespace WebCore;

RemoteResourceCacheProxy::RemoteResourceCacheProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    : m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
{
}

RemoteResourceCacheProxy::~RemoteResourceCacheProxy()
{
    clearImageBufferBackends();
}

void RemoteResourceCacheProxy::clear()
{
    clearImageBufferBackends();
    m_imageBuffers.clear();
    clearRenderingResources();
}

void RemoteResourceCacheProxy::cacheImageBuffer(RemoteImageBufferProxy& imageBuffer)
{
    auto addResult = m_imageBuffers.add(imageBuffer.renderingResourceIdentifier(), imageBuffer);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

RefPtr<RemoteImageBufferProxy> RemoteResourceCacheProxy::cachedImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return m_imageBuffers.get(renderingResourceIdentifier).get();
}

void RemoteResourceCacheProxy::forgetImageBuffer(RenderingResourceIdentifier identifier)
{
    auto iterator = m_imageBuffers.find(identifier);
    RELEASE_ASSERT(iterator != m_imageBuffers.end());

    auto success = m_imageBuffers.remove(iterator);
    ASSERT_UNUSED(success, success);
}

void RemoteResourceCacheProxy::recordImageBufferUse(WebCore::ImageBuffer& imageBuffer)
{
    auto iterator = m_imageBuffers.find(imageBuffer.renderingResourceIdentifier());
    ASSERT_UNUSED(iterator, iterator != m_imageBuffers.end());
}

void RemoteResourceCacheProxy::recordDecomposedGlyphsUse(DecomposedGlyphs& decomposedGlyphs)
{
    if (m_decomposedGlyphs.add(decomposedGlyphs.renderingResourceIdentifier()).isNewEntry) {
        decomposedGlyphs.addObserver(m_resourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());
        m_remoteRenderingBackendProxy->cacheDecomposedGlyphs(decomposedGlyphs);
    }
}

void RemoteResourceCacheProxy::recordGradientUse(Gradient& gradient)
{
    if (m_gradients.add(gradient.renderingResourceIdentifier()).isNewEntry) {
        gradient.addObserver(m_resourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());
        m_remoteRenderingBackendProxy->cacheGradient(gradient);
    }
}

void RemoteResourceCacheProxy::recordFilterUse(Filter& filter)
{
    if (m_filters.add(filter.renderingResourceIdentifier()).isNewEntry) {
        filter.addObserver(m_resourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());
        m_remoteRenderingBackendProxy->cacheFilter(filter);
    }
}

void RemoteResourceCacheProxy::recordNativeImageUse(NativeImage& image)
{
    if (isMainRunLoop())
        WebProcess::singleton().deferNonVisibleProcessEarlyMemoryCleanupTimer();
    auto identifier = image.renderingResourceIdentifier();
    if (m_nativeImages.contains(identifier))
        return;

    RemoteNativeImageBackendProxy* backend = dynamicDowncast<RemoteNativeImageBackendProxy>(image.backend());
    std::unique_ptr<RemoteNativeImageBackendProxy> newBackend;
    if (!backend) {
        newBackend = RemoteNativeImageBackendProxy::create(image);
        backend = newBackend.get();
    }
    std::optional<ShareableBitmap::Handle> handle;
    if (backend)
        handle = backend->createHandle();
    if (!handle) {
        // FIXME: Failing to send the image to GPUP will crash it when referencing this image.
        LOG_WITH_STREAM(Images, stream
            << "RemoteResourceCacheProxy::recordNativeImageUse() " << this
            << " image.size(): " << image.size()
            << " image.colorSpace(): " << image.colorSpace()
            << " ShareableBitmap could not be created; bailing.");
        return;
    }
    m_nativeImages.add(identifier);
    // Set itself as an observer to NativeImage, so releaseNativeImage()
    // gets called when NativeImage is being deleleted.
    image.addObserver(m_nativeImageResourceObserverWeakFactory.createWeakPtr(static_cast<RenderingResourceObserver&>(*this)).releaseNonNull());

    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);
    // Replace the contents of the original NativeImage to save memory.
    if (newBackend)
        image.replaceBackend(makeUniqueRefFromNonNullUniquePtr(WTFMove(newBackend)));

    // Tell the GPU process to cache this resource.
    m_remoteRenderingBackendProxy->cacheNativeImage(WTFMove(*handle), identifier);
}

void RemoteResourceCacheProxy::recordFontUse(Font& font)
{
    if (RefPtr platformData = font.platformData().customPlatformData())
        recordFontCustomPlatformDataUse(*platformData);

    auto result = m_fonts.add(font.renderingResourceIdentifier(), m_renderingUpdateID);

    if (result.isNewEntry) {
        auto renderingResourceIdentifier = font.platformData().customPlatformData() ? std::optional(font.platformData().customPlatformData()->m_renderingResourceIdentifier) : std::nullopt;
        m_remoteRenderingBackendProxy->cacheFont(font.attributes(), font.platformData().attributes(), renderingResourceIdentifier);
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
        return;
    }

    auto& currentState = result.iterator->value;
    if (currentState != m_renderingUpdateID) {
        currentState = m_renderingUpdateID;
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
    }
}

void RemoteResourceCacheProxy::recordFontCustomPlatformDataUse(const FontCustomPlatformData& customPlatformData)
{
    auto result = m_fontCustomPlatformDatas.add(customPlatformData.m_renderingResourceIdentifier, m_renderingUpdateID);

    if (result.isNewEntry) {
        m_remoteRenderingBackendProxy->cacheFontCustomPlatformData(customPlatformData);
        ++m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate;
        return;
    }

    auto& currentState = result.iterator->value;
    if (currentState != m_renderingUpdateID) {
        currentState = m_renderingUpdateID;
        ++m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate;
    }
}


void RemoteResourceCacheProxy::willDestroyNativeImage(RenderingResourceIdentifier identifier)
{
    bool removed = m_nativeImages.remove(identifier);
    RELEASE_ASSERT(removed);
    m_remoteRenderingBackendProxy->releaseNativeImage(identifier);
}

void RemoteResourceCacheProxy::willDestroyGradient(RenderingResourceIdentifier identifier)
{
    bool removed = m_gradients.remove(identifier);
    RELEASE_ASSERT(removed);
    m_remoteRenderingBackendProxy->releaseGradient(identifier);
}

void RemoteResourceCacheProxy::willDestroyDecomposedGlyphs(RenderingResourceIdentifier identifier)
{
    bool removed = m_decomposedGlyphs.remove(identifier);
    RELEASE_ASSERT(removed);
    m_remoteRenderingBackendProxy->releaseDecomposedGlyphs(identifier);
}

void RemoteResourceCacheProxy::willDestroyFilter(RenderingResourceIdentifier identifier)
{
    bool removed = m_filters.remove(identifier);
    RELEASE_ASSERT(removed);
    m_remoteRenderingBackendProxy->releaseFilter(identifier);
}

void RemoteResourceCacheProxy::clearRenderingResources()
{
    m_resourceObserverWeakFactory.revokeAll();
    m_decomposedGlyphs.clear();
    m_filters.clear();
    m_gradients.clear();
    clearNativeImages();
}

void RemoteResourceCacheProxy::clearNativeImages()
{
    m_nativeImageResourceObserverWeakFactory.revokeAll();
    m_nativeImages.clear();
}

void RemoteResourceCacheProxy::prepareForNextRenderingUpdate()
{
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
    m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::clearFontMap()
{
    m_fonts.clear();
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::clearFontCustomPlatformDataMap()
{
    m_fontCustomPlatformDatas.clear();
    m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::clearImageBufferBackends()
{
    // Get a copy of m_imageBuffers.values() because clearBackend()
    // may release some of the cached ImageBuffers.
    for (auto& weakImageBuffer : copyToVector(m_imageBuffers.values())) {
        auto protectedImageBuffer = weakImageBuffer.get();
        if (!protectedImageBuffer)
            continue;
        protectedImageBuffer->clearBackend();
    }
}

void RemoteResourceCacheProxy::finalizeRenderingUpdateForFonts()
{
    static constexpr unsigned minimumRenderingUpdateCountToKeepFontAlive = 4;

    unsigned totalFontCount = m_fonts.size();
    RELEASE_ASSERT(m_numberOfFontsUsedInCurrentRenderingUpdate <= totalFontCount);
    if (totalFontCount != m_numberOfFontsUsedInCurrentRenderingUpdate) {
        HashSet<WebCore::RenderingResourceIdentifier> toRemove;
        auto renderingUpdateID = m_renderingUpdateID;
        for (auto& item : m_fonts) {
            if (renderingUpdateID - item.value >= minimumRenderingUpdateCountToKeepFontAlive) {
                toRemove.add(item.key);
                m_remoteRenderingBackendProxy->releaseFont(item.key);
            }
        }

        m_fonts.removeIf([&](const auto& bucket) {
            return toRemove.contains(bucket.key);
        });
    }

    totalFontCount = m_fontCustomPlatformDatas.size();
    RELEASE_ASSERT(m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate <= totalFontCount);
    if (totalFontCount != m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate) {
        HashSet<WebCore::RenderingResourceIdentifier> toRemove;
        auto renderingUpdateID = m_renderingUpdateID;
        for (auto& item : m_fontCustomPlatformDatas) {
            if (renderingUpdateID - item.value >= minimumRenderingUpdateCountToKeepFontAlive) {
                toRemove.add(item.key);
                m_remoteRenderingBackendProxy->releaseFontCustomPlatformData(item.key);
            }
        }

        m_fontCustomPlatformDatas.removeIf([&](const auto& bucket) {
            return toRemove.contains(bucket.key);
        });
    }
}

void RemoteResourceCacheProxy::didPaintLayers()
{
    finalizeRenderingUpdateForFonts();
    prepareForNextRenderingUpdate();
    m_renderingUpdateID++;
}

void RemoteResourceCacheProxy::remoteResourceCacheWasDestroyed()
{
    clearImageBufferBackends();

    for (auto& weakImageBuffer : m_imageBuffers.values()) {
        auto protectedImageBuffer = weakImageBuffer.get();
        if (!protectedImageBuffer)
            continue;
        m_remoteRenderingBackendProxy->createRemoteImageBuffer(*protectedImageBuffer);
    }

    clearRenderingResources();
    clearFontMap();
    clearFontCustomPlatformDataMap();
}

void RemoteResourceCacheProxy::releaseMemory()
{
    clearRenderingResources();
    clearFontMap();
    clearFontCustomPlatformDataMap();

    m_remoteRenderingBackendProxy->releaseAllDrawingResources();
}

void RemoteResourceCacheProxy::releaseAllImageResources()
{
    clearNativeImages();
    m_remoteRenderingBackendProxy->releaseAllImageResources();
}


} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
