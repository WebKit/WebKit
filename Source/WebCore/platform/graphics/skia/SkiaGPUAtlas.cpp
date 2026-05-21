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
#include "BitmapTexture.h"
#include "SkiaImageAtlasLayout.h"
#if USE(GBM)
#include "MemoryMappedGPUBuffer.h"
#endif
#include <wtf/TZoneMallocInlines.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkPixmap.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

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

    auto backendTexture = atlasTexture->createSkiaBackendTexture();
    if (!backendTexture.isValid())
        return nullptr;

    return adoptRef(*new SkiaGPUAtlas(WTF::move(atlasTexture), WTF::move(backendTexture), layout, atlasSize));
}

void SkiaGPUAtlas::uploadImages()
{
    // The atlas's storage byte order is locked at creation: kBGRA on contexts with
    // GL_EXT_texture_format_BGRA8888, kRGBA otherwise (UseBGRALayout was sanitized
    // away). Each upload must match the storage byte order - every entry shares one
    // m_pixelFormat, and BitmapTexture::updateContents asserts on partial-region
    // pixel-format changes. So the fast path is "source already matches storage";
    // the slow path is an SkImage::readPixels conversion for the rare mismatched
    // entry. On each platform, the typical source colorType already matches storage
    // (BGRA on BGRA-capable, RGBA on the sanitized path), so we hit the fast path.
    const bool storageIsBGRA = m_atlasTexture->flags().contains(BitmapTexture::Flags::UseBGRALayout);
    const SkColorType storageColorType = storageIsBGRA ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType;

    Vector<uint8_t> conversionBuffer;
    auto pixelsMatchingStorage = [&](const sk_sp<SkImage>& image) -> std::optional<std::pair<const void*, size_t>> {
        SkPixmap pixmap;
        if (!image->peekPixels(&pixmap))
            return std::nullopt;

        auto* colorSpace = image->colorSpace();
        if (image->colorType() == storageColorType && (!colorSpace || colorSpace->isSRGB()))
            return std::pair { pixmap.addr(), pixmap.rowBytes() };

        auto dstInfo = SkImageInfo::Make(image->width(), image->height(), storageColorType, image->alphaType(), SkColorSpace::MakeSRGB());
        size_t dstRowBytes = dstInfo.minRowBytes();
        conversionBuffer.resize(dstInfo.computeMinByteSize());

        if (!image->readPixels(static_cast<GrDirectContext*>(nullptr), dstInfo, conversionBuffer.mutableSpan().data(), dstRowBytes, 0, 0))
            return std::nullopt;

        return std::pair { static_cast<const void*>(conversionBuffer.mutableSpan().data()), dstRowBytes };
    };

#if USE(GBM)
    if (auto* gpuBuffer = m_atlasTexture->memoryMappedGPUBuffer()) {
        if (gpuBuffer->isLinear() || gpuBuffer->isVivanteSuperTiled()) {
            auto writeScope = makeGPUBufferWriteScope(*gpuBuffer);
            RELEASE_ASSERT_WITH_MESSAGE(writeScope, "Failed to map GPU buffer for atlas upload");

            for (const auto& entry : m_layout->entries()) {
                if (auto pixels = pixelsMatchingStorage(entry.rasterImage))
                    gpuBuffer->updateContents(*writeScope, pixels->first, entry.atlasRect, pixels->second);
            }

            return;
        }
    }
#endif

    // GL fallback: pass the atlas's logical pixelFormat (BGRA8) so the compositor's
    // draw-time R<->B swap shader fires on contexts where storage was forced to RGBA.
    for (const auto& entry : m_layout->entries()) {
        if (auto pixels = pixelsMatchingStorage(entry.rasterImage))
            m_atlasTexture->updateContents(pixels->first, entry.atlasRect, IntPoint::zero(), pixels->second, PixelFormat::BGRA8);
    }
}

} // namespace WebCore

#endif // USE(SKIA)
