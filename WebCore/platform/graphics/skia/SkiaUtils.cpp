/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
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

#include "SkiaUtils.h"

#include "ImageBuffer.h"
#include "SharedBuffer.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkMatrix.h"
#include "SkRegion.h"

namespace WebCore {

static const struct CompositOpToPorterDuffMode {
    uint8_t mCompositOp;
    uint8_t mPorterDuffMode;
} gMapCompositOpsToPorterDuffModes[] = {
    { CompositeClear,           SkPorterDuff::kClear_Mode },
    { CompositeCopy,            SkPorterDuff::kSrcOver_Mode },  // TODO
    { CompositeSourceOver,      SkPorterDuff::kSrcOver_Mode },
    { CompositeSourceIn,        SkPorterDuff::kSrcIn_Mode },
    { CompositeSourceOut,       SkPorterDuff::kSrcOut_Mode },
    { CompositeSourceAtop,      SkPorterDuff::kSrcATop_Mode },
    { CompositeDestinationOver, SkPorterDuff::kDstOver_Mode },
    { CompositeDestinationIn,   SkPorterDuff::kDstIn_Mode },
    { CompositeDestinationOut,  SkPorterDuff::kDstOut_Mode },
    { CompositeDestinationAtop, SkPorterDuff::kDstATop_Mode },
    { CompositeXOR,             SkPorterDuff::kXor_Mode },
    { CompositePlusDarker,      SkPorterDuff::kDarken_Mode },
    { CompositeHighlight,       SkPorterDuff::kSrcOver_Mode },  // TODO
    { CompositePlusLighter,     SkPorterDuff::kLighten_Mode }
};

SkPorterDuff::Mode WebCoreCompositeToSkiaComposite(CompositeOperator op)
{
    const CompositOpToPorterDuffMode* table = gMapCompositOpsToPorterDuffModes;
    
    for (unsigned i = 0; i < SK_ARRAY_COUNT(gMapCompositOpsToPorterDuffModes); i++) {
        if (table[i].mCompositOp == op)
            return (SkPorterDuff::Mode)table[i].mPorterDuffMode;
    }

    SkDEBUGF(("GraphicsContext::setCompositeOperation uknown CompositeOperator %d\n", op));
    return SkPorterDuff::kSrcOver_Mode; // fall-back
}

static U8CPU InvScaleByte(U8CPU component, uint32_t scale)
{
    SkASSERT(component == (uint8_t)component);
    return (component * scale + 0x8000) >> 16;
}

SkColor SkPMColorToColor(SkPMColor pm)
{
    if (0 == pm)
        return 0;
    
    unsigned a = SkGetPackedA32(pm);
    uint32_t scale = (255 << 16) / a;
    
    return SkColorSetARGB(a,
                          InvScaleByte(SkGetPackedR32(pm), scale),
                          InvScaleByte(SkGetPackedG32(pm), scale),
                          InvScaleByte(SkGetPackedB32(pm), scale));
}

Color SkPMColorToWebCoreColor(SkPMColor pm)
{
    return SkPMColorToColor(pm);
}

void IntersectRectAndRegion(const SkRegion& region, const SkRect& srcRect, SkRect* destRect) {
    // The cliperator requires an int rect, so we round out.
    SkIRect srcRectRounded;
    srcRect.roundOut(&srcRectRounded);

    // The Cliperator will iterate over a bunch of rects where our transformed
    // rect and the clipping region (which may be non-square) overlap.
    SkRegion::Cliperator cliperator(region, srcRectRounded);
    if (cliperator.done()) {
        destRect->setEmpty();
        return;
    }

    // Get the union of all visible rects in the clip that overlap our bitmap.
    SkIRect currentVisibleRect = cliperator.rect();
    cliperator.next();
    while (!cliperator.done()) {
        currentVisibleRect.join(cliperator.rect());
        cliperator.next();
    }

    destRect->set(currentVisibleRect);
}

void ClipRectToCanvas(const SkCanvas& canvas, const SkRect& srcRect, SkRect* destRect) {
    // Translate into the canvas' coordinate space. This is where the clipping
    // region applies.
    SkRect transformedSrc;
    canvas.getTotalMatrix().mapRect(&transformedSrc, srcRect);

    // Do the intersection.
    SkRect transformedDest;
    IntersectRectAndRegion(canvas.getTotalClip(), transformedSrc, &transformedDest);

    // Now transform it back into world space.
    SkMatrix inverseTransform;
    canvas.getTotalMatrix().invert(&inverseTransform);
    inverseTransform.mapRect(destRect, transformedDest);
}

bool SkPathContainsPoint(SkPath* originalPath, const FloatPoint& point, SkPath::FillType ft)
{
    SkRegion rgn;
    SkRegion clip;

    SkPath::FillType originalFillType = originalPath->getFillType();

    const SkPath* path = originalPath;
    SkPath scaledPath;
    int scale = 1;

    SkRect bounds;
    // FIXME:  This #ifdef can go away once we're firmly using the new Skia.
    // During the transition, this makes the code compatible with both versions.
#ifdef SK_USE_OLD_255_TO_256
    bounds = originalPath->getBounds();
#else
    originalPath->computeBounds(&bounds, SkPath::kFast_BoundsType);
#endif

    // We can immediately return false if the point is outside the bounding rect
    if (!bounds.contains(SkFloatToScalar(point.x()), SkFloatToScalar(point.y())))
        return false;

    originalPath->setFillType(ft);

    // Skia has trouble with coordinates close to the max signed 16-bit values
    // If we have those, we need to scale. 
    //
    // TODO: remove this code once Skia is patched to work properly with large
    // values
    const SkScalar kMaxCoordinate = SkIntToScalar(1<<15);
    SkScalar biggestCoord = std::max(std::max(std::max(bounds.fRight, bounds.fBottom), -bounds.fLeft), -bounds.fTop);

    if (biggestCoord > kMaxCoordinate) {
        scale = SkScalarCeil(SkScalarDiv(biggestCoord, kMaxCoordinate));

        SkMatrix m;
        m.setScale(SkScalarInvert(SkIntToScalar(scale)), SkScalarInvert(SkIntToScalar(scale)));
        originalPath->transform(m, &scaledPath);
        path = &scaledPath;
    }

    int x = static_cast<int>(floorf(point.x() / scale));
    int y = static_cast<int>(floorf(point.y() / scale));
    clip.setRect(x, y, x + 1, y + 1);

    bool contains = rgn.setPath(*path, clip);

    originalPath->setFillType(originalFillType);
    return contains;
}

GraphicsContext* scratchContext()
{
    static ImageBuffer* scratch = 0;
    if (!scratch)
        scratch = ImageBuffer::create(IntSize(1, 1), false).release();
    // We don't bother checking for failure creating the ImageBuffer, since our
    // ImageBuffer initializer won't fail.
    return scratch->context();
}

}  // namespace WebCore
