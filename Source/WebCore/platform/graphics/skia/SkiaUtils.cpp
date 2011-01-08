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

static const struct CompositOpToXfermodeMode {
    uint8_t mCompositOp;
    uint8_t m_xfermodeMode;
} gMapCompositOpsToXfermodeModes[] = {
    { CompositeClear,           SkXfermode::kClear_Mode },
    { CompositeCopy,            SkXfermode::kSrc_Mode },
    { CompositeSourceOver,      SkXfermode::kSrcOver_Mode },
    { CompositeSourceIn,        SkXfermode::kSrcIn_Mode },
    { CompositeSourceOut,       SkXfermode::kSrcOut_Mode },
    { CompositeSourceAtop,      SkXfermode::kSrcATop_Mode },
    { CompositeDestinationOver, SkXfermode::kDstOver_Mode },
    { CompositeDestinationIn,   SkXfermode::kDstIn_Mode },
    { CompositeDestinationOut,  SkXfermode::kDstOut_Mode },
    { CompositeDestinationAtop, SkXfermode::kDstATop_Mode },
    { CompositeXOR,             SkXfermode::kXor_Mode },
    { CompositePlusDarker,      SkXfermode::kDarken_Mode },
    { CompositeHighlight,       SkXfermode::kSrcOver_Mode },  // TODO
    { CompositePlusLighter,     SkXfermode::kPlus_Mode }
};

SkXfermode::Mode WebCoreCompositeToSkiaComposite(CompositeOperator op)
{
    const CompositOpToXfermodeMode* table = gMapCompositOpsToXfermodeModes;
    
    for (unsigned i = 0; i < SK_ARRAY_COUNT(gMapCompositOpsToXfermodeModes); i++) {
        if (table[i].mCompositOp == op)
            return (SkXfermode::Mode)table[i].m_xfermodeMode;
    }

    SkDEBUGF(("GraphicsContext::setPlatformCompositeOperation unknown CompositeOperator %d\n", op));
    return SkXfermode::kSrcOver_Mode; // fall-back
}

static U8CPU InvScaleByte(U8CPU component, uint32_t scale)
{
    SkASSERT(component == (uint8_t)component);
    return (component * scale + 0x8000) >> 16;
}

SkColor SkPMColorToColor(SkPMColor pm)
{
    if (!pm)
        return 0;
    unsigned a = SkGetPackedA32(pm);
    if (!a) {
        // A zero alpha value when there are non-zero R, G, or B channels is an
        // invalid premultiplied color (since all channels should have been
        // multiplied by 0 if a=0).
        SkASSERT(false); 
        // In production, return 0 to protect against division by zero.
        return 0;
    }
    
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

    SkRect bounds = originalPath->getBounds();

    // We can immediately return false if the point is outside the bounding
    // rect.  We don't use bounds.contains() here, since it would exclude
    // points on the right and bottom edges of the bounding rect, and we want
    // to include them.
    SkScalar fX = SkFloatToScalar(point.x());
    SkScalar fY = SkFloatToScalar(point.y());
    if (fX < bounds.fLeft || fX > bounds.fRight || fY < bounds.fTop || fY > bounds.fBottom)
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
    clip.setRect(x - 1, y - 1, x + 1, y + 1);

    bool contains = rgn.setPath(*path, clip);

    originalPath->setFillType(originalFillType);
    return contains;
}

GraphicsContext* scratchContext()
{
    static ImageBuffer* scratch = ImageBuffer::create(IntSize(1, 1)).leakPtr();
    // We don't bother checking for failure creating the ImageBuffer, since our
    // ImageBuffer initializer won't fail.
    return scratch->context();
}

}  // namespace WebCore
