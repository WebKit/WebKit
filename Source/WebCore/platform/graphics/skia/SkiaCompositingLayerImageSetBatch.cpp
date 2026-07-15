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

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "BitmapTexture.h"
#include "CoordinatedTileBuffer.h"
#include "FloatRect.h"
#include "SkiaBackingStore.h"
#include "SkiaDamageRegion.h"

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

bool SkiaCompositingLayerImageSetBatch::imageRequiresLinearSampling(const SkCanvas& canvas, const sk_sp<SkImage>& image, const FloatRect& rect, const SkMatrix& ctm) const
{
    if (m_samplingOptions.filter == SkFilterMode::kLinear) {
        // Linear is already the batch sampling options, so don't need change it to keep the batch.
        return false;
    }

    // For axis aligned, not scaled and integer translated transforms, nearest and linear do the same,
    // so we can keep nearest to avoid the sampling options switch that will cause a new batch.
    auto matrix = canvas.getLocalToDeviceAs3x3() * ctm;
    matrix.preConcat(SkMatrix::RectToRect(SkRect::MakeWH(image->width(), image->height()), SkRect(rect)));
    if (!matrix.isScaleTranslate())
        return true;

    if (std::abs(matrix.getScaleX()) != 1 || std::abs(matrix.getScaleY()) != 1)
        return true;

    return !WTF::isIntegral(matrix.getTranslateX()) || !WTF::isIntegral(matrix.getTranslateY());
}

SkSamplingOptions SkiaCompositingLayerImageSetBatch::samplingOptionsForImage(const SkCanvas& canvas, const sk_sp<SkImage>& image, const FloatRect& rect, const SkMatrix& ctm) const
{
    if (!imageRequiresLinearSampling(canvas, image, rect, ctm))
        return m_samplingOptions;

    return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone);
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
    const auto sampling = SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone);

    if (!damageRegion) {
        updateSamplingOptions(canvas, sampling);
        backingStore.appendImageSetEntries(canvas, ctm, matrixIndexForDraw(ctm), opacity, enableAntialias, m_imageSet);
        return;
    }

    // Planned once for the whole layer. appendImageSetEntries() then splits each tile by the rects that touch it.
    const auto layerDeviceRect = ctm.mapRect(SkRect(FloatRect { { }, backingStore.size() }));

    if (!planRestrictedDraw(canvas, transform, ctm, layerDeviceRect, *damageRegion, [&](SkCanvas& canvas) {
        backingStore.paintToCanvas(canvas, fallbackPaint, damageRegion);
    }))
        return;

    updateSamplingOptions(canvas, sampling);
    backingStore.appendImageSetEntries(canvas, ctm, matrixIndexForDraw(ctm), opacity, enableAntialias, m_imageSet, damageRegion);
}

void SkiaCompositingLayerImageSetBatch::addImage(SkCanvas& canvas, const sk_sp<SkImage>& image, const FloatRect& rect, const SkM44& transform, float opacity, bool enableAntialias, const SkiaDamageRegion* damageRegion, const SkPaint& fallbackPaint)
{
    const auto ctm = transform.asM33();
    const SkRect srcRectFull = SkRect::MakeWH(image->width(), image->height());
    const SkRect dstRectFull = SkRect(rect);

    if (!damageRegion) {
        updateSamplingOptions(canvas, samplingOptionsForImage(canvas, image, rect, ctm));
        const auto matrixIndex = matrixIndexForDraw(ctm);
        const unsigned aaFlags = enableAntialias ? SkCanvas::kAll_QuadAAFlags : SkCanvas::kNone_QuadAAFlags;
        m_imageSet.append(SkCanvas::ImageSetEntry(image, srcRectFull, dstRectFull, matrixIndex, opacity, aaFlags, false));
        return;
    }

    const auto deviceRect = ctm.mapRect(dstRectFull);

    const auto inverse = planRestrictedDraw(canvas, transform, ctm, deviceRect, *damageRegion, [&](SkCanvas& canvas) {
        canvas.drawImageRect(image, srcRectFull, dstRectFull, SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNone), &fallbackPaint, SkCanvas::kFast_SrcRectConstraint);
    });
    if (!inverse)
        return;

    // Splitting creates edges inside the image, and antialiasing them would blend along those edges. This
    // never happens: a split needs a CTM that keeps rects as rects, which is never antialiased.
    ASSERT(!enableAntialias);

    updateSamplingOptions(canvas, samplingOptionsForImage(canvas, image, rect, ctm));
    const auto matrixIndex = matrixIndexForDraw(ctm);
    damageRegion->forEachDamagedSubRect(deviceRect, dstRectFull, srcRectFull, *inverse, [&](const SkRect& srcSubRect, const SkRect& dstSubRect) {
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

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
