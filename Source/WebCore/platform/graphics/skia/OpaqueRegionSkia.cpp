/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "OpaqueRegionSkia.h"

#include "PlatformContextSkia.h"

#include "SkCanvas.h"
#include "SkColorFilter.h"
#include "SkShader.h"

namespace WebCore {

OpaqueRegionSkia::OpaqueRegionSkia()
    : m_opaqueRect(SkRect::MakeEmpty())
{
}

OpaqueRegionSkia::~OpaqueRegionSkia()
{
}

IntRect OpaqueRegionSkia::asRect() const
{
    // Returns the largest enclosed rect.
    int left = SkScalarCeil(m_opaqueRect.fLeft);
    int top = SkScalarCeil(m_opaqueRect.fTop);
    int right = SkScalarFloor(m_opaqueRect.fRight);
    int bottom = SkScalarFloor(m_opaqueRect.fBottom);
    return IntRect(left, top, right-left, bottom-top);
}

// Returns true if the xfermode will force the dst to be opaque, regardless of the current dst.
static inline bool xfermodeIsOpaque(const SkPaint& paint, bool srcIsOpaque)
{
    if (!srcIsOpaque)
        return false;

    SkXfermode* xfermode = paint.getXfermode();
    if (!xfermode)
        return true; // default to kSrcOver_Mode
    SkXfermode::Mode mode;
    if (!xfermode->asMode(&mode))
        return false;

    switch (mode) {
    case SkXfermode::kSrc_Mode: // source
    case SkXfermode::kSrcOver_Mode: // source + dest - source*dest
    case SkXfermode::kDstOver_Mode: // source + dest - source*dest
    case SkXfermode::kDstATop_Mode: // source
    case SkXfermode::kPlus_Mode: // source+dest
    default: // the rest are all source + dest - source*dest
        return true;
    case SkXfermode::kClear_Mode: // 0
    case SkXfermode::kDst_Mode: // dest
    case SkXfermode::kSrcIn_Mode: // source * dest
    case SkXfermode::kDstIn_Mode: // dest * source
    case SkXfermode::kSrcOut_Mode: // source * (1-dest)
    case SkXfermode::kDstOut_Mode: // dest * (1-source)
    case SkXfermode::kSrcATop_Mode: // dest
    case SkXfermode::kXor_Mode: // source + dest - 2*(source*dest)
        return false;
    }
}

// Returns true if the xfermode will keep the dst opaque, assuming the dst is already opaque.
static inline bool xfermodePreservesOpaque(const SkPaint& paint, bool srcIsOpaque)
{
    SkXfermode* xfermode = paint.getXfermode();
    if (!xfermode)
        return true; // default to kSrcOver_Mode
    SkXfermode::Mode mode;
    if (!xfermode->asMode(&mode))
        return false;

    switch (mode) {
    case SkXfermode::kDst_Mode: // dest
    case SkXfermode::kSrcOver_Mode: // source + dest - source*dest
    case SkXfermode::kDstOver_Mode: // source + dest - source*dest
    case SkXfermode::kSrcATop_Mode: // dest
    case SkXfermode::kPlus_Mode: // source+dest
    default: // the rest are all source + dest - source*dest
        return true;
    case SkXfermode::kClear_Mode: // 0
    case SkXfermode::kSrcOut_Mode: // source * (1-dest)
    case SkXfermode::kDstOut_Mode: // dest * (1-source)
    case SkXfermode::kXor_Mode: // source + dest - 2*(source*dest)
        return false;
    case SkXfermode::kSrc_Mode: // source
    case SkXfermode::kSrcIn_Mode: // source * dest
    case SkXfermode::kDstIn_Mode: // dest * source
    case SkXfermode::kDstATop_Mode: // source
        return srcIsOpaque;
    }
}

// Returns true if all pixels painted will be opaque.
static inline bool paintIsOpaque(const SkPaint& paint, const SkBitmap* bitmap = 0, bool checkFillOnly = false)
{
    if (paint.getAlpha() < 0xFF)
        return false;
    if (!checkFillOnly && paint.getStyle() != SkPaint::kFill_Style && paint.isAntiAlias())
        return false;
    SkShader* shader = paint.getShader();
    if (shader && !shader->isOpaque())
        return false;
    if (bitmap && !bitmap->isOpaque())
        return false;
    if (paint.getLooper())
        return false;
    if (paint.getImageFilter())
        return false;
    if (paint.getMaskFilter())
        return false;
    SkColorFilter* colorFilter = paint.getColorFilter();
    if (colorFilter && !(colorFilter->getFlags() & SkColorFilter::kAlphaUnchanged_Flag))
        return false;
    return true;
}

void OpaqueRegionSkia::didDrawRect(PlatformContextSkia* context, const AffineTransform& transform, const SkRect& fillRect, const SkPaint& paint, const SkBitmap* bitmap)
{
    // Any stroking may put alpha in pixels even if the filling part does not.
    if (paint.getStyle() != SkPaint::kFill_Style) {
        bool opaque = paintIsOpaque(paint, bitmap);
        bool fillsBounds = false;

        if (!paint.canComputeFastBounds())
            didDrawUnbounded(paint, opaque);
        else {
            SkRect strokeRect;
            strokeRect = paint.computeFastBounds(fillRect, &strokeRect);
            didDraw(context, transform, strokeRect, paint, opaque, fillsBounds);
        }
    }

    bool checkFillOnly = true;
    bool opaque = paintIsOpaque(paint, bitmap, checkFillOnly);
    bool fillsBounds = paint.getStyle() != SkPaint::kStroke_Style;
    didDraw(context, transform, fillRect, paint, opaque, fillsBounds);
}

void OpaqueRegionSkia::didDrawPath(PlatformContextSkia* context, const AffineTransform& transform, const SkPath& path, const SkPaint& paint)
{
    SkRect rect;
    if (path.isRect(&rect)) {
        didDrawRect(context, transform, rect, paint, 0);
        return;
    }

    bool opaque = paintIsOpaque(paint);
    bool fillsBounds = false;

    if (!paint.canComputeFastBounds())
        didDrawUnbounded(paint, opaque);
    else {
        rect = paint.computeFastBounds(path.getBounds(), &rect);
        didDraw(context, transform, rect, paint, opaque, fillsBounds);
    }
}

void OpaqueRegionSkia::didDrawPoints(PlatformContextSkia* context, const AffineTransform& transform, SkCanvas::PointMode mode, int numPoints, const SkPoint points[], const SkPaint& paint)
{
    if (!numPoints)
        return;

    SkRect rect;
    rect.fLeft = points[0].fX;
    rect.fRight = points[0].fX + 1;
    rect.fTop = points[0].fY;
    rect.fBottom = points[0].fY + 1;

    for (int i = 1; i < numPoints; ++i) {
        rect.fLeft = std::min(rect.fLeft, points[i].fX);
        rect.fRight = std::max(rect.fRight, points[i].fX + 1);
        rect.fTop = std::min(rect.fTop, points[i].fY);
        rect.fBottom = std::max(rect.fBottom, points[i].fY + 1);
    }

    bool opaque = paintIsOpaque(paint);
    bool fillsBounds = false;

    if (!paint.canComputeFastBounds())
        didDrawUnbounded(paint, opaque);
    else {
        rect = paint.computeFastBounds(rect, &rect);
        didDraw(context, transform, rect, paint, opaque, fillsBounds);
    }
}

void OpaqueRegionSkia::didDrawBounded(PlatformContextSkia* context, const AffineTransform& transform, const SkRect& bounds, const SkPaint& paint)
{
    bool opaque = paintIsOpaque(paint);
    bool fillsBounds = false;

    if (!paint.canComputeFastBounds())
        didDrawUnbounded(paint, opaque);
    else {
        SkRect rect;
        rect = paint.computeFastBounds(bounds, &rect);
        didDraw(context, transform, rect, paint, opaque, fillsBounds);
    }
}

void OpaqueRegionSkia::didDraw(PlatformContextSkia* context, const AffineTransform& transform, const SkRect& rect, const SkPaint& paint, bool drawsOpaque, bool fillsBounds)
{
    SkRect targetRect = rect;

    bool xfersOpaque = xfermodeIsOpaque(paint, drawsOpaque);
    bool preservesOpaque = xfermodePreservesOpaque(paint, drawsOpaque);

    // Apply the transform to device coordinate space.
    SkMatrix canvasTransform = context->canvas()->getTotalMatrix();
    if (!canvasTransform.mapRect(&targetRect))
        fillsBounds = false;

    // Apply the current clip in device coordinate space.
    if (context->canvas()->getClipType() != SkCanvas::kRect_ClipType)
        fillsBounds = false;
    else {
        SkIRect deviceClip;
        context->canvas()->getClipDeviceBounds(&deviceClip);
        if (!targetRect.intersect(SkIntToScalar(deviceClip.fLeft), SkIntToScalar(deviceClip.fTop), SkIntToScalar(deviceClip.fRight), SkIntToScalar(deviceClip.fBottom)))
            return;
    }

    // Apply any layers that we are drawing into.
    for (SkCanvas::LayerIter it(context->canvas(), false); !it.done(); it.next()) {
        // Apply the layer's clip (which is in device space)
        const SkRegion& deviceLayerClip = it.clip();
        if (deviceLayerClip.isEmpty() || !deviceLayerClip.isRect())
            fillsBounds = false;
        else {
            SkIRect clipBounds = it.clip().getBounds();
            if (!targetRect.intersect(SkIntToScalar(clipBounds.fLeft), SkIntToScalar(clipBounds.fTop), SkIntToScalar(clipBounds.fRight), SkIntToScalar(clipBounds.fBottom)))
                return;
        }

        // Make sure the paint stays opaque through the layer.
        bool drawsOpaque = paintIsOpaque(it.paint());
        xfersOpaque = xfersOpaque && xfermodeIsOpaque(it.paint(), drawsOpaque);
        preservesOpaque = preservesOpaque && xfermodePreservesOpaque(it.paint(), drawsOpaque);
    }

    // Apply the transform to the tracking space.
    SkMatrix canvasToTargetTransform = transform;
    if (!canvasToTargetTransform.mapRect(&targetRect))
        fillsBounds = false;

    if (fillsBounds && xfersOpaque)
        markRectAsOpaque(targetRect);
    else if (SkRect::Intersects(targetRect, m_opaqueRect) && !preservesOpaque)
        markRectAsNonOpaque(targetRect);
}

void OpaqueRegionSkia::didDrawUnbounded(const SkPaint& paint, bool drawsOpaque)
{
    if (!xfermodePreservesOpaque(paint, drawsOpaque)) {
        // We don't know what was drawn on so just destroy the known opaque area.
        m_opaqueRect = SkRect::MakeEmpty();
    }
}

void OpaqueRegionSkia::markRectAsOpaque(const SkRect& rect)
{
    // We want to keep track of an opaque region but bound its complexity at a constant size.
    // We keep track of the largest rectangle seen by area. If we can add the new rect to this
    // rectangle then we do that, as that is the cheapest way to increase the area returned
    // without increasing the complexity.

    if (rect.isEmpty())
        return;
    if (m_opaqueRect.contains(rect))
        return;
    if (rect.contains(m_opaqueRect)) {
        m_opaqueRect = rect;
        return;
    }

    if (rect.fTop <= m_opaqueRect.fTop && rect.fBottom >= m_opaqueRect.fBottom) {
        if (rect.fLeft < m_opaqueRect.fLeft && rect.fRight >= m_opaqueRect.fLeft)
            m_opaqueRect.fLeft = rect.fLeft;
        if (rect.fRight > m_opaqueRect.fRight && rect.fLeft <= m_opaqueRect.fRight)
            m_opaqueRect.fRight = rect.fRight;
    } else if (rect.fLeft <= m_opaqueRect.fLeft && rect.fRight >= m_opaqueRect.fRight) {
        if (rect.fTop < m_opaqueRect.fTop && rect.fBottom >= m_opaqueRect.fTop)
            m_opaqueRect.fTop = rect.fTop;
        if (rect.fBottom > m_opaqueRect.fBottom && rect.fTop <= m_opaqueRect.fBottom)
            m_opaqueRect.fBottom = rect.fBottom;
    }

    long opaqueArea = (long)m_opaqueRect.width() * (long)m_opaqueRect.height();
    long area = (long)rect.width() * (long)rect.height();
    if (area > opaqueArea)
        m_opaqueRect = rect;
}

void OpaqueRegionSkia::markRectAsNonOpaque(const SkRect& rect)
{
    // We want to keep as much of the current opaque rectangle as we can, so find the one largest
    // rectangle inside m_opaqueRect that does not intersect with |rect|.

    if (rect.contains(m_opaqueRect)) {
        m_opaqueRect.setEmpty();
        return;
    }

    int deltaLeft = rect.fLeft - m_opaqueRect.fLeft;
    int deltaRight = m_opaqueRect.fRight - rect.fRight;
    int deltaTop = rect.fTop - m_opaqueRect.fTop;
    int deltaBottom = m_opaqueRect.fBottom - rect.fBottom;

    // horizontal is the larger of the two rectangles to the left or to the right of |rect| and inside m_opaqueRect.
    // vertical is the larger of the two rectangles above or below |rect| and inside m_opaqueRect.
    SkRect horizontal = m_opaqueRect;
    if (deltaTop > deltaBottom)
        horizontal.fBottom = rect.fTop;
    else
        horizontal.fTop = rect.fBottom;
    SkRect vertical = m_opaqueRect;
    if (deltaLeft > deltaRight)
        vertical.fRight = rect.fLeft;
    else
        vertical.fLeft = rect.fRight;

    if ((long)horizontal.width() * (long)horizontal.height() > (long)vertical.width() * (long)vertical.height())
        m_opaqueRect = horizontal;
    else
        m_opaqueRect = vertical;
}


} // namespace WebCore
