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

#include "RemoteRenderingBackendProxy.h"

namespace WebKit {
using namespace WebCore;

RemoteResourceCacheProxy::RemoteResourceCacheProxy(RemoteRenderingBackendProxy& remoteRenderingBackendProxy)
    : m_remoteRenderingBackendProxy(remoteRenderingBackendProxy)
{
}

RemoteResourceCacheProxy::~RemoteResourceCacheProxy()
{
    for (auto& image : m_nativeImages.values())
        image->removeObserver(*this);
}

void RemoteResourceCacheProxy::cacheImageBuffer(WebCore::ImageBuffer& imageBuffer)
{
    auto addResult = m_imageBuffers.add(imageBuffer.renderingResourceIdentifier(), makeWeakPtr(imageBuffer));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

ImageBuffer* RemoteResourceCacheProxy::cachedImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return m_imageBuffers.get(renderingResourceIdentifier).get();
}

void RemoteResourceCacheProxy::releaseImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    bool found = m_imageBuffers.remove(renderingResourceIdentifier);
    ASSERT_UNUSED(found, found);
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

void RemoteResourceCacheProxy::cacheNativeImage(NativeImage& image)
{
    if (m_nativeImages.contains(image.renderingResourceIdentifier()))
        return;

    auto bitmap = createShareableBitmapFromNativeImage(image);
    if (!bitmap)
        return;

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);
    if (handle.isNull())
        return;

    m_nativeImages.add(image.renderingResourceIdentifier(), makeWeakPtr(image));

    // Set itself as an observer to NativeImage, so releaseNativeImage()
    // gets called when NativeImage is being deleleted.
    image.addObserver(*this);

    // Tell the GPU process to cache this resource.
    m_remoteRenderingBackendProxy.cacheNativeImage(handle, image.renderingResourceIdentifier());
}

void RemoteResourceCacheProxy::cacheFont(Font& font)
{
    auto result = m_fontIdentifierToLastRenderingUpdateVersionMap.ensure(font.renderingResourceIdentifier(), [&] {
        return 0;
    });
    auto& lastVersion = result.iterator->value;
    if (lastVersion != m_currentRenderingUpdateVersion) {
        lastVersion = m_currentRenderingUpdateVersion;
        ++m_numberOfFontsUsedInCurrentRenderingUpdate;
    }
    if (result.isNewEntry)
        m_remoteRenderingBackendProxy.cacheFont(makeRef(font));
}

void RemoteResourceCacheProxy::releaseNativeImage(RenderingResourceIdentifier renderingResourceIdentifier)
{
    if (!m_nativeImages.remove(renderingResourceIdentifier))
        return;

    // Tell the GPU process to remove this resource.
    m_remoteRenderingBackendProxy.releaseRemoteResource(renderingResourceIdentifier);
}

void RemoteResourceCacheProxy::didFinalizeRenderingUpdate()
{
    static constexpr unsigned minimumRenderingUpdateCountToKeepFontAlive = 4;
    static constexpr double minimumFractionOfUnusedFontCountToTriggerRemoval = 0.25;
    static constexpr unsigned maximumUnusedFontCountToSkipRemoval = 0;

    unsigned totalFontCount = m_fontIdentifierToLastRenderingUpdateVersionMap.size();
    RELEASE_ASSERT(m_numberOfFontsUsedInCurrentRenderingUpdate <= totalFontCount);
    unsigned unusedFontCount = totalFontCount - m_numberOfFontsUsedInCurrentRenderingUpdate;
    if (unusedFontCount < minimumFractionOfUnusedFontCountToTriggerRemoval * totalFontCount && unusedFontCount <= maximumUnusedFontCountToSkipRemoval)
        return;

    for (auto& item : m_fontIdentifierToLastRenderingUpdateVersionMap) {
        if (m_currentRenderingUpdateVersion - item.value >= minimumRenderingUpdateCountToKeepFontAlive)
            m_remoteRenderingBackendProxy.releaseRemoteResource(item.key);
    }

    ++m_currentRenderingUpdateVersion;
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
}

void RemoteResourceCacheProxy::releaseMemory()
{
    m_fontIdentifierToLastRenderingUpdateVersionMap.clear();
    m_numberOfFontsUsedInCurrentRenderingUpdate = 0;
    m_remoteRenderingBackendProxy.deleteAllFonts();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
