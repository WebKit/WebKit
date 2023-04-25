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
#include "RemoteRenderingBackendProxy.h"
#include <WebCore/FontCustomPlatformData.h>

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
    clearGradientMap();
}

void RemoteResourceCacheProxy::clear()
{
    clearNativeImageMap();
    clearImageBufferBackends();
    m_imageBuffers.clear();
    clearDecomposedGlyphsMap();
    clearGradientMap();
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
    forgetImageBuffer(imageBuffer.renderingResourceIdentifier());

    m_remoteRenderingBackendProxy.releaseRenderingResource(imageBuffer.renderingResourceIdentifier());
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

inline static std::optional<ShareableBitmap::Handle> createShareableBitmapFromNativeImage(NativeImage& image)
{
    RefPtr<ShareableBitmap> bitmap;
    PlatformImagePtr platformImage;

#if USE(CG)
    bitmap = ShareableBitmap::createFromImagePixels(image);
    if (bitmap)
        platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);
#endif

    // If we failed to create ShareableBitmap or PlatformImage, fall back to image-draw method.
    if (!platformImage)
        bitmap = ShareableBitmap::createFromImageDraw(image);

    if (!platformImage && bitmap)
        platformImage = bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes);

    if (!platformImage)
        return std::nullopt;

    auto handle = bitmap->createHandle();
    if (!handle)
        return std::nullopt;

    handle->takeOwnershipOfMemory(MemoryLedger::Graphics);

    // Replace the PlatformImage of the input NativeImage with the shared one.
    image.setPlatformImage(WTFMove(platformImage));
    return handle;
}

void RemoteResourceCacheProxy::recordNativeImageUse(NativeImage& image)
{
    WebProcess::singleton().deferNonVisibleProcessEarlyMemoryCleanupTimer();

    auto iterator = m_nativeImages.find(image.renderingResourceIdentifier());
    if (iterator != m_nativeImages.end())
        return;

    auto handle = createShareableBitmapFromNativeImage(image);
    if (!handle) {
        // FIXME: Failing to send the image to GPUP will crash it when referencing this image.
        LOG_WITH_STREAM(Images, stream
            << "RemoteResourceCacheProxy::recordNativeImageUse() " << this
            << " image.size(): " << image.size()
            << " image.colorSpace(): " << image.colorSpace()
            << " ShareableBitmap could not be created; bailing.");
        return;
    }

    m_nativeImages.add(image.renderingResourceIdentifier(), image);

    // Set itself as an observer to NativeImage, so releaseNativeImage()
    // gets called when NativeImage is being deleleted.
    image.addObserver(*this);

    // Tell the GPU process to cache this resource.
    m_remoteRenderingBackendProxy.cacheNativeImage(WTFMove(*handle), image.renderingResourceIdentifier());
}

void RemoteResourceCacheProxy::recordFontUse(Font& font)
{
    if (font.platformData().customPlatformData())
        recordFontCustomPlatformDataUse(*font.platformData().customPlatformData());

    auto result = m_fonts.add(font.renderingResourceIdentifier(), m_renderingUpdateID);

    if (result.isNewEntry) {
        auto renderingResourceIdentifier = font.platformData().customPlatformData() ? std::optional(font.platformData().customPlatformData()->m_renderingResourceIdentifier) : std::nullopt;
        m_remoteRenderingBackendProxy.cacheFont(font.attributes(), font.platformData().attributes(), renderingResourceIdentifier);
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
        m_remoteRenderingBackendProxy.cacheFontCustomPlatformData(customPlatformData);
        ++m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate;
        return;
    }

    auto& currentState = result.iterator->value;
    if (currentState != m_renderingUpdateID) {
        currentState = m_renderingUpdateID;
        ++m_numberOfFontCustomPlatformDatasUsedInCurrentRenderingUpdate;
    }
}

void RemoteResourceCacheProxy::recordDecomposedGlyphsUse(DecomposedGlyphs& decomposedGlyphs)
{
    if (m_decomposedGlyphs.add(decomposedGlyphs.renderingResourceIdentifier(), decomposedGlyphs).isNewEntry) {
        decomposedGlyphs.addObserver(*this);
        m_remoteRenderingBackendProxy.cacheDecomposedGlyphs(decomposedGlyphs);
    }
}

void RemoteResourceCacheProxy::recordGradientUse(Gradient& gradient)
{
    if (m_gradients.add(gradient.renderingResourceIdentifier(), gradient).isNewEntry) {
        gradient.addObserver(*this);
        m_remoteRenderingBackendProxy.cacheGradient(gradient);
    }
}

void RemoteResourceCacheProxy::releaseRenderingResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    bool removed = m_nativeImages.remove(renderingResourceIdentifier)
        || m_decomposedGlyphs.remove(renderingResourceIdentifier)
        || m_gradients.remove(renderingResourceIdentifier);
    RELEASE_ASSERT(removed);
    m_remoteRenderingBackendProxy.releaseRenderingResource(renderingResourceIdentifier);
}

void RemoteResourceCacheProxy::clearNativeImageMap()
{
    for (auto& nativeImage : m_nativeImages.values())
        nativeImage.get()->removeObserver(*this);
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
    for (auto& imageBuffer : copyToVector(m_imageBuffers.values())) {
        if (!imageBuffer)
            continue;
        imageBuffer->clearBackend();
    }
}

void RemoteResourceCacheProxy::clearDecomposedGlyphsMap()
{
    for (auto& decomposedGlyphs : m_decomposedGlyphs.values())
        decomposedGlyphs.get()->removeObserver(*this);
    m_decomposedGlyphs.clear();
}

void RemoteResourceCacheProxy::clearGradientMap()
{
    for (auto& gradients : m_gradients.values())
        gradients.get()->removeObserver(*this);
    m_gradients.clear();
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
                m_remoteRenderingBackendProxy.releaseRenderingResource(item.key);
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
                m_remoteRenderingBackendProxy.releaseRenderingResource(item.key);
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
    clearNativeImageMap();
    clearFontMap();
    clearFontCustomPlatformDataMap();
    clearImageBufferBackends();
    clearDecomposedGlyphsMap();
    clearGradientMap();

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
    clearFontCustomPlatformDataMap();
    clearDecomposedGlyphsMap();
    clearGradientMap();
    m_remoteRenderingBackendProxy.releaseAllRemoteResources();
}

void RemoteResourceCacheProxy::releaseAllImageResources()
{
    clearNativeImageMap();
    m_remoteRenderingBackendProxy.releaseAllImageResources();
}


} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
