/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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
#include "SkiaFontWin.h"

#include "AffineTransform.h"
#include "PlatformContextSkia.h"
#include "Gradient.h"
#include "Pattern.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkShader.h"
#include "SkTemplates.h"
#include "SkTypeface_win.h"

#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

struct CachedOutlineKey {
    CachedOutlineKey() : font(0), glyph(0), path(0) {}
    CachedOutlineKey(HFONT f, WORD g) : font(f), glyph(g), path(0) {}

    HFONT font;
    WORD glyph;

    // The lifetime of this pointer is managed externally to this class. Be sure
    // to call DeleteOutline to remove items.
    SkPath* path;
};

const bool operator==(const CachedOutlineKey& a, const CachedOutlineKey& b)
{
    return a.font == b.font && a.glyph == b.glyph;
}

struct CachedOutlineKeyHash {
    static unsigned hash(const CachedOutlineKey& key)
    {
        unsigned keyBytes;
        memcpy(&keyBytes, &key.font, sizeof(unsigned));
        return keyBytes + key.glyph;
    }

    static unsigned equal(const CachedOutlineKey& a, const CachedOutlineKey& b)
    {
        return a.font == b.font && a.glyph == b.glyph;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

// The global number of glyph outlines we'll cache.
static const int outlineCacheSize = 256;

typedef ListHashSet<CachedOutlineKey, outlineCacheSize+1, CachedOutlineKeyHash> OutlineCache;

// FIXME: Convert from static constructor to accessor function. WebCore tries to
// avoid global constructors to save on start-up time.
static OutlineCache outlineCache;

static SkScalar FIXEDToSkScalar(FIXED fixed)
{
    SkFixed skFixed;
    memcpy(&skFixed, &fixed, sizeof(SkFixed));
    return SkFixedToScalar(skFixed);
}

// Removes the given key from the cached outlines, also deleting the path.
static void deleteOutline(OutlineCache::iterator deleteMe)
{
    delete deleteMe->path;
    outlineCache.remove(deleteMe);
}

static void addPolyCurveToPath(const TTPOLYCURVE* polyCurve, SkPath* path)
{
    switch (polyCurve->wType) {
    case TT_PRIM_LINE:
        for (WORD i = 0; i < polyCurve->cpfx; i++) {
          path->lineTo(FIXEDToSkScalar(polyCurve->apfx[i].x), -FIXEDToSkScalar(polyCurve->apfx[i].y));
        }
        break;

    case TT_PRIM_QSPLINE:
        // FIXME: doesn't this duplicate points if we do the loop > once?
        for (WORD i = 0; i < polyCurve->cpfx - 1; i++) {
            SkScalar bx = FIXEDToSkScalar(polyCurve->apfx[i].x);
            SkScalar by = FIXEDToSkScalar(polyCurve->apfx[i].y);

            SkScalar cx = FIXEDToSkScalar(polyCurve->apfx[i + 1].x);
            SkScalar cy = FIXEDToSkScalar(polyCurve->apfx[i + 1].y);
            if (i < polyCurve->cpfx - 2) {
                // We're not the last point, compute C.
                cx = SkScalarAve(bx, cx);
                cy = SkScalarAve(by, cy);
            }

            // Need to flip the y coordinates since the font's coordinate system is
            // flipped from ours vertically.
            path->quadTo(bx, -by, cx, -cy);
        }
        break;

    case TT_PRIM_CSPLINE:
        // FIXME
        break;
    }
}

// The size of the glyph path buffer.
static const int glyphPathBufferSize = 4096;

// Fills the given SkPath with the outline for the given glyph index. The font
// currently selected into the given DC is used. Returns true on success.
static bool getPathForGlyph(HDC dc, WORD glyph, SkPath* path)
{
    char buffer[glyphPathBufferSize];
    GLYPHMETRICS gm;
    MAT2 mat = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};  // Each one is (fract,value).

    DWORD totalSize = GetGlyphOutlineW(dc, glyph, GGO_GLYPH_INDEX | GGO_NATIVE,
                                       &gm, glyphPathBufferSize, buffer, &mat);
    if (totalSize == GDI_ERROR)
        return false;

    const char* curGlyph = buffer;
    const char* endGlyph = &buffer[totalSize];
    while (curGlyph < endGlyph) {
        const TTPOLYGONHEADER* polyHeader =
            reinterpret_cast<const TTPOLYGONHEADER*>(curGlyph);
        path->moveTo(FIXEDToSkScalar(polyHeader->pfxStart.x),
                     -FIXEDToSkScalar(polyHeader->pfxStart.y));

        const char* curPoly = curGlyph + sizeof(TTPOLYGONHEADER);
        const char* endPoly = curGlyph + polyHeader->cb;
        while (curPoly < endPoly) {
            const TTPOLYCURVE* polyCurve =
                reinterpret_cast<const TTPOLYCURVE*>(curPoly);
            addPolyCurveToPath(polyCurve, path);
            curPoly += sizeof(WORD) * 2 + sizeof(POINTFX) * polyCurve->cpfx;
        }
        path->close();
        curGlyph += polyHeader->cb;
    }

    return true;
}

// Returns a SkPath corresponding to the give glyph in the given font. The font
// should be selected into the given DC. The returned path is owned by the
// hashtable. Returns 0 on error.
const SkPath* SkiaWinOutlineCache::lookupOrCreatePathForGlyph(HDC hdc, HFONT font, WORD glyph)
{
    CachedOutlineKey key(font, glyph);
    OutlineCache::iterator found = outlineCache.find(key);
    if (found != outlineCache.end()) {
        // Keep in MRU order by removing & reinserting the value.
        key = *found;
        outlineCache.remove(found);
        outlineCache.add(key);
        return key.path;
    }

    key.path = new SkPath;
    if (!getPathForGlyph(hdc, glyph, key.path))
      return 0;

    if (outlineCache.size() > outlineCacheSize)
        // The cache is too big, find the oldest value (first in the list).
        deleteOutline(outlineCache.begin());

    outlineCache.add(key);
    return key.path;
}


void SkiaWinOutlineCache::removePathsForFont(HFONT hfont)
{
    // ListHashSet isn't the greatest structure for deleting stuff out of, but
    // removing entries will be relatively rare (we don't remove fonts much, nor
    // do we draw out own glyphs using these routines much either).
    //
    // We keep a list of all glyphs we're removing which we do in a separate
    // pass.
    Vector<CachedOutlineKey> outlinesToDelete;
    for (OutlineCache::iterator i = outlineCache.begin();
         i != outlineCache.end(); ++i)
        outlinesToDelete.append(*i);

    for (Vector<CachedOutlineKey>::iterator i = outlinesToDelete.begin();
         i != outlinesToDelete.end(); ++i)
        deleteOutline(outlineCache.find(*i));
}

bool windowsCanHandleDrawTextShadow(GraphicsContext *context)
{
    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    ColorSpace shadowColorSpace;

    bool hasShadow = context->getShadow(shadowOffset, shadowBlur, shadowColor, shadowColorSpace);
    return !hasShadow || (!shadowBlur && (shadowColor.alpha() == 255) && (context->fillColor().alpha() == 255));
}

bool windowsCanHandleTextDrawing(GraphicsContext* context)
{
    if (!windowsCanHandleTextDrawingWithoutShadow(context))
        return false;

    // Check for shadow effects.
    if (!windowsCanHandleDrawTextShadow(context))
        return false;

    return true;
}

bool windowsCanHandleTextDrawingWithoutShadow(GraphicsContext* context)
{
    // Check for non-translation transforms. Sometimes zooms will look better in
    // Skia, and sometimes better in Windows. The main problem is that zooming
    // in using Skia will show you the hinted outlines for the smaller size,
    // which look weird. All else being equal, it's better to use Windows' text
    // drawing, so we don't check for zooms.
    const AffineTransform& matrix = context->getCTM();
    if (matrix.b() != 0 || matrix.c() != 0)  // Check for skew.
        return false;

    // Check for stroke effects.
    if (context->platformContext()->getTextDrawingMode() != TextModeFill)
        return false;

    // Check for gradients.
    if (context->fillGradient() || context->strokeGradient())
        return false;

    // Check for patterns.
    if (context->fillPattern() || context->strokePattern())
        return false;

    if (!context->platformContext()->isNativeFontRenderingAllowed())
        return false;

    return true;
}

// Draws the given text string using skia.  Note that gradient or
// pattern may be NULL, in which case a solid colour is used.
static bool skiaDrawText(HFONT hfont,
                         HDC dc,
                         PlatformContextSkia* platformContext,
                         const SkPoint& point,
                         SkPaint* paint,
                         const WORD* glyphs,
                         const int* advances,
                         const GOFFSET* offsets,
                         int numGlyphs)
{
    SkCanvas* canvas = platformContext->canvas();
    if (!platformContext->isNativeFontRenderingAllowed()) {
        SkASSERT(sizeof(WORD) == sizeof(uint16_t));

        // Reserve space for 64 glyphs on the stack. If numGlyphs is larger, the array
        // will dynamically allocate it space for numGlyph glyphs.
        static const size_t kLocalGlyphMax = 64;
        SkAutoSTArray<kLocalGlyphMax, SkPoint> posStorage(numGlyphs);
        SkPoint* pos = posStorage.get();
        SkScalar x = point.fX;
        SkScalar y = point.fY;
        for (int i = 0; i < numGlyphs; i++) {
            pos[i].set(x + (offsets ? offsets[i].du : 0),
                       y + (offsets ? offsets[i].dv : 0));
            x += SkIntToScalar(advances[i]);
        }
        canvas->drawPosText(glyphs, numGlyphs * sizeof(uint16_t), pos, *paint);
    } else {
        float x = point.fX, y = point.fY;

        for (int i = 0; i < numGlyphs; i++) {
            const SkPath* path = SkiaWinOutlineCache::lookupOrCreatePathForGlyph(dc, hfont, glyphs[i]);
            if (!path)
                return false;

            float offsetX = 0.0f, offsetY = 0.0f;
            if (offsets && (offsets[i].du || offsets[i].dv)) {
                offsetX = offsets[i].du;
                offsetY = offsets[i].dv;
            }

            SkPath newPath;
            newPath.addPath(*path, x + offsetX, y + offsetY);
            canvas->drawPath(newPath, *paint);

            x += advances[i];
        }
    }
    return true;
}

static void setupPaintForFont(HFONT hfont, SkPaint* paint)
{
    //  FIXME:
    //  Much of this logic could also happen in
    //  FontCustomPlatformData::fontPlatformData and be cached,
    //  allowing us to avoid talking to GDI at this point.
    //
    LOGFONT info;
    GetObject(hfont, sizeof(info), &info);
    int size = info.lfHeight;
    if (size < 0)
        size = -size; // We don't let GDI dpi-scale us (see SkFontHost_win.cpp).
    paint->setTextSize(SkIntToScalar(size));

    SkTypeface* face = SkCreateTypefaceFromLOGFONT(info);
    paint->setTypeface(face);
    SkSafeUnref(face);
}

bool paintSkiaText(GraphicsContext* context,
                   HFONT hfont,
                   int numGlyphs,
                   const WORD* glyphs,
                   const int* advances,
                   const GOFFSET* offsets,
                   const SkPoint* origin)
{
    HDC dc = GetDC(0);
    HGDIOBJ oldFont = SelectObject(dc, hfont);

    PlatformContextSkia* platformContext = context->platformContext();
    TextDrawingModeFlags textMode = platformContext->getTextDrawingMode();

    // Filling (if necessary). This is the common case.
    SkPaint paint;
    platformContext->setupPaintForFilling(&paint);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    if (!platformContext->isNativeFontRenderingAllowed()) {
        paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
        setupPaintForFont(hfont, &paint);
    }
    bool didFill = false;

    if ((textMode & TextModeFill) && (SkColorGetA(paint.getColor()) || paint.getLooper())) {
        if (!skiaDrawText(hfont, dc, platformContext, *origin, &paint,
                          &glyphs[0], &advances[0], &offsets[0], numGlyphs))
            return false;
        didFill = true;
    }

    // Stroking on top (if necessary).
    if ((textMode & TextModeStroke)
        && platformContext->getStrokeStyle() != NoStroke
        && platformContext->getStrokeThickness() > 0) {

        paint.reset();
        platformContext->setupPaintForStroking(&paint, 0, 0);
        paint.setFlags(SkPaint::kAntiAlias_Flag);
        if (!platformContext->isNativeFontRenderingAllowed()) {
            paint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);
            setupPaintForFont(hfont, &paint);
        }

        if (didFill) {
            // If there is a shadow and we filled above, there will already be
            // a shadow. We don't want to draw it again or it will be too dark
            // and it will go on top of the fill.
            //
            // Note that this isn't strictly correct, since the stroke could be
            // very thick and the shadow wouldn't account for this. The "right"
            // thing would be to draw to a new layer and then draw that layer
            // with a shadow. But this is a lot of extra work for something
            // that isn't normally an issue.
            paint.setLooper(0);
        }

        if (!skiaDrawText(hfont, dc, platformContext, *origin, &paint,
                          &glyphs[0], &advances[0], &offsets[0], numGlyphs))
            return false;
    }

    SelectObject(dc, oldFont);
    ReleaseDC(0, dc);

    return true;
}

}  // namespace WebCore
