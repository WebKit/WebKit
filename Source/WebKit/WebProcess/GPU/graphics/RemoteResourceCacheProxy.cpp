/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
}

void RemoteResourceCacheProxy::cacheImageBuffer(WebCore::ImageBuffer& imageBuffer)
{
    auto addResult = m_imageBuffers.add(imageBuffer.renderingResourceIdentifier(), ImageBufferState { makeWeakPtr(imageBuffer), 0 });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

ImageBuffer* RemoteResourceCacheProxy::cachedImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return m_imageBuffers.get(renderingResourceIdentifier).imageBuffer.get();
}

void RemoteResourceCacheProxy::releaseImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto iterator = m_imageBuffers.find(renderingResourceIdentifier);
    RELEASE_ASSERT(iterator != m_imageBuffers.end());

    auto useCount = iterator->value.useCount;

    auto success = m_imageBuffers.remove(iterator);
    ASSERT_UNUSED(success, success);

    m_remoteRenderingBackendProxy.releaseRemoteResource(renderingResourceIdentifier, useCount);
}

inline static RefPtr<ShareableBitmap> createShareableBitmapFromNativeImage(NativeImage& image)
{
    auto imageSize = image.size();

    auto bitmap = ShareableBitmap::createShareable(image.size(), { });
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
    ASSERT(iterator != m_imageBuffers.end());

    ++iterator->value.useCount;
}

void RemoteResourceCacheProxy::recordNativeImageUse(NativeImage& image)
{
    auto iterator = m_nativeImages.find(image.renderingResourceIdentifier());
    if (iterator != m_nativeImages.end()) {
        ++iterator->value.useCount;
        return;
    }

    auto bitmap = createShareableBitmapFromNativeImage(image);
    if (!bitmap)
        return;

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);
    if (handle.isNull())
        return;

    m_nativeImages.add(image.renderingResourceIdentifier(), NativeImageState { makeWeakPtr(image), 1 });

    // Set itself as an observer to NativeImage, so releaseNativeImage()
    // gets called when NativeImage is being deleleted.
    image.addObserver(*this);

    // Tell the GPU process to cache this resource.
    m_remoteRenderingBackendProxy.cacheNativeImage(handle, image.renderingResourceIdentifier());
}

void RemoteResourceCacheProxy::recordFontUse(Font& font)
{
    auto result = m_fonts.ensure(font.renderingResourceIdentifier(), [&] {
        return FontState { m_remoteRenderingBackendProxy.renderingUpdateID(), 1 };
    });

    if (result.isNewEntry) {
        m_remoteRenderingBackendProxy.cacheFont(makeRef(font));
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
        return;
    }

    auto& currentState = result.iterator->value;
    ++currentState.useCount;
    if (currentState.lastRenderingUpdateVersionUsedWithin != m_remoteRenderingBackendProxy.renderingUpdateID()) {
        currentState.lastRenderingUpdateVersionUsedWithin = m_remoteRenderingBackendProxy.renderingUpdateID();
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
    }
}

void RemoteResourceCacheProxy::releaseNativeImage(RenderingResourceIdentifier renderingResourceIdentifier)
{
    auto iterator = m_nativeImages.find(renderingResourceIdentifier);
    RELEASE_ASSERT(iterator != m_nativeImages.end());

    auto useCount = iterator->value.useCount;

    auto success = m_nativeImages.remove(iterator);
    ASSERT_UNUSED(success, success);

    m_remoteRenderingBackendProxy.releaseRemoteResource(renderingResourceIdentifier, useCount);
}

void RemoteResourceCacheProxy::clearNativeImageMap()
{
    for (auto& nativeImageState : m_nativeImages.values())
        nativeImageState.image->removeObserver(*this);
    m_nativeImages.clear();
}

void RemoteResourceCacheProxy::prepareForNextRenderingUpdate()
{
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::releaseAllRemoteFonts()
{
    for (auto& fontState : m_fonts)
        m_remoteRenderingBackendProxy.releaseRemoteResource(fontState.key, fontState.value.useCount);
}

void RemoteResourceCacheProxy::clearFontMap()
{
    m_fonts.clear();
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::finalizeRenderingUpdateForFonts()
{
    static constexpr unsigned minimumRenderingUpdateCountToKeepFontAlive = 4;

    unsigned totalFontCount = m_fonts.size();
    RELEASE_ASSERT(m_numberOfFontsUsedInCurrentRenderingUpdate <= totalFontCount);
    if (totalFontCount == m_numberOfFontsUsedInCurrentRenderingUpdate)
        return;

    HashSet<WebCore::RenderingResourceIdentifier> toRemove;
    auto renderingUpdateID = m_remoteRenderingBackendProxy.renderingUpdateID();
    for (auto& item : m_fonts) {
        if (renderingUpdateID - item.value.lastRenderingUpdateVersionUsedWithin >= minimumRenderingUpdateCountToKeepFontAlive) {
            toRemove.add(item.key);
            m_remoteRenderingBackendProxy.releaseRemoteResource(item.key, item.value.useCount);
        }
    }

    m_fonts.removeIf([&](const auto& bucket) {
        return toRemove.contains(bucket.key);
    });
}

void RemoteResourceCacheProxy::finalizeRenderingUpdate()
{
    finalizeRenderingUpdateForFonts();
    prepareForNextRenderingUpdate();
}

void RemoteResourceCacheProxy::remoteResourceCacheWasDestroyed()
{
    clearNativeImageMap();
    clearFontMap();

    // Get a copy of m_imageBuffers.values() because clearBackend()
    // may release some of the cached ImageBuffers.
    for (auto& item : copyToVector(m_imageBuffers.values())) {
        if (!item.imageBuffer)
            continue;
        item.useCount = 0;
        item.imageBuffer->clearBackend();
    }

    for (auto& item : m_imageBuffers.values()) {
        if (!item.imageBuffer)
            continue;
        m_remoteRenderingBackendProxy.createRemoteImageBuffer(*item.imageBuffer);
    }
}

void RemoteResourceCacheProxy::releaseMemory()
{
    releaseAllRemoteFonts();
    clearFontMap();
    m_remoteRenderingBackendProxy.deleteAllFonts();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
