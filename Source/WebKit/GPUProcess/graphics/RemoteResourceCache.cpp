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

#include <WebCore/Filter.h>
#include <WebCore/Font.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/Gradient.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>

namespace WebKit {
using namespace WebCore;

RemoteResourceCache::RemoteResourceCache() = default;

RemoteResourceCache::~RemoteResourceCache() = default;

bool RemoteResourceCache::cacheNativeImage(WebCore::RenderingResourceIdentifier identifier, Ref<NativeImage>&& image)
{
    return m_nativeImages.add(identifier, WTF::move(image)).isNewEntry;
}

bool RemoteResourceCache::releaseNativeImage(RenderingResourceIdentifier identifier)
{
    return m_nativeImages.remove(identifier);
}

RefPtr<NativeImage> RemoteResourceCache::cachedNativeImage(RenderingResourceIdentifier identifier) const
{
    return m_nativeImages.get(identifier);
}

bool RemoteResourceCache::cachePathImpl(RemotePathImplIdentifier identifier, Ref<PathImpl>&& path)
{
    return m_paths.add(identifier, WTF::move(path)).isNewEntry;
}

bool RemoteResourceCache::releasePathImpl(RemotePathImplIdentifier identifier)
{
    return m_paths.remove(identifier);
}

RefPtr<PathImpl> RemoteResourceCache::cachedPathImpl(RemotePathImplIdentifier identifier) const
{
    return m_paths.get(identifier);
}

bool RemoteResourceCache::cacheGradient(RemoteGradientIdentifier identifier, Ref<Gradient>&& gradient)
{
    return m_gradients.add(identifier, WTF::move(gradient)).isNewEntry;
}

bool RemoteResourceCache::releaseGradient(RemoteGradientIdentifier identifier)
{
    return m_gradients.remove(identifier);
}

RefPtr<Gradient> RemoteResourceCache::cachedGradient(RemoteGradientIdentifier identifier) const
{
    return m_gradients.get(identifier);
}

void RemoteResourceCache::cacheFilter(Ref<Filter>&& filter)
{
    auto identifier = filter->renderingResourceIdentifier();
    m_filters.add(identifier, WTF::move(filter));
}

bool RemoteResourceCache::releaseFilter(RenderingResourceIdentifier identifier)
{
    return m_filters.remove(identifier);
}

RefPtr<Filter> RemoteResourceCache::cachedFilter(RenderingResourceIdentifier identifier) const
{
    return m_filters.get(identifier);
}

void RemoteResourceCache::cacheFont(Ref<Font>&& font)
{
    auto identifier = font->renderingResourceIdentifier();
    m_fonts.add(identifier, WTF::move(font));
}

bool RemoteResourceCache::releaseFont(RenderingResourceIdentifier identifier)
{
    return m_fonts.remove(identifier);
}

RefPtr<Font> RemoteResourceCache::cachedFont(RenderingResourceIdentifier identifier) const
{
    return m_fonts.get(identifier);
}

void RemoteResourceCache::cacheFontCustomPlatformData(Ref<FontCustomPlatformData>&& customPlatformData)
{
    auto identifier = customPlatformData->m_renderingResourceIdentifier;
    m_fontCustomPlatformDatas.add(identifier, WTF::move(customPlatformData));
}

bool RemoteResourceCache::releaseFontCustomPlatformData(RenderingResourceIdentifier identifier)
{
    return m_fontCustomPlatformDatas.remove(identifier);
}

RefPtr<FontCustomPlatformData> RemoteResourceCache::cachedFontCustomPlatformData(RenderingResourceIdentifier identifier) const
{
    return m_fontCustomPlatformDatas.get(identifier);
}

bool RemoteResourceCache::cacheDisplayList(RemoteDisplayListIdentifier identifier, Ref<const DisplayList::DisplayList> displayList)
{
    return m_displayLists.add(identifier, displayList).isNewEntry;
}

RefPtr<const DisplayList::DisplayList> RemoteResourceCache::cachedDisplayList(RemoteDisplayListIdentifier identifier) const
{
    return m_displayLists.get(identifier);
}

bool RemoteResourceCache::releaseDisplayList(RemoteDisplayListIdentifier identifier)
{
    return m_displayLists.remove(identifier);
}

void RemoteResourceCache::releaseAllResources()
{
    m_imageBuffers.clear();
    m_nativeImages.clear();
    releaseMemory();
}

void RemoteResourceCache::releaseMemory()
{
    m_gradients.clear();
    m_filters.clear();
    m_fonts.clear();
    m_paths.clear();
    m_fontCustomPlatformDatas.clear();
    m_displayLists.clear();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
