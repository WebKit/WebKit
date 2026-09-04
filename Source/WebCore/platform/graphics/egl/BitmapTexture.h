/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2014, 2026 Igalia S.L.
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

#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "PixelFormat.h"
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

#if USE(TEXTURE_MAPPER)
#include "ClipStack.h"
#include "FilterOperation.h"
#endif

#if USE(GBM)
#include "MemoryMappedGPUBuffer.h"
#endif

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkRefCnt.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
class GrDirectContext;
class SkSurface;
enum GrSurfaceOrigin : int;
#endif

typedef void *EGLImage;

namespace WebCore {

class GraphicsLayer;
class NativeImage;
enum class TextureMapperFlags : uint16_t;

class BitmapTexture final : public ThreadSafeRefCounted<BitmapTexture> {
public:
    enum class Flags : uint8_t {
        SupportsAlpha = 1 << 0,
#if USE(GBM)
        BackedByDMABuf = 1 << 1,
        ForceLinearBuffer = 1 << 2,
        ForceVivanteSuperTiledBuffer = 1 << 3,
#endif
        UseBGRALayout = 1 << 4,
        NearestFiltering = 1 << 5,
        ExternalOESRenderTarget = 1 << 6,
    };

    static Ref<BitmapTexture> create(const IntSize& size, OptionSet<Flags> flags = { })
    {
        return adoptRef(*new BitmapTexture(size, flags));
    }

#if USE(GBM) || OS(ANDROID)
    static Ref<BitmapTexture> create(EGLImage image, const IntSize& size, OptionSet<Flags> flags = { })
    {
        return adoptRef(*new BitmapTexture(image, size, flags));
    }
#endif

    WEBCORE_EXPORT ~BitmapTexture();

    const IntSize& size() const { return m_size; };
    size_t sizeInBytes() const;
    OptionSet<Flags> flags() const { return m_flags; }
    bool isOpaque() const { return !m_flags.contains(Flags::SupportsAlpha); }

    uint32_t id() const { return m_id; }

    void updateContents(const void* srcData, const IntRect& targetRect, const IntPoint& sourceOffset, int bytesPerLine, PixelFormat);

    void reset(const IntSize&, OptionSet<Flags> = { });

#if USE(TEXTURE_MAPPER)
    void updateContents(NativeImage*, const IntRect&, const IntPoint& offset);
    void updateContents(GraphicsLayer*, const IntRect& target, const IntPoint& offset, float scale = 1);

    void bindAsSurface();
    void initializeStencil();

    void swapTexture(BitmapTexture&);

    RefPtr<const FilterOperation> filterOperation() const { return m_filterOperation; }
    void setFilterOperation(RefPtr<const FilterOperation>&& filterOperation) { m_filterOperation = WTF::move(filterOperation); }

    ClipStack& clipStack() LIFETIME_BOUND { return m_clipStack; }

    OptionSet<TextureMapperFlags> colorConvertFlags() const;
#endif

    void copyFromExternalTexture(unsigned sourceTextureID, const IntRect& targetRect, const IntSize& sourceOffset);

#if USE(SKIA)
    GrBackendTexture createSkiaBackendTexture() const;
    sk_sp<SkSurface> createSkiaSurface(GrDirectContext*, GrSurfaceOrigin = kTopLeft_GrSurfaceOrigin, unsigned sampleCount = 0) const;
#endif

#if USE(GBM)
    MemoryMappedGPUBuffer* memoryMappedGPUBuffer() const LIFETIME_BOUND { return m_memoryMappedGPUBuffer.get(); }
    IntSize allocatedSize() const;
#else
    IntSize allocatedSize() const { return m_size; }
#endif

private:
    BitmapTexture(const IntSize&, OptionSet<Flags>);
#if USE(GBM) || OS(ANDROID)
    BitmapTexture(EGLImage, const IntSize&, OptionSet<Flags>);
#endif

#if USE(TEXTURE_MAPPER)
    void clearIfNeeded();
    void createFboIfNeeded();
#endif

    void determineRenderTargetAndBinding();

    unsigned textureFormat() const;
    void createTexture();
    void allocateTexture();
#if USE(GBM)
    bool allocateTextureFromMemoryMappedGPUBuffer();
#endif

    OptionSet<Flags> m_flags;
    IntSize m_size;
    unsigned m_id { 0 };
    unsigned m_renderTarget { 0 };
    unsigned m_binding { 0 };
    PixelFormat m_pixelFormat { PixelFormat::RGBA8 };

#if USE(GBM)
    std::unique_ptr<MemoryMappedGPUBuffer> m_memoryMappedGPUBuffer;
#endif

#if USE(TEXTURE_MAPPER)
    unsigned m_fbo { 0 };
    unsigned m_stencilBufferObject { 0 };
    bool m_stencilBound { false };
    bool m_shouldClear { true };
    ClipStack m_clipStack;
    RefPtr<const FilterOperation> m_filterOperation;
#endif
};

} // namespace WebCore
