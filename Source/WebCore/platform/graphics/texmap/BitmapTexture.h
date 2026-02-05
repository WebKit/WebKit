/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014 Igalia S.L.
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

#include "ClipStack.h"
#include "FilterOperation.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "PixelFormat.h"
#include "TextureMapperGLHeaders.h"
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if USE(GBM)
#include "MemoryMappedGPUBuffer.h"
#endif

typedef void *EGLImage;

namespace WebCore {

class GraphicsLayer;
class NativeImage;
class TextureMapper;
enum class TextureMapperFlags : uint16_t;

class BitmapTexture final : public ThreadSafeRefCounted<BitmapTexture> {
public:
    enum class Flags : uint8_t {
        SupportsAlpha = 1 << 0,
        DepthBuffer = 1 << 1,
#if USE(GBM)
        BackedByDMABuf = 1 << 2,
        ForceLinearBuffer = 1 << 3,
        ForceVivanteSuperTiledBuffer = 1 << 4,
#endif
        UseBGRALayout = 1 << 5,
        NearestFiltering = 1 << 6,
    };

    static Ref<BitmapTexture> create(const IntSize& size, OptionSet<Flags> flags = { })
    {
        return adoptRef(*new BitmapTexture(size, flags));
    }

#if USE(GBM)
    static Ref<BitmapTexture> create(EGLImage image, OptionSet<Flags> flags = { })
    {
        return adoptRef(*new BitmapTexture(image, flags));
    }
#endif

    WEBCORE_EXPORT ~BitmapTexture();

    const IntSize& size() const { return m_size; };
    OptionSet<Flags> flags() const { return m_flags; }
    bool isOpaque() const { return !m_flags.contains(Flags::SupportsAlpha); }

    void bindAsSurface();
    void initializeStencil();
    void initializeDepthBuffer();
    uint32_t id() const { return m_id; }

    void updateContents(NativeImage*, const IntRect&, const IntPoint& offset);
    void updateContents(GraphicsLayer*, const IntRect& target, const IntPoint& offset, float scale = 1);
    void updateContents(const void* srcData, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine, PixelFormat);

    void swapTexture(BitmapTexture&);
    void reset(const IntSize&, OptionSet<Flags> = { });

    int numberOfBytes() const { return size().width() * size().height() * 32 >> 3; }

    RefPtr<const FilterOperation> filterOperation() const { return m_filterOperation; }
    void setFilterOperation(RefPtr<const FilterOperation>&& filterOperation) { m_filterOperation = WTF::move(filterOperation); }

    ClipStack& clipStack() { return m_clipStack; }

    void copyFromExternalTexture(GLuint sourceTextureID, const IntRect& targetRect, const IntSize& sourceOffset);

    OptionSet<TextureMapperFlags> colorConvertFlags() const;

#if USE(GBM)
    MemoryMappedGPUBuffer* memoryMappedGPUBuffer() const { return m_memoryMappedGPUBuffer.get(); }
    IntSize allocatedSize() const;
#else
    IntSize allocatedSize() const { return m_size; }
#endif

private:
    BitmapTexture(const IntSize&, OptionSet<Flags>);
#if USE(GBM)
    BitmapTexture(EGLImage, OptionSet<Flags>);
#endif

    void clearIfNeeded();
    void createFboIfNeeded();

    GLenum textureFormat() const;
    void createTexture();
    void allocateTexture();
#if USE(GBM)
    bool allocateTextureFromMemoryMappedGPUBuffer();
#endif

    OptionSet<Flags> m_flags;
    IntSize m_size;
    GLuint m_id { 0 };
    GLuint m_fbo { 0 };
    GLuint m_depthBufferObject { 0 };
    GLuint m_stencilBufferObject { 0 };
    bool m_stencilBound { false };
    bool m_shouldClear { true };
    ClipStack m_clipStack;
    RefPtr<const FilterOperation> m_filterOperation;
    PixelFormat m_pixelFormat { PixelFormat::RGBA8 };

#if USE(GBM)
    std::unique_ptr<MemoryMappedGPUBuffer> m_memoryMappedGPUBuffer;
#endif
};

} // namespace WebCore
