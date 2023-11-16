/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "DisplayListResourceHeap.h"

#include "DecomposedGlyphs.h"
#include "DisplayList.h"
#include "Filter.h"
#include "Gradient.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "SourceImage.h"

namespace WebCore {
namespace DisplayList {

ResourceHeap::ResourceHeap(const ResourceHeap& other, const Vector<RenderingResourceIdentifier>& resourceIdentifiers)
{
    for (auto resourceIdentifier : resourceIdentifiers) {
        auto resource = other.m_resources.get(resourceIdentifier);
        WTF::switchOn(resource, [&](auto& resource) {
            if constexpr (std::is_same<std::decay_t<decltype(resource)>, std::monostate>::value)
                RELEASE_ASSERT_NOT_REACHED();
            else
                add(WTFMove(resource));
        });
    }
}

void ResourceHeap::add(Ref<ImageBuffer>&& imageBuffer)
{
    auto renderingResourceIdentifier = imageBuffer->renderingResourceIdentifier();
    add<ImageBuffer>(renderingResourceIdentifier, WTFMove(imageBuffer), m_imageBufferCount);
}

void ResourceHeap::add(Ref<RenderingResource>&& resource)
{
    auto renderingResourceIdentifier = resource->renderingResourceIdentifier();
    add<RenderingResource>(renderingResourceIdentifier, WTFMove(resource), m_renderingResourceCount);
}

void ResourceHeap::add(Ref<NativeImage>&& image)
{
    auto renderingResourceIdentifier = image->renderingResourceIdentifier();
    add<RenderingResource>(renderingResourceIdentifier, WTFMove(image), m_renderingResourceCount);
}

void ResourceHeap::add(Ref<DecomposedGlyphs>&& decomposedGlyphs)
{
    auto renderingResourceIdentifier = decomposedGlyphs->renderingResourceIdentifier();
    add<RenderingResource>(renderingResourceIdentifier, WTFMove(decomposedGlyphs), m_renderingResourceCount);
}

void ResourceHeap::add(Ref<Gradient>&& gradient)
{
    auto renderingResourceIdentifier = gradient->renderingResourceIdentifier();
    add<RenderingResource>(renderingResourceIdentifier, WTFMove(gradient), m_renderingResourceCount);
}

void ResourceHeap::add(Ref<Filter>&& filter)
{
    auto renderingResourceIdentifier = filter->renderingResourceIdentifier();
    add<RenderingResource>(renderingResourceIdentifier, WTFMove(filter), m_renderingResourceCount);
}

void ResourceHeap::add(Ref<DisplayList>&& displayList)
{
    auto renderingResourceIdentifier = displayList->renderingResourceIdentifier();
    add<RenderingResource>(renderingResourceIdentifier, WTFMove(displayList), m_renderingResourceCount);
}

void ResourceHeap::add(Ref<Font>&& font)
{
    auto renderingResourceIdentifier = font->renderingResourceIdentifier();
    add<Font>(renderingResourceIdentifier, WTFMove(font), m_fontCount);
}

void ResourceHeap::add(Ref<FontCustomPlatformData>&& customPlatformData)
{
    auto renderingResourceIdentifier = customPlatformData->m_renderingResourceIdentifier;
    add<FontCustomPlatformData>(renderingResourceIdentifier, WTFMove(customPlatformData), m_customPlatformDataCount);
}

ImageBuffer* ResourceHeap::getImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return get<ImageBuffer>(renderingResourceIdentifier);
}

NativeImage* ResourceHeap::getNativeImage(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
    return dynamicDowncast<NativeImage>(renderingResource);
}

std::optional<SourceImage> ResourceHeap::getSourceImage(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    if (auto nativeImage = getNativeImage(renderingResourceIdentifier))
        return { { *nativeImage } };

    if (auto imageBuffer = getImageBuffer(renderingResourceIdentifier))
        return { { *imageBuffer } };

    return std::nullopt;
}

DecomposedGlyphs* ResourceHeap::getDecomposedGlyphs(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
    return dynamicDowncast<DecomposedGlyphs>(renderingResource);
}

Gradient* ResourceHeap::getGradient(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
    return dynamicDowncast<Gradient>(renderingResource);
}

Filter* ResourceHeap::getFilter(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
    return dynamicDowncast<Filter>(renderingResource);
}

DisplayList* ResourceHeap::getDisplayList(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    auto* renderingResource = get<RenderingResource>(renderingResourceIdentifier);
    return dynamicDowncast<DisplayList>(renderingResource);
}

Font* ResourceHeap::getFont(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return get<Font>(renderingResourceIdentifier);
}

FontCustomPlatformData* ResourceHeap::getFontCustomPlatformData(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    return get<FontCustomPlatformData>(renderingResourceIdentifier);
}

bool ResourceHeap::removeImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return remove<ImageBuffer>(renderingResourceIdentifier, m_imageBufferCount);
}

bool ResourceHeap::removeRenderingResource(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return remove<RenderingResource>(renderingResourceIdentifier, m_renderingResourceCount);
}

bool ResourceHeap::removeFont(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return remove<Font>(renderingResourceIdentifier, m_fontCount);
}

bool ResourceHeap::removeFontCustomPlatformData(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return remove<FontCustomPlatformData>(renderingResourceIdentifier, m_customPlatformDataCount);
}

void ResourceHeap::clearAllResources()
{
    m_resources.clear();

    m_imageBufferCount = 0;
    m_renderingResourceCount = 0;
    m_fontCount = 0;
    m_customPlatformDataCount = 0;
}

void ResourceHeap::clearAllImageResources()
{
    checkInvariants();

    m_resources.removeIf([&] (auto& resource) {
        auto value = std::get_if<Ref<RenderingResource>>(&resource.value);
        if (!value || !is<NativeImage>(value->get()))
            return false;
        --m_renderingResourceCount;
        return true;
    });

    checkInvariants();
}

void ResourceHeap::clearAllDrawingResources()
{
    checkInvariants();

    if (!m_renderingResourceCount && !m_fontCount && !m_customPlatformDataCount)
        return;

    m_resources.removeIf([] (const auto& resource) {
        return std::holds_alternative<Ref<RenderingResource>>(resource.value)
            || std::holds_alternative<Ref<Font>>(resource.value)
            || std::holds_alternative<Ref<FontCustomPlatformData>>(resource.value);
    });

    m_renderingResourceCount = 0;
    m_fontCount = 0;
    m_customPlatformDataCount = 0;

    checkInvariants();
}

template <typename T>
void ResourceHeap::add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<T>&& object, unsigned& counter)
{
    checkInvariants();

    if (m_resources.add(renderingResourceIdentifier, WTFMove(object)).isNewEntry)
        ++counter;

    checkInvariants();
}

template <typename T>
T* ResourceHeap::get(RenderingResourceIdentifier renderingResourceIdentifier) const
{
    checkInvariants();

    auto iterator = m_resources.find(renderingResourceIdentifier);
    if (iterator == m_resources.end())
        return nullptr;

    auto value = std::get_if<Ref<T>>(&iterator->value);
    return value ? value->ptr() : nullptr;
}

template <typename T>
bool ResourceHeap::remove(RenderingResourceIdentifier renderingResourceIdentifier, unsigned& counter)
{
    checkInvariants();

    if (!counter)
        return false;

    auto iterator = m_resources.find(renderingResourceIdentifier);
    if (iterator == m_resources.end())
        return false;
    if (!std::holds_alternative<Ref<T>>(iterator->value))
        return false;

    auto result = m_resources.remove(iterator);
    ASSERT(result);
    --counter;

    checkInvariants();

    return result;
}

void ResourceHeap::checkInvariants() const
{
#if ASSERT_ENABLED
    unsigned imageBufferCount = 0;
    unsigned renderingResourceCount = 0;
    unsigned fontCount = 0;
    unsigned customPlatformDataCount = 0;
    for (const auto& resource : m_resources) {
        if (std::holds_alternative<Ref<ImageBuffer>>(resource.value))
            ++imageBufferCount;
        else if (std::holds_alternative<Ref<RenderingResource>>(resource.value))
            ++renderingResourceCount;
        else if (std::holds_alternative<Ref<Font>>(resource.value))
            ++fontCount;
        else if (std::holds_alternative<Ref<FontCustomPlatformData>>(resource.value))
            ++customPlatformDataCount;
    }
    ASSERT(imageBufferCount == m_imageBufferCount);
    ASSERT(renderingResourceCount == m_renderingResourceCount);
    ASSERT(fontCount == m_fontCount);
    ASSERT(customPlatformDataCount == m_customPlatformDataCount);
    ASSERT(m_resources.size() == m_imageBufferCount + m_renderingResourceCount + m_fontCount + m_customPlatformDataCount);
#endif
}

} // namespace DisplayList
} // namespace WebCore
