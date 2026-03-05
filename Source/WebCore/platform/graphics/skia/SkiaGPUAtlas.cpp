/*
 * Copyright (C) 2025, 2026 Igalia S.L.
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
#include "SkiaGPUAtlas.h"

#if USE(SKIA)
#include "SkiaImageAtlasLayout.h"
#if USE(GBM)
#include "MemoryMappedGPUBuffer.h"
#endif
#include <wtf/TZoneMallocInlines.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkPixmap.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#if USE(LIBEPOXY)
#include <epoxy/gl.h>
#else
#include <GLES3/gl3.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaGPUAtlas);

SkiaGPUAtlas::SkiaGPUAtlas(Ref<BitmapTexture>&& atlasTexture, GrBackendTexture&& backendTexture, const SkiaImageAtlasLayout& layout, const IntSize& size)
    : m_atlasTexture(WTF::move(atlasTexture))
    , m_backendTexture(WTF::move(backendTexture))
    , m_layout(layout)
    , m_size(size)
{
    m_imageToRect.reserveInitialCapacity(layout.entries().size());
    for (const auto& entry : layout.entries()) {
        const auto& rect = entry.atlasRect;
        m_imageToRect.add(entry.rasterImage.get(), SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()));
    }
}

SkiaGPUAtlas::~SkiaGPUAtlas() = default;

RefPtr<SkiaGPUAtlas> SkiaGPUAtlas::create(const SkiaImageAtlasLayout& layout, Ref<BitmapTexture>&& atlasTexture)
{
    const auto& atlasSize = layout.atlasSize();
    if (atlasSize.isEmpty())
        return nullptr;

    RELEASE_ASSERT(atlasSize == atlasTexture->size());

    GrGLTextureInfo externalTexture;
    externalTexture.fTarget = GL_TEXTURE_2D;
    externalTexture.fID = atlasTexture->id();
    externalTexture.fFormat = GL_RGBA8;
    auto backendTexture = GrBackendTextures::MakeGL(atlasSize.width(), atlasSize.height(), skgpu::Mipmapped::kNo, externalTexture);
    if (!backendTexture.isValid())
        return nullptr;

    return adoptRef(*new SkiaGPUAtlas(WTF::move(atlasTexture), WTF::move(backendTexture), layout, atlasSize));
}

bool SkiaGPUAtlas::uploadImages()
{
    Vector<uint8_t> conversionBuffer;

    // Returns pixel data for atlas upload — either a zero-copy reference to the original
    // pixels (fast path) or a converted sRGB copy (for non-sRGB images).
    auto pixelDataInSRGB = [&conversionBuffer](const sk_sp<SkImage>& image) -> std::optional<std::pair<const void*, size_t>> {
        SkPixmap pixmap;
        if (!image->peekPixels(&pixmap))
            return std::nullopt;

        if (auto* colorSpace = image->colorSpace(); !colorSpace || colorSpace->isSRGB())
            return std::pair { pixmap.addr(), pixmap.rowBytes() };

        // Convert to sRGB using Skia's built-in color space conversion.
        auto srgbInfo = SkImageInfo::Make(image->width(), image->height(), image->colorType(), image->alphaType(), SkColorSpace::MakeSRGB());
        size_t srgbRowBytes = srgbInfo.minRowBytes();
        conversionBuffer.resize(srgbInfo.computeMinByteSize());

        if (!image->readPixels(static_cast<GrDirectContext*>(nullptr), srgbInfo, conversionBuffer.mutableSpan().data(), srgbRowBytes, 0, 0))
            return std::nullopt;

        return std::pair { static_cast<const void*>(conversionBuffer.mutableSpan().data()), srgbRowBytes };
    };

#if USE(GBM)
    if (auto* gpuBuffer = m_atlasTexture->memoryMappedGPUBuffer()) {
        if (gpuBuffer->isLinear() || gpuBuffer->isVivanteSuperTiled()) {
            auto writeScope = makeGPUBufferWriteScope(*gpuBuffer);
            if (!writeScope)
                return false;

            for (const auto& entry : m_layout->entries()) {
                if (auto pixels = pixelDataInSRGB(entry.rasterImage))
                    gpuBuffer->updateContents(*writeScope, pixels->first, entry.atlasRect, pixels->second);
            }

            return true;
        }
    }
#endif

    // GL fallback: use BitmapTexture::updateContents() per entry.
    for (const auto& entry : m_layout->entries()) {
        if (auto pixels = pixelDataInSRGB(entry.rasterImage))
            m_atlasTexture->updateContents(pixels->first, entry.atlasRect, IntPoint::zero(), pixels->second, PixelFormat::BGRA8);
    }

    return true;
}

} // namespace WebCore

#endif // USE(SKIA)
