/*
 * Copyright (C) 2021-2023 Apple Inc.  All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "QualifiedRenderingResourceIdentifier.h"
#include <WebCore/DecomposedGlyphs.h>
#include <WebCore/Font.h>
#include <WebCore/Gradient.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SourceImage.h>
#include <wtf/HashMap.h>

namespace WebKit {

class QualifiedResourceHeap {
public:
    QualifiedResourceHeap(WebCore::ProcessIdentifier webProcessIdentifier)
        : m_webProcessIdentifier(webProcessIdentifier)
    {
    }

    void add(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, Ref<WebCore::ImageBuffer>&& imageBuffer)
    {
        add(renderingResourceIdentifier, WTFMove(imageBuffer), m_imageBufferCount);
    }

    void add(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, Ref<WebCore::NativeImage>&& image)
    {
        add(renderingResourceIdentifier, WTFMove(image), m_nativeImageCount);
    }

    void add(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, Ref<WebCore::Font>&& font)
    {
        add(renderingResourceIdentifier, WTFMove(font), m_fontCount);
    }

    void add(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, Ref<WebCore::DecomposedGlyphs>&& decomposedGlyphs)
    {
        add(renderingResourceIdentifier, WTFMove(decomposedGlyphs), m_decomposedGlyphsCount);
    }

    void add(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, Ref<WebCore::Gradient>&& gradient)
    {
        add(renderingResourceIdentifier, WTFMove(gradient), m_gradientCount);
    }

    WebCore::ImageBuffer* getImageBuffer(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<WebCore::ImageBuffer>(renderingResourceIdentifier);
    }

    WebCore::NativeImage* getNativeImage(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<WebCore::NativeImage>(renderingResourceIdentifier);
    }
    
    std::optional<WebCore::SourceImage> getSourceImage(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
    {
        if (!renderingResourceIdentifier)
            return std::nullopt;

        if (auto nativeImage = getNativeImage(renderingResourceIdentifier))
            return { { *nativeImage } };

        if (auto imageBuffer = getImageBuffer(renderingResourceIdentifier))
            return { { *imageBuffer } };

        return std::nullopt;
    }

    WebCore::Font* getFont(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<WebCore::Font>(renderingResourceIdentifier);
    }

    WebCore::DecomposedGlyphs* getDecomposedGlyphs(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<WebCore::DecomposedGlyphs>(renderingResourceIdentifier);
    }

    WebCore::Gradient* getGradient(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
    {
        return get<WebCore::Gradient>(renderingResourceIdentifier);
    }

    bool removeImageBuffer(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<WebCore::ImageBuffer>(renderingResourceIdentifier, m_imageBufferCount);
    }

    bool removeNativeImage(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<WebCore::NativeImage>(renderingResourceIdentifier, m_nativeImageCount);
    }

    bool removeFont(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<WebCore::Font>(renderingResourceIdentifier, m_fontCount);
    }

    bool removeDecomposedGlyphs(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<WebCore::DecomposedGlyphs>(renderingResourceIdentifier, m_decomposedGlyphsCount);
    }

    bool removeGradient(QualifiedRenderingResourceIdentifier renderingResourceIdentifier)
    {
        return remove<WebCore::Gradient>(renderingResourceIdentifier, m_gradientCount);
    }

    void releaseAllResources()
    {
        checkInvariants();

        if (!m_nativeImageCount && !m_fontCount && !m_decomposedGlyphsCount && !m_gradientCount)
            return;

        m_resources.removeIf([] (const auto& resource) {
            return std::holds_alternative<Ref<WebCore::NativeImage>>(resource.value)
                || std::holds_alternative<Ref<WebCore::Font>>(resource.value)
                || std::holds_alternative<Ref<WebCore::DecomposedGlyphs>>(resource.value)
                || std::holds_alternative<Ref<WebCore::Gradient>>(resource.value);
        });

        m_nativeImageCount = 0;
        m_fontCount = 0;
        m_decomposedGlyphsCount = 0;
        m_gradientCount = 0;

        checkInvariants();
    }

    void releaseAllNativeImages()
    {
        checkInvariants();

        if (!m_nativeImageCount)
            return;

        m_resources.removeIf([] (const auto& resource) {
            return std::holds_alternative<Ref<WebCore::NativeImage>>(resource.value);
        });

        m_nativeImageCount = 0;

        checkInvariants();
    }

private:
    template <typename T>
    void add(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, Ref<T>&& object, unsigned& counter)
    {
        checkInvariants();

        ASSERT(renderingResourceIdentifier.processIdentifier() == m_webProcessIdentifier);
        if (m_resources.add(renderingResourceIdentifier, WTFMove(object)).isNewEntry)
            ++counter;

        checkInvariants();
    }

    template <typename T>
    T* get(QualifiedRenderingResourceIdentifier renderingResourceIdentifier) const
    {
        checkInvariants();

        auto iterator = m_resources.find(renderingResourceIdentifier);
        if (iterator == m_resources.end())
            return nullptr;
        auto value = std::get_if<Ref<T>>(&iterator->value);
        return value ? value->ptr() : nullptr;
    }

    template <typename T>
    bool remove(QualifiedRenderingResourceIdentifier renderingResourceIdentifier, unsigned& counter)
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

    void checkInvariants() const
    {
#if ASSERT_ENABLED
        unsigned imageBufferCount = 0;
        unsigned nativeImageCount = 0;
        unsigned fontCount = 0;
        unsigned decomposedGlyphsCount = 0;
        unsigned gradientCount = 0;
        for (const auto& pair : m_resources) {
            WTF::switchOn(pair.value, [&] (std::monostate) {
                ASSERT_NOT_REACHED();
            }, [&] (const Ref<WebCore::ImageBuffer>&) {
                ++imageBufferCount;
            }, [&] (const Ref<WebCore::NativeImage>&) {
                ++nativeImageCount;
            }, [&] (const Ref<WebCore::Font>&) {
                ++fontCount;
            }, [&] (const Ref<WebCore::DecomposedGlyphs>&) {
                ++decomposedGlyphsCount;
            }, [&] (const Ref<WebCore::Gradient>&) {
                ++gradientCount;
            });
        }
        ASSERT(imageBufferCount == m_imageBufferCount);
        ASSERT(nativeImageCount == m_nativeImageCount);
        ASSERT(fontCount == m_fontCount);
        ASSERT(decomposedGlyphsCount == m_decomposedGlyphsCount);
        ASSERT(gradientCount == m_gradientCount);
        ASSERT(m_resources.size() == m_imageBufferCount + m_nativeImageCount + m_fontCount + m_decomposedGlyphsCount + m_gradientCount);
#endif
    }

    using Resource = std::variant<
        std::monostate,
        Ref<WebCore::ImageBuffer>,
        Ref<WebCore::NativeImage>,
        Ref<WebCore::Font>,
        Ref<WebCore::DecomposedGlyphs>,
        Ref<WebCore::Gradient>
    >;
    HashMap<QualifiedRenderingResourceIdentifier, Resource> m_resources;
    WebCore::ProcessIdentifier m_webProcessIdentifier;
    unsigned m_imageBufferCount { 0 };
    unsigned m_nativeImageCount { 0 };
    unsigned m_fontCount { 0 };
    unsigned m_decomposedGlyphsCount { 0 };
    unsigned m_gradientCount { 0 };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
