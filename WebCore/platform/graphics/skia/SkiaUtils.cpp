// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#include "SkiaUtils.h"

#include "ImageBuffer.h"
#include "SharedBuffer.h"
#include "SkCanvas.h"
#include "SkColorPriv.h"
#include "SkMatrix.h"
#include "SkRegion.h"

namespace WebCore {

void WebCorePointToSkiaPoint(const WebCore::FloatPoint& src, SkPoint* dst)
{
    dst->set(WebCoreFloatToSkScalar(src.x()), WebCoreFloatToSkScalar(src.y()));
}

void WebCoreRectToSkiaRect(const WebCore::IntRect& src, SkRect* dst)
{
    dst->set(SkIntToScalar(src.x()),
             SkIntToScalar(src.y()),
             SkIntToScalar(src.x() + src.width()),
             SkIntToScalar(src.y() + src.height()));
}

void WebCoreRectToSkiaRect(const WebCore::IntRect& src, SkIRect* dst)
{
    dst->set(src.x(),
             src.y(),
             src.x() + src.width(),
             src.y() + src.height());
}

void WebCoreRectToSkiaRect(const WebCore::FloatRect& src, SkRect* dst)
{
    dst->set(WebCoreFloatToSkScalar(src.x()),
             WebCoreFloatToSkScalar(src.y()),
             WebCoreFloatToSkScalar(src.x() + src.width()),
             WebCoreFloatToSkScalar(src.y() + src.height()));
}

void WebCoreRectToSkiaRect(const WebCore::FloatRect& src, SkIRect* dst)
{
    dst->set(SkScalarRound(WebCoreFloatToSkScalar(src.x())),
             SkScalarRound(WebCoreFloatToSkScalar(src.y())),
             SkScalarRound(WebCoreFloatToSkScalar(src.x() + src.width())),
             SkScalarRound(WebCoreFloatToSkScalar(src.y() + src.height())));
}

static const struct CompositOpToPorterDuffMode {
    uint8_t mCompositOp;
    uint8_t mPorterDuffMode;
} gMapCompositOpsToPorterDuffModes[] = {
    { WebCore::CompositeClear,           SkPorterDuff::kClear_Mode },
    { WebCore::CompositeCopy,            SkPorterDuff::kSrcOver_Mode },  // TODO
    { WebCore::CompositeSourceOver,      SkPorterDuff::kSrcOver_Mode },
    { WebCore::CompositeSourceIn,        SkPorterDuff::kSrcIn_Mode },
    { WebCore::CompositeSourceOut,       SkPorterDuff::kSrcOut_Mode },
    { WebCore::CompositeSourceAtop,      SkPorterDuff::kSrcATop_Mode },
    { WebCore::CompositeDestinationOver, SkPorterDuff::kDstOver_Mode },
    { WebCore::CompositeDestinationIn,   SkPorterDuff::kDstIn_Mode },
    { WebCore::CompositeDestinationOut,  SkPorterDuff::kDstOut_Mode },
    { WebCore::CompositeDestinationAtop, SkPorterDuff::kDstATop_Mode },
    { WebCore::CompositeXOR,             SkPorterDuff::kXor_Mode },
    { WebCore::CompositePlusDarker,      SkPorterDuff::kDarken_Mode },
    { WebCore::CompositeHighlight,       SkPorterDuff::kSrcOver_Mode },  // TODO
    { WebCore::CompositePlusLighter,     SkPorterDuff::kLighten_Mode }
};

SkPorterDuff::Mode WebCoreCompositeToSkiaComposite(WebCore::CompositeOperator op)
{
    const CompositOpToPorterDuffMode* table = gMapCompositOpsToPorterDuffModes;
    
    for (unsigned i = 0; i < SK_ARRAY_COUNT(gMapCompositOpsToPorterDuffModes); i++) {
        if (table[i].mCompositOp == op) {
            return (SkPorterDuff::Mode)table[i].mPorterDuffMode;
        }
    }

    SkDEBUGF(("GraphicsContext::setCompositeOperation uknown CompositOperator %d\n", op));
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

WebCore::Color SkPMColorToWebCoreColor(SkPMColor pm)
{
    return SkPMColorToColor(pm);
}

void IntersectRectAndRegion(const SkRegion& region, const SkRect& src_rect,
                            SkRect* dest_rect) {
    // The cliperator requires an int rect, so we round out.
    SkIRect src_rect_rounded;
    src_rect.roundOut(&src_rect_rounded);

    // The Cliperator will iterate over a bunch of rects where our transformed
    // rect and the clipping region (which may be non-square) overlap.
    SkRegion::Cliperator cliperator(region, src_rect_rounded);
    if (cliperator.done()) {
        dest_rect->setEmpty();
        return;
    }

    // Get the union of all visible rects in the clip that overlap our bitmap.
    SkIRect cur_visible_rect = cliperator.rect();
    cliperator.next();
    while (!cliperator.done()) {
        cur_visible_rect.join(cliperator.rect());
        cliperator.next();
    }

    dest_rect->set(cur_visible_rect);
}

void ClipRectToCanvas(const SkCanvas& canvas, const SkRect& src_rect,
                      SkRect* dest_rect) {
    // Translate into the canvas' coordinate space. This is where the clipping
    // region applies.
    SkRect transformed_src;
    canvas.getTotalMatrix().mapRect(&transformed_src, src_rect);

    // Do the intersection.
    SkRect transformed_dest;
    IntersectRectAndRegion(canvas.getTotalClip(), transformed_src,
                           &transformed_dest);

    // Now transform it back into world space.
    SkMatrix inverse_transform;
    canvas.getTotalMatrix().invert(&inverse_transform);
    inverse_transform.mapRect(dest_rect, transformed_dest);
}

bool SkPathContainsPoint(SkPath* orig_path, WebCore::FloatPoint point, SkPath::FillType ft)
{
    SkRegion    rgn, clip;

    SkPath::FillType orig_ft = orig_path->getFillType();    // save

    const SkPath* path = orig_path;
    SkPath scaled_path;
    int scale = 1;

    SkRect bounds;
    orig_path->computeBounds(&bounds, SkPath::kFast_BoundsType);

    // We can immediately return false if the point is outside the bounding rect
    if (!bounds.contains(SkFloatToScalar(point.x()), SkFloatToScalar(point.y())))
      return false;

    orig_path->setFillType(ft);

    // Skia has trouble with coordinates close to the max signed 16-bit values
    // If we have those, we need to scale. 
    //
    // TODO: remove this code once Skia is patched to work properly with large
    // values
    const SkScalar kMaxCoordinate = SkIntToScalar(1<<15);
    SkScalar biggest_coord = std::max(std::max(std::max(
      bounds.fRight, bounds.fBottom), -bounds.fLeft), -bounds.fTop);

    if (biggest_coord > kMaxCoordinate) {
      scale = SkScalarCeil(SkScalarDiv(biggest_coord, kMaxCoordinate));

      SkMatrix m;
      m.setScale(SkScalarInvert(SkIntToScalar(scale)), 
                 SkScalarInvert(SkIntToScalar(scale)));
      orig_path->transform(m, &scaled_path);
      path = &scaled_path;
    }

    int x = (int)floorf(point.x() / scale);
    int y = (int)floorf(point.y() / scale);
    clip.setRect(x, y, x + 1, y + 1);

    bool contains = rgn.setPath(*path, clip);

    orig_path->setFillType(orig_ft);    // restore
    return contains;
}

GraphicsContext* scratchContext()
{
    static ImageBuffer* scratch = NULL;
    if (!scratch)
        scratch = ImageBuffer::create(IntSize(1, 1), false).release();
    // We don't bother checking for failure creating the ImageBuffer, since our
    // ImageBuffer initializer won't fail.
    return scratch->context();
}

}  // namespace WebCore
