/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "SkiaBackingStore.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexturePool.h"
#include "CoordinatedTileBuffer.h"
#include "PlatformDisplay.h"
#include "SkiaPaintingEngine.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/gpu/ganesh/GrBackendSurface.h>
#include <skia/gpu/ganesh/SkImageGanesh.h>
#include <skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <skia/gpu/ganesh/gl/GrGLBackendSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/SystemTracing.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SkiaBackingStore);

void SkiaBackingStore::update(const FloatSize& size, float scale, CoordinatedBackingStoreProxy::Update&& update)
{
    m_size = size;
    m_scale = scale;

    for (auto tileID : update.tilesToCreate())
        m_tiles.add(tileID, Tile(m_scale));

    for (auto tileID : update.tilesToRemove()) {
        ASSERT(m_tiles.contains(tileID));
        m_tiles.remove(tileID);
    }

    for (const auto& tileUpdate : update.tilesToUpdate()) {
        auto it = m_tiles.find(tileUpdate.tileID);
        ASSERT(it != m_tiles.end());
        it->value.update(tileUpdate.dirtyRect, tileUpdate.tileRect, tileUpdate.buffer);
    }
}

static inline bool allTileEdgesExposed(const FloatRect& totalRect, const FloatRect& tileRect)
{
    return !tileRect.x() && !tileRect.y() && tileRect.width() + tileRect.x() >= totalRect.width() && tileRect.height() + tileRect.y() >= totalRect.height();
}

void SkiaBackingStore::paintToCanvas(SkCanvas& canvas, const SkPaint& paint)
{
    if (m_tiles.isEmpty())
        return;

    FloatRect layerRect = { { }, m_size };

    auto tilePaint = paint;
    for (auto& tile : m_tiles.values()) {
        if (canvas.quickReject(tile.rect()))
            continue;

        const auto& image = tile.image();
        if (!image)
            continue;

        tilePaint.setAntiAlias(allTileEdgesExposed(layerRect, tile.rect()));
        canvas.drawImageRect(image, SkRect::MakeWH(image->width(), image->height()), tile.rect(), SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone), &tilePaint, SkCanvas::kFast_SrcRectConstraint);
    }
}

void SkiaBackingStore::drawDebugBorders(SkCanvas& canvas, const SkPaint& paint)
{
    for (const auto& tile : m_tiles.values())
        canvas.drawRect(SkRect(tile.rect()), paint);
}

bool SkiaBackingStore::Tile::tryEnsureSurface(const IntSize& size, CoordinatedTileBuffer& buffer)
{
    if (m_surface)
        return true;

    OptionSet<BitmapTexture::Flags> flags;
    if (buffer.supportsAlpha())
        flags.add(BitmapTexture::Flags::SupportsAlpha);

#if USE(GBM)
    if (SkiaPaintingEngine::shouldUseLinearTileTextures()) {
        flags.add(BitmapTexture::Flags::BackedByDMABuf);
        flags.add(BitmapTexture::Flags::ForceLinearBuffer);
    } else if (SkiaPaintingEngine::shouldUseVivanteSuperTiledTileTextures()) {
        flags.add(BitmapTexture::Flags::BackedByDMABuf);
        flags.add(BitmapTexture::Flags::ForceVivanteSuperTiledBuffer);
    }
#endif

    auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
    auto texture = BitmapTexturePool::singleton().acquireTexture(size, flags);
    unsigned textureID = texture->id();
    GrBackendTexture backendTexture = texture->createSkiaBackendTexture();
    auto surface = SkSurfaces::WrapBackendTexture(grContext, backendTexture, kTopLeft_GrSurfaceOrigin, 0, kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr, +[](void* userData) {
        static_cast<BitmapTexture*>(userData)->deref();
    }, &texture.leakRef());
    if (!surface)
        return false;

    auto* canvas = surface->getCanvas();
    if (!canvas)
        return false;

    canvas->clear(SK_ColorTRANSPARENT);
    m_surface = WTF::move(surface);
    m_textureID = textureID;
    m_cachedImage = nullptr;
    return true;
}

void SkiaBackingStore::Tile::update(const IntRect& dirtyRect, const IntRect& tileRect, CoordinatedTileBuffer& buffer)
{
    WTFBeginSignpost(this, SkiaBackingStoreTileUpdate, "rect %ix%i+%i+%i %s", tileRect.x(), tileRect.y(), tileRect.width(), tileRect.height(), buffer.isBackedByOpenGL() ? "GPUToGPU" : "CPUToGPU");

    FloatRect unscaledTileRect(tileRect);
    unscaledTileRect.scale(1. / m_scale);

    if (unscaledTileRect != m_rect) {
        m_rect = unscaledTileRect;
        m_surface = nullptr;
    }

    if (buffer.isBackedByOpenGL()) {
        auto& acceleratedBuffer = static_cast<CoordinatedAcceleratedTileBuffer&>(buffer);
        acceleratedBuffer.serverWait();

        Ref texture = acceleratedBuffer.texture();
        GrBackendTexture backendTexture = texture->createSkiaBackendTexture();
        if (dirtyRect.size() == tileRect.size()) {
            // Fast path: whole tile content changed -- take ownership of the incoming texture, replacing the existing tile buffer (avoiding texture copies).
            m_textureID = texture->id();
            m_cachedImage = nullptr;

            if (m_surface) {
                m_surface->replaceBackendTexture(backendTexture, kTopLeft_GrSurfaceOrigin, SkSurface::kDiscard_ContentChangeMode, +[](void* userData) {
                    static_cast<BitmapTexture*>(userData)->deref();
                }, &texture.leakRef());
            } else {
                auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
                m_surface = SkSurfaces::WrapBackendTexture(grContext, backendTexture, kTopLeft_GrSurfaceOrigin, 0, kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr, +[](void* userData) {
                    static_cast<BitmapTexture*>(userData)->deref();
                }, &texture.leakRef());
            }
        } else if (tryEnsureSurface(tileRect.size(), buffer)) {
            auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
            auto image = SkImages::BorrowTextureFrom(grContext, backendTexture, kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
            m_surface->getCanvas()->drawImageRect(image, SkRect::MakeWH(dirtyRect.width(), dirtyRect.height()), SkRect::Make(SkIRect(dirtyRect)), SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone), nullptr, SkCanvas::kFast_SrcRectConstraint);
        }
    } else if (tryEnsureSurface(tileRect.size(), buffer)) {
        auto& unacceleratedBuffer = static_cast<CoordinatedUnacceleratedTileBuffer&>(buffer);
        auto imageInfo = SkImageInfo::Make(dirtyRect.width(), dirtyRect.height(), kBGRA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
        SkPixmap pixmap(imageInfo, unacceleratedBuffer.data(), unacceleratedBuffer.stride());
        m_surface->writePixels(pixmap, dirtyRect.x(), dirtyRect.y());
    }

    WTFEndSignpost(this, SkiaBackingStoreTileUpdate);
}

sk_sp<SkImage> SkiaBackingStore::Tile::image()
{
    // SkSurface::makeImageSnapshot() does a copy-on-write, but when the surface is wrapping an
    // external texture, it always copies because it doesn't know if the texture will be modified
    // externally. We know the texture won't change, so we can use our own cached image wihtout copying.
    if (!m_cachedImage && m_surface) {
        GrGLTextureInfo externalTexture;
        externalTexture.fTarget = GL_TEXTURE_2D;
        externalTexture.fID = m_textureID;
        externalTexture.fFormat = GL_RGBA8;
        auto backendTexture = GrBackendTextures::MakeGL(m_surface->width(), m_surface->height(), skgpu::Mipmapped::kNo, externalTexture);
        m_cachedImage = SkImages::BorrowTextureFrom(PlatformDisplay::sharedDisplay().skiaGrContext(), backendTexture, kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    }
    return m_cachedImage;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
