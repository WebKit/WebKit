/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DisplayListResourceCache.h"

#include "DisplayList.h"
#include "FEImage.h"
#include "Pattern.h"
#include "SourceImage.h"

namespace WebCore {
namespace DisplayList {

ResourceCache::ResourceCache() = default;

ResourceCache::~ResourceCache() = default;

void ResourceCache::appendFont(Ref<const FontBase>&& font)
{
    m_fonts.append(WTF::move(font));
}

bool ResourceCache::cacheNativeImage(RenderingResourceIdentifier identifier, Ref<NativeImage>&& image)
{
    if (m_imageBuffers.contains(identifier))
        return false;
    return m_nativeImages.add(identifier, WTF::move(image)).isNewEntry;
}

bool ResourceCache::cacheImageBuffer(RenderingResourceIdentifier identifier, Ref<ImageBuffer>&& imageBuffer)
{
    if (m_nativeImages.contains(identifier))
        return false;
    return m_imageBuffers.add(identifier, WTF::move(imageBuffer)).isNewEntry;
}

void ResourceCache::appendDisplayList(Ref<const DisplayList>&& displayList)
{
    m_displayLists.append(WTF::move(displayList));
}

bool ResourceCache::resolveItem(Item& item) const
{
    return WTF::switchOn(item, [&](auto& item) {
        return resolve(item);
    });
}

bool ResourceCache::resolveNativeImage(auto& item) const
{
    if (item.isResolved())
        return true;
    auto iterator = m_nativeImages.find(item.imageIdentifier());
    if (iterator == m_nativeImages.end())
        return false;
    item.setNativeImage(iterator->value.copyRef());
    return true;
}

bool ResourceCache::resolveImageBuffer(auto& item) const
{
    if (item.isResolved())
        return true;
    auto iterator = m_imageBuffers.find(item.imageIdentifier());
    if (iterator == m_imageBuffers.end())
        return false;
    item.setImageBuffer(iterator->value.copyRef());
    return true;
}

std::optional<SourceImage> ResourceCache::sourceImage(RenderingResourceIdentifier identifier) const
{
    if (auto iterator = m_nativeImages.find(identifier); iterator != m_nativeImages.end())
        return SourceImage { iterator->value.copyRef() };
    if (auto iterator = m_imageBuffers.find(identifier); iterator != m_imageBuffers.end())
        return SourceImage { iterator->value.copyRef() };
    return std::nullopt;
}

bool ResourceCache::resolvePattern(const SourceBrush& brush) const
{
    RefPtr pattern = brush.pattern();
    if (!pattern || pattern->tileImage().isResolved())
        return true;
    auto tileImage = sourceImage(pattern->tileImage().imageIdentifier());
    if (!tileImage)
        return false;
    pattern->setTileImage(WTF::move(*tileImage));
    return true;
}

bool ResourceCache::resolve(ClipToNativeImage& item) const
{
    return resolveNativeImage(item);
}

bool ResourceCache::resolve(DrawDisplayList& item) const
{
    if (item.isResolved())
        return true;
    if (item.displayListIndex() >= m_displayLists.size())
        return false;
    item.setDisplayList(m_displayLists[item.displayListIndex()].copyRef());
    return true;
}

bool ResourceCache::resolve(DrawFilteredImageBuffer& item) const
{
    if (!item.isSourceImageResolved()) {
        auto iterator = m_imageBuffers.find(*item.sourceImageIdentifier());
        if (iterator == m_imageBuffers.end())
            return false;
        item.setSourceImage(iterator->value.copyRef());
    }
    Ref filter = item.filter();
    for (auto& effect : filter->effectsOfType(FilterFunction::Type::FEImage)) {
        Ref feImage = downcast<FEImage>(effect);
        if (feImage->sourceImage().isResolved())
            continue;
        auto image = sourceImage(feImage->sourceImage().imageIdentifier());
        if (!image)
            return false;
        feImage->setImageSource(WTF::move(*image));
    }
    return true;
}

bool ResourceCache::resolve(DrawGlyphs& item) const
{
    if (item.glyphs().size() != item.advances().size())
        return false;
    if (item.isResolved())
        return true;
    if (item.fontIndex() >= m_fonts.size())
        return false;
    item.setFont(m_fonts[item.fontIndex()].copyRef());
    return true;
}

bool ResourceCache::resolve(DrawImageBuffer& item) const
{
    return resolveImageBuffer(item);
}

bool ResourceCache::resolve(DrawNativeImage& item) const
{
    return resolveNativeImage(item);
}

bool ResourceCache::resolve(DrawPatternImageBuffer& item) const
{
    return resolveImageBuffer(item);
}

bool ResourceCache::resolve(DrawPatternNativeImage& item) const
{
    return resolveNativeImage(item);
}

bool ResourceCache::resolve(SetState& item) const
{
    return resolvePattern(item.state().fillBrush()) && resolvePattern(item.state().strokeBrush());
}

} // namespace DisplayList
} // namespace WebCore
