/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "Font.h"
#include "ImageBuffer.h"
#include "NativeImage.h"
#include "RenderingResourceIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {
namespace DisplayList {

class ResourceHeap {
public:
    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<ImageBuffer>&& imageBuffer)
    {
        if (m_resources.add(renderingResourceIdentifier, RefPtr<ImageBuffer>(WTFMove(imageBuffer))).isNewEntry)
            ++m_imageBufferCount;
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<NativeImage>&& image)
    {
        if (m_resources.add(renderingResourceIdentifier, RefPtr<NativeImage>(WTFMove(image))).isNewEntry)
            ++m_nativeImageCount;
    }

    void add(RenderingResourceIdentifier renderingResourceIdentifier, Ref<Font>&& font)
    {
        if (m_resources.add(renderingResourceIdentifier, RefPtr<Font>(WTFMove(font))).isNewEntry)
            ++m_fontCount;
    }

    ImageBuffer* getImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<ImageBuffer>(renderingResourceIdentifier);
    }

    NativeImage* getNativeImage(RenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<NativeImage>(renderingResourceIdentifier);
    }

    Font* getFont(RenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<Font>(renderingResourceIdentifier);
    }

    bool hasImageBuffer() const
    {
        return m_imageBufferCount;
    }

    bool hasNativeImage() const
    {
        return m_nativeImageCount;
    }

    bool hasFont() const
    {
        return m_fontCount;
    }

    bool removeImageBuffer(RenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<ImageBuffer>(renderingResourceIdentifier, m_imageBufferCount);
    }

    bool removeNativeImage(RenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<NativeImage>(renderingResourceIdentifier, m_nativeImageCount);
    }

    bool removeFont(RenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<Font>(renderingResourceIdentifier, m_fontCount);
    }

    void clear()
    {
        m_resources.clear();
        m_imageBufferCount = 0;
        m_nativeImageCount = 0;
        m_fontCount = 0;
    }

    void deleteAllFonts()
    {
        m_resources.removeIf([] (const auto& resource) {
            return std::holds_alternative<RefPtr<Font>>(resource.value);
        });
        m_fontCount = 0;
    }

private:
    template <typename T>
    T* get(RenderingResourceIdentifier renderingResourceIdentifier) const
    {
        auto iterator = m_resources.find(renderingResourceIdentifier);
        if (iterator == m_resources.end())
            return nullptr;
        if (!std::holds_alternative<RefPtr<T>>(iterator->value))
            return nullptr;
        return WTF::get<RefPtr<T>>(iterator->value).get();
    }

    template <typename T>
    bool remove(RenderingResourceIdentifier renderingResourceIdentifier, unsigned& counter)
    {
        auto iterator = m_resources.find(renderingResourceIdentifier);
        if (iterator == m_resources.end())
            return false;
        if (!std::holds_alternative<RefPtr<T>>(iterator->value))
            return false;
        auto result = m_resources.remove(iterator);
        ASSERT(result);
        --counter;
        return result;
    }

    using Resource = Variant<RefPtr<ImageBuffer>, RefPtr<NativeImage>, RefPtr<Font>>;
    HashMap<RenderingResourceIdentifier, Resource> m_resources;
    unsigned m_imageBufferCount { 0 };
    unsigned m_nativeImageCount { 0 };
    unsigned m_fontCount { 0 };
};

}
}
