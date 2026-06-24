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
#include "FontRenderOptions.h"
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
        it->value.scheduleUpdate(tileUpdate.dirtyRect, tileUpdate.tileRect, tileUpdate.buffer);
        m_hasPendingTileUpdates = true;
    }
}

void SkiaBackingStore::processPendingTileUpdates()
{
    if (!m_hasPendingTileUpdates)
        return;

    for (auto& tile : m_tiles.values())
        tile.processPendingUpdateIfNeeded();

    m_hasPendingTileUpdates = false;
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

        tilePaint.setAntiAlias(paint.isAntiAlias() && allTileEdgesExposed(layerRect, tile.rect()));
        canvas.drawImageRect(image, SkRect::MakeWH(image->width(), image->height()), tile.rect(), SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone), &tilePaint, SkCanvas::kFast_SrcRectConstraint);
    }
}

Vector<SkCanvas::ImageSetEntry> SkiaBackingStore::buildImageSet(SkCanvas& canvas, const SkMatrix& ctm, size_t matrixIndex, float opacity, bool enableAntialias) const
{
    if (m_tiles.isEmpty())
        return { };

    FloatRect layerRect = { { }, m_size };

    SkAutoCanvasRestore autoRestore(&canvas, true);
    canvas.concat(ctm);

    Vector<SkCanvas::ImageSetEntry> images;
    for (auto& tile : m_tiles.values()) {
        if (canvas.quickReject(tile.rect()))
            continue;

        const auto& image = tile.image();
        if (!image)
            continue;

        // FIXME: implement per edge antialiasing.
        unsigned aaFlags = enableAntialias && allTileEdgesExposed(layerRect, tile.rect()) ? SkCanvas::kAll_QuadAAFlags : SkCanvas::kNone_QuadAAFlags;
        images.append(SkCanvas::ImageSetEntry(image, SkRect::MakeWH(image->width(), image->height()), SkRect(tile.rect()), matrixIndex, opacity, aaFlags, false));
    }
    return images;
}

void SkiaBackingStore::drawDebugBorders(SkCanvas& canvas, const SkPaint& paint)
{
    for (const auto& tile : m_tiles.values())
        canvas.drawRect(SkRect(tile.rect()), paint);
}

void SkiaBackingStore::Tile::scheduleUpdate(const IntRect& dirtyRect, const IntRect& tileRect, CoordinatedTileBuffer& buffer)
{
    m_pendingUpdates.append({ tileRect, dirtyRect, Ref { buffer } });
}

void SkiaBackingStore::Tile::processPendingUpdateIfNeeded()
{
    for (auto& pendingUpdate : m_pendingUpdates)
        update(pendingUpdate.dirtyRect, pendingUpdate.tileRect, pendingUpdate.buffer.get());
    m_pendingUpdates.clear();
}

void SkiaBackingStore::Tile::ensureTexture(const IntSize& size, CoordinatedTileBuffer& buffer)
{
    OptionSet<BitmapTexture::Flags> flags;
    if (buffer.supportsAlpha())
        flags.add(BitmapTexture::Flags::SupportsAlpha);
    if (buffer.pixelFormat() == PixelFormat::BGRA8)
        flags.add(BitmapTexture::Flags::UseBGRALayout);

#if USE(GBM)
    if (SkiaPaintingEngine::shouldUseLinearTileTextures()) {
        flags.add(BitmapTexture::Flags::BackedByDMABuf);
        flags.add(BitmapTexture::Flags::ForceLinearBuffer);
    } else if (SkiaPaintingEngine::shouldUseVivanteSuperTiledTileTextures()) {
        flags.add(BitmapTexture::Flags::BackedByDMABuf);
        flags.add(BitmapTexture::Flags::ForceVivanteSuperTiledBuffer);
    }
#endif

    if (m_texture) {
        if (buffer.supportsAlpha() == m_texture->isOpaque())
            m_texture->reset(size, flags);
    } else {
        m_texture = BitmapTexturePool::singleton().acquireTexture(size, flags);
        m_cachedImage = nullptr;
    }
}

void SkiaBackingStore::Tile::update(const IntRect& dirtyRect, const IntRect& tileRect, CoordinatedTileBuffer& buffer)
{
    WTFBeginSignpost(this, SkiaBackingStoreTileUpdate, "rect %ix%i+%i+%i %s", tileRect.x(), tileRect.y(), tileRect.width(), tileRect.height(), buffer.isBackedByOpenGL() ? "GPUToGPU" : "CPUToGPU");

    FloatRect unscaledTileRect(tileRect);
    unscaledTileRect.scale(1. / m_scale);

    if (unscaledTileRect != m_rect) {
        m_rect = unscaledTileRect;
        m_texture = nullptr;
        m_surface = nullptr;
    }

    if (buffer.isBackedByOpenGL()) {
        auto& acceleratedBuffer = static_cast<CoordinatedAcceleratedTileBuffer&>(buffer);
        acceleratedBuffer.serverWait();

        if (auto displayList = acceleratedBuffer.displayList()) {
            ASSERT(!m_texture);
            ASSERT(!m_cachedImage);

            auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
            ASSERT(grContext);

            if (!m_surface) {
                const auto& characterization = displayList->characterization();
                m_surface = SkSurfaces::RenderTarget(grContext, skgpu::Budgeted::kYes, characterization.imageInfo(), characterization.sampleCount(), characterization.origin(), &characterization.surfaceProps());
            }

            skgpu::ganesh::DrawDDL(m_surface.get(), displayList);
        } else if (auto texture = acceleratedBuffer.texture()) {
            ASSERT(!m_surface);

            if (dirtyRect.size() == tileRect.size()) {
                // Fast path: whole tile content changed -- take ownership of the incoming texture, replacing the existing tile buffer (avoiding texture copies).
                if (m_texture)
                    m_texture->swapTexture(*texture);
                else
                    m_texture = WTF::move(texture);
                m_cachedImage = nullptr;
            } else {
                ensureTexture(tileRect.size(), buffer);
                m_texture->copyFromExternalTexture(texture->id(), dirtyRect, { });
            }
        }
    } else {
        auto& unacceleratedBuffer = static_cast<CoordinatedUnacceleratedTileBuffer&>(buffer);
        ensureTexture(tileRect.size(), buffer);
        m_texture->updateContents(unacceleratedBuffer.data(), dirtyRect, { }, unacceleratedBuffer.stride(), buffer.pixelFormat());
    }

    WTFEndSignpost(this, SkiaBackingStoreTileUpdate);
}

sk_sp<SkImage> SkiaBackingStore::Tile::image() const
{
    if (m_surface)
        return m_surface->makeImageSnapshot();

    if (!m_cachedImage && m_texture) {
        auto* grContext = PlatformDisplay::sharedDisplay().skiaGrContext();
        ASSERT(grContext);

        auto colorType = m_texture->flags().contains(BitmapTexture::Flags::UseBGRALayout) ? kBGRA_8888_SkColorType : kRGBA_8888_SkColorType;
        GrGLTextureInfo externalTexture;
        externalTexture.fTarget = GL_TEXTURE_2D;
        externalTexture.fID = m_texture->id();
        externalTexture.fFormat = colorType == kBGRA_8888_SkColorType ? GL_BGRA8_EXT : GL_RGBA8;
        auto backendTexture = GrBackendTextures::MakeGL(m_texture->size().width(), m_texture->size().height(), skgpu::Mipmapped::kNo, externalTexture);
        m_cachedImage = SkImages::BorrowTextureFrom(grContext, backendTexture, kTopLeft_GrSurfaceOrigin, colorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    }
    return m_cachedImage;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
