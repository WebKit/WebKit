/*
 * Copyright (C) 2015, 2024 Igalia S.L.
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

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include "GLFence.h"
#include "TextureMapperFlags.h"
#include "TextureMapperPlatformLayer.h"
#include <wtf/OptionSet.h>

namespace WebCore {

class CoordinatedPlatformLayerBuffer : public TextureMapperPlatformLayer {
    WTF_MAKE_NONCOPYABLE(CoordinatedPlatformLayerBuffer);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(CoordinatedPlatformLayerBuffer);
public:
    enum class Type : uint8_t {
        RGB,
        YUV,
        ExternalOES,
        HolePunch,
        Video,
        DMABuf,
        NativeImage
    };

    virtual ~CoordinatedPlatformLayerBuffer() = default;

    Type type() const { return m_type; }
    const IntSize& size() const { return m_size; }
    OptionSet<TextureMapperFlags> flags() const { return m_flags; }

    void waitForContentsIfNeeded()
    {
        if (auto fence = WTF::move(m_fence))
            fence->serverWait();
    }

protected:
    CoordinatedPlatformLayerBuffer(Type type, const IntSize& size, OptionSet<TextureMapperFlags> flags, std::unique_ptr<GLFence>&& fence)
        : m_type(type)
        , m_size(size)
        , m_flags(flags)
        , m_fence(WTF::move(fence))
    {
    }

    Type m_type;
    IntSize m_size;
    OptionSet<TextureMapperFlags> m_flags;
    std::unique_ptr<GLFence> m_fence;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_COORDINATED_PLATFORM_LAYER_BUFFER_TYPE(ToValueTypeName, TypeEnumValue) \
    SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName)              \
    static bool isType(const WebCore::CoordinatedPlatformLayerBuffer& buffer) { return buffer.type() == WebCore::CoordinatedPlatformLayerBuffer::TypeEnumValue; } \
    SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(COORDINATED_GRAPHICS)
