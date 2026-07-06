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
#include "SkiaRecordCanvas.h"

#if USE(SKIA)
#include "IntSize.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPaint.h>
#include <skia/core/SkPath.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace WebCore {

static constexpr unsigned s_minSlowPathsForMSAA = 6;

SkiaRecordCanvas::SkiaRecordCanvas(const IntSize& size)
    : SkNWayCanvas(size.width(), size.height())
{
}

SkiaRecordCanvas::~SkiaRecordCanvas() = default;

bool SkiaRecordCanvas::shouldEnableMSAA() const
{
    // Do not enable MSAA if there are operations with antialias disabled.
    if (m_hasNonAntialiasPaint)
        return false;

    return m_slowPathForMSAACount >= s_minSlowPathsForMSAA;
}

void SkiaRecordCanvas::checkSlowPathsForPaint(const SkPaint& paint)
{
    m_hasNonAntialiasPaint |= !paint.isAntiAlias();
    if (m_hasNonAntialiasPaint || m_slowPathForMSAACount >= s_minSlowPathsForMSAA)
        return;

    if (paint.getPathEffect())
        m_slowPathForMSAACount++;
}

void SkiaRecordCanvas::onDrawPoints(PointMode mode, size_t count, const SkPoint pts[], const SkPaint& paint)
{
    m_hasNonAntialiasPaint |= !paint.isAntiAlias();
    if (!m_hasNonAntialiasPaint && m_slowPathForMSAACount < s_minSlowPathsForMSAA) {
        if (paint.getPathEffect() && paint.getStrokeCap() == SkPaint::kRound_Cap)
            m_slowPathForMSAACount++;
    }
    SkNWayCanvas::onDrawPoints(mode, count, pts, paint);
}

void SkiaRecordCanvas::onDrawDRRect(const SkRRect& outer, const SkRRect& inner, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawDRRect(outer, inner, paint);
}

void SkiaRecordCanvas::onDrawGlyphRunList(const sktext::GlyphRunList& glyphRunList, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawGlyphRunList(glyphRunList, paint);
}

void SkiaRecordCanvas::onDrawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawTextBlob(blob, x, y, paint);
}

void SkiaRecordCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawRect(rect, paint);
}

void SkiaRecordCanvas::onDrawRegion(const SkRegion& region, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawRegion(region, paint);
}

void SkiaRecordCanvas::onDrawOval(const SkRect& rect, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawOval(rect, paint);
}

void SkiaRecordCanvas::onDrawArc(const SkRect& rect, SkScalar startAngle, SkScalar sweepAngle, bool useCenter, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawArc(rect, startAngle, sweepAngle, useCenter, paint);
}

void SkiaRecordCanvas::onDrawRRect(const SkRRect& rect, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    SkNWayCanvas::onDrawRRect(rect, paint);
}

void SkiaRecordCanvas::onDrawPath(const SkPath& path, const SkPaint& paint)
{
    checkSlowPathsForPaint(paint);
    if (!m_hasNonAntialiasPaint && paint.isAntiAlias()) {
        if (!path.isConvex() && m_slowPathForMSAACount < s_minSlowPathsForMSAA) {
            if (paint.getStyle() == SkPaint::kStroke_Style && paint.getStrokeWidth())
                m_slowPathForMSAACount++;
            else if (paint.getStyle() == SkPaint::kFill_Style) {
                const auto& pathBounds = path.getBounds();
                if (pathBounds.width() >= 64 || pathBounds.height() >= 64 || path.isVolatile())
                    m_slowPathForMSAACount++;
            }
        }
    }
    SkNWayCanvas::onDrawPath(path, paint);
}

void SkiaRecordCanvas::onDrawImageRect2(const SkImage* image, const SkRect& src, const SkRect& dst, const SkSamplingOptions& sampling, const SkPaint* paint, SrcRectConstraint constraint)
{
    if (paint)
        checkSlowPathsForPaint(*paint);
    SkNWayCanvas::onDrawImageRect2(image, src, dst, sampling, paint, constraint);
}

void SkiaRecordCanvas::onClipRRect(const SkRRect& rect, SkClipOp op, ClipEdgeStyle edgeStyle)
{
    m_hasNonAntialiasPaint |= edgeStyle == kHard_ClipEdgeStyle;
    SkNWayCanvas::onClipRRect(rect, op, edgeStyle);
}

void SkiaRecordCanvas::onClipPath(const SkPath& path, SkClipOp op, ClipEdgeStyle edgeStyle)
{
    m_hasNonAntialiasPaint |= edgeStyle == kHard_ClipEdgeStyle;
    if (!m_hasNonAntialiasPaint && edgeStyle == kSoft_ClipEdgeStyle && m_slowPathForMSAACount < s_minSlowPathsForMSAA)
        m_slowPathForMSAACount += path.isConvex() ? 0 : 1;
    SkNWayCanvas::onClipPath(path, op, edgeStyle);
}

} // namespace WebCore

#endif // USE(SKIA)
