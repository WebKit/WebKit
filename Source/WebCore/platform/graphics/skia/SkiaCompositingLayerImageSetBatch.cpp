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
#include "SkiaCompositingLayerImageSetBatch.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
#include "BitmapTexture.h"
#include "CoordinatedTileBuffer.h"
#include "FloatRect.h"
#include "SkiaBackingStore.h"
#include "SkiaDamageRegion.h"
#include "SkiaUtilities.h"

namespace WebCore {

void SkiaCompositingLayerImageSetBatch::updatePaintProperties(SkCanvas& canvas, const sk_sp<SkColorFilter>& colorFilter, const std::optional<SkBlendMode>& blendMode)
{
    if (!m_imageSet.isEmpty() && (m_blendMode != blendMode || m_colorFilter != colorFilter))
        flushIfNeeded(canvas);

    m_blendMode = blendMode;
    m_colorFilter = colorFilter;
}

void SkiaCompositingLayerImageSetBatch::updateSamplingOptions(SkCanvas& canvas, SkSamplingOptions samplingOptions)
{
    if (!m_imageSet.isEmpty() && m_samplingOptions != samplingOptions)
        flushIfNeeded(canvas);

    m_samplingOptions = samplingOptions;
}

SkSamplingOptions SkiaCompositingLayerImageSetBatch::samplingOptionsForImage(const SkCanvas& canvas, const sk_sp<SkImage>& image, const FloatRect& rect, const SkMatrix& ctm) const
{
    if (m_samplingOptions.filter == SkFilterMode::kLinear)
        return m_samplingOptions;

    const auto matrix = canvas.getLocalToDeviceAs3x3() * ctm;
    return SkiaUtilities::samplingOptionsForImageDraw(matrix, SkRect::MakeWH(image->width(), image->height()), SkRect(rect));
}

SkSamplingOptions SkiaCompositingLayerImageSetBatch::samplingOptionsForBackingStore(const SkCanvas& canvas, const SkiaBackingStore& backingStore, const SkMatrix& ctm) const
{
    if (m_samplingOptions.filter == SkFilterMode::kLinear)
        return m_samplingOptions;

    const auto matrix = canvas.getLocalToDeviceAs3x3() * ctm;
    return backingStore.samplingOptionsForMatrix(matrix);
}

size_t SkiaCompositingLayerImageSetBatch::matrixIndexForDraw(const SkMatrix& ctm)
{
    if (m_preViewMatrices.isEmpty() || m_preViewMatrices.last() != ctm)
        m_preViewMatrices.append(ctm);

    return m_preViewMatrices.size() - 1;
}

void SkiaCompositingLayerImageSetBatch::addImageSet(SkCanvas& canvas, SkiaBackingStore& backingStore, const SkM44& transform, float opacity, bool enableAntialias, const SkiaDamageRegion* damageRegion, const SkPaint& fallbackPaint)
{
    const auto ctm = transform.asM33();
    const auto sampling = samplingOptionsForBackingStore(canvas, backingStore, ctm);

    // A batch draws all of its entries with one constraint. A strict one would clamp every piece of a tile that
    // the damage split, so the pieces would no longer join up. Draw this layer alone instead.
    if (backingStore.requiresStrictSourceConstraint(sampling)) {
        drawOutsideBatch(canvas, transform, damageRegion, [&](SkCanvas& canvas) {
            backingStore.paintToCanvas(canvas, fallbackPaint, damageRegion);
        });
        return;
    }

    if (!damageRegion) {
        updateSamplingOptions(canvas, sampling);
        backingStore.appendImageSetEntries(canvas, ctm, matrixIndexForDraw(ctm), opacity, enableAntialias, m_imageSet);
        return;
    }

    // Planned once for the whole layer. appendImageSetEntries() then splits each tile by the rects that touch it.
    const auto layerDeviceRect = ctm.mapRect(SkRect(FloatRect { { }, backingStore.size() }));

    SkMatrix inverse;
    const auto plan = planRestrictedDraw(canvas, transform, ctm, layerDeviceRect, *damageRegion, inverse, [&](SkCanvas& canvas) {
        backingStore.paintToCanvas(canvas, fallbackPaint, damageRegion);
    });
    if (plan == RestrictedDraw::Done)
        return;

    updateSamplingOptions(canvas, sampling);
    // A covered layer has every tile inside the damage, so there is nothing to split by.
    backingStore.appendImageSetEntries(canvas, ctm, matrixIndexForDraw(ctm), opacity, enableAntialias, m_imageSet,
        plan == RestrictedDraw::Whole ? nullptr : damageRegion);
}

void SkiaCompositingLayerImageSetBatch::addImage(SkCanvas& canvas, const sk_sp<SkImage>& image, const FloatRect& rect, const SkM44& transform, float opacity, bool enableAntialias, const SkiaDamageRegion* damageRegion, const SkPaint& fallbackPaint)
{
    const auto ctm = transform.asM33();
    const SkRect srcRectFull = SkRect::MakeWH(image->width(), image->height());
    const SkRect dstRectFull = SkRect(rect);
    const auto sampling = samplingOptionsForImage(canvas, image, rect, ctm);

    if (!damageRegion) {
        updateSamplingOptions(canvas, sampling);
        const auto matrixIndex = matrixIndexForDraw(ctm);
        const unsigned aaFlags = enableAntialias ? SkCanvas::kAll_QuadAAFlags : SkCanvas::kNone_QuadAAFlags;
        m_imageSet.append(SkCanvas::ImageSetEntry(image, srcRectFull, dstRectFull, matrixIndex, opacity, aaFlags, false));
        return;
    }

    const auto deviceRect = ctm.mapRect(dstRectFull);

    SkMatrix inverse;
    const auto plan = planRestrictedDraw(canvas, transform, ctm, deviceRect, *damageRegion, inverse, [&](SkCanvas& canvas) {
        canvas.drawImageRect(image, srcRectFull, dstRectFull, sampling, &fallbackPaint, SkCanvas::kFast_SrcRectConstraint);
    });
    if (plan == RestrictedDraw::Done)
        return;

    updateSamplingOptions(canvas, sampling);
    const auto matrixIndex = matrixIndexForDraw(ctm);

    // Drawn whole, so it has no interior edges and antialiases like a draw with no damage.
    if (plan == RestrictedDraw::Whole) {
        const unsigned aaFlags = enableAntialias ? SkCanvas::kAll_QuadAAFlags : SkCanvas::kNone_QuadAAFlags;
        m_imageSet.append(SkCanvas::ImageSetEntry(image, srcRectFull, dstRectFull, matrixIndex, opacity, aaFlags, false));
        return;
    }

    // Splitting creates edges inside the image, and antialiasing them would blend along those edges. This
    // never happens: a split needs a CTM that keeps rects as rects, which is never antialiased.
    ASSERT(!enableAntialias);

    damageRegion->forEachDamagedSubRect(deviceRect, dstRectFull, srcRectFull, inverse, [&](const SkRect& srcSubRect, const SkRect& dstSubRect) {
        m_imageSet.append(SkCanvas::ImageSetEntry(image, srcSubRect, dstSubRect, matrixIndex, opacity, SkCanvas::kNone_QuadAAFlags, false));
    });
}

void SkiaCompositingLayerImageSetBatch::flushIfNeeded(SkCanvas& canvas)
{
    if (m_imageSet.isEmpty())
        return;

    SkPaint paint;
    if (m_blendMode)
        paint.setBlendMode(*m_blendMode);
    if (m_colorFilter)
        paint.setColorFilter(m_colorFilter);

    // No entry ever has a clip, so there are no clip quads to pass.
    canvas.experimental_DrawEdgeAAImageSet(m_imageSet.span().data(), m_imageSet.size(), nullptr,
        m_preViewMatrices.span().data(), m_samplingOptions, &paint, SkCanvas::kFast_SrcRectConstraint);

    m_imageSet.clear();
    m_preViewMatrices.clear();
    m_blendMode = std::nullopt;
    m_samplingOptions = { };
}

SkiaCompositingLayerImageSetBatch::ScopedFlush::ScopedFlush(SkCanvas& canvas, SkiaCompositingLayerImageSetBatch& imageSetBatch, Mode mode)
    : m_canvas(canvas)
    , m_imageSetBatch(imageSetBatch)
    , m_mode(mode)
    , m_saveCount(canvas.getSaveCount())
{
    if (m_mode != Mode::DoNothing) {
        m_imageSetBatch.flushIfNeeded(m_canvas);
        canvas.save();
    }
}

SkiaCompositingLayerImageSetBatch::ScopedFlush::~ScopedFlush()
{
    if (m_mode == Mode::FlushBeforeAndAfter)
        m_imageSetBatch.flushIfNeeded(m_canvas);

    if (m_mode != Mode::DoNothing)
        m_canvas.restoreToCount(m_saveCount);
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA) && !USE(TEXTURE_MAPPER)
