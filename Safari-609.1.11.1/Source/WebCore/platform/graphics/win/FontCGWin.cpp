/*
 * Copyright (C) 2006-2009, 2013, 2016 Apple Inc.  All rights reserved.
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
#include "FontCascade.h"

#if USE(CG)

#include "AffineTransform.h"
#include "FloatConversion.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "UniscribeController.h"
#include "WebCoreTextRenderer.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/MathExtras.h>

namespace WebCore {

static inline CGFloat toCGFloat(FIXED f)
{
    return f.value + f.fract / CGFloat(65536.0);
}

static CGPathRef createPathForGlyph(HDC hdc, Glyph glyph)
{
    CGMutablePathRef path = CGPathCreateMutable();

    static const MAT2 identity = { 0, 1,  0, 0,  0, 0,  0, 1 };
    GLYPHMETRICS glyphMetrics;
    // GGO_NATIVE matches the outline perfectly when Windows font smoothing is off.
    // GGO_NATIVE | GGO_UNHINTED does not match perfectly either when Windows font smoothing is on or off.
    DWORD outlineLength = GetGlyphOutline(hdc, glyph, GGO_GLYPH_INDEX | GGO_NATIVE, &glyphMetrics, 0, 0, &identity);
    ASSERT(outlineLength >= 0);
    if (outlineLength < 0)
        return path;

    Vector<UInt8> outline(outlineLength);
    GetGlyphOutline(hdc, glyph, GGO_GLYPH_INDEX | GGO_NATIVE, &glyphMetrics, outlineLength, outline.data(), &identity);

    unsigned offset = 0;
    while (offset < outlineLength) {
        LPTTPOLYGONHEADER subpath = reinterpret_cast<LPTTPOLYGONHEADER>(outline.data() + offset);
        ASSERT(subpath->dwType == TT_POLYGON_TYPE);
        if (subpath->dwType != TT_POLYGON_TYPE)
            return path;

        CGPathMoveToPoint(path, 0, toCGFloat(subpath->pfxStart.x), toCGFloat(subpath->pfxStart.y));

        unsigned subpathOffset = sizeof(*subpath);
        while (subpathOffset < subpath->cb) {
            LPTTPOLYCURVE segment = reinterpret_cast<LPTTPOLYCURVE>(reinterpret_cast<UInt8*>(subpath) + subpathOffset);
            switch (segment->wType) {
                case TT_PRIM_LINE:
                    for (unsigned i = 0; i < segment->cpfx; i++)
                        CGPathAddLineToPoint(path, 0, toCGFloat(segment->apfx[i].x), toCGFloat(segment->apfx[i].y));
                    break;

                case TT_PRIM_QSPLINE:
                    for (unsigned i = 0; i < segment->cpfx; i++) {
                        CGFloat x = toCGFloat(segment->apfx[i].x);
                        CGFloat y = toCGFloat(segment->apfx[i].y);
                        CGFloat cpx;
                        CGFloat cpy;

                        if (i == segment->cpfx - 2) {
                            cpx = toCGFloat(segment->apfx[i + 1].x);
                            cpy = toCGFloat(segment->apfx[i + 1].y);
                            i++;
                        } else {
                            cpx = (toCGFloat(segment->apfx[i].x) + toCGFloat(segment->apfx[i + 1].x)) / 2;
                            cpy = (toCGFloat(segment->apfx[i].y) + toCGFloat(segment->apfx[i + 1].y)) / 2;
                        }

                        CGPathAddQuadCurveToPoint(path, 0, x, y, cpx, cpy);
                    }
                    break;

                case TT_PRIM_CSPLINE:
                    for (unsigned i = 0; i < segment->cpfx; i += 3) {
                        CGFloat cp1x = toCGFloat(segment->apfx[i].x);
                        CGFloat cp1y = toCGFloat(segment->apfx[i].y);
                        CGFloat cp2x = toCGFloat(segment->apfx[i + 1].x);
                        CGFloat cp2y = toCGFloat(segment->apfx[i + 1].y);
                        CGFloat x = toCGFloat(segment->apfx[i + 2].x);
                        CGFloat y = toCGFloat(segment->apfx[i + 2].y);

                        CGPathAddCurveToPoint(path, 0, cp1x, cp1y, cp2x, cp2y, x, y);
                    }
                    break;

                default:
                    ASSERT_NOT_REACHED();
                    return path;
            }

            subpathOffset += sizeof(*segment) + (segment->cpfx - 1) * sizeof(segment->apfx[0]);
        }
        CGPathCloseSubpath(path);
        offset += subpath->cb;
    }
    return path;
}

void FontCascade::drawGlyphs(GraphicsContext& graphicsContext, const Font& font, const GlyphBuffer& glyphBuffer,
    unsigned from, unsigned numGlyphs, const FloatPoint& point, FontSmoothingMode smoothingMode)
{
    CGContextRef cgContext = graphicsContext.platformContext();
    bool shouldUseFontSmoothing = WebCoreShouldUseFontSmoothing();

    switch (smoothingMode) {
    case FontSmoothingMode::Antialiased: {
        graphicsContext.setShouldAntialias(true);
        shouldUseFontSmoothing = false;
        break;
    }
    case FontSmoothingMode::SubpixelAntialiased: {
        graphicsContext.setShouldAntialias(true);
        shouldUseFontSmoothing = true;
        break;
    }
    case FontSmoothingMode::NoSmoothing: {
        graphicsContext.setShouldAntialias(false);
        shouldUseFontSmoothing = false;
        break;
    }
    case FontSmoothingMode::AutoSmoothing: {
        // For the AutoSmooth case, don't do anything! Keep the default settings.
        break; 
    }
    default: 
        ASSERT_NOT_REACHED();
    }

    uint32_t oldFontSmoothingStyle = FontCascade::setFontSmoothingStyle(cgContext, shouldUseFontSmoothing);

    const FontPlatformData& platformData = font.platformData();

    CGContextSetFont(cgContext, platformData.cgFont());

    CGAffineTransform matrix = CGAffineTransformIdentity;
    matrix.b = -matrix.b;
    matrix.d = -matrix.d;

    if (platformData.syntheticOblique()) {
        static float skew = -tanf(syntheticObliqueAngle() * piFloat / 180.0f);
        matrix = CGAffineTransformConcat(matrix, CGAffineTransformMake(1, 0, skew, 1, 0, 0));
    }

    CGAffineTransform savedMatrix = CGContextGetTextMatrix(cgContext);
    CGContextSetTextMatrix(cgContext, matrix);

    CGContextSetFontSize(cgContext, platformData.size());
    FontCascade::setCGContextFontRenderingStyle(cgContext, font.platformData().isSystemFont(), false, font.platformData().useGDI());

    FloatSize shadowOffset;
    float shadowBlur;
    Color shadowColor;
    graphicsContext.getShadow(shadowOffset, shadowBlur, shadowColor);

    bool hasSimpleShadow = graphicsContext.textDrawingMode() == TextModeFill && shadowColor.isValid() && !shadowBlur && (!graphicsContext.shadowsIgnoreTransforms() || graphicsContext.getCTM().isIdentityOrTranslationOrFlipped());
    if (hasSimpleShadow) {
        // Paint simple shadows ourselves instead of relying on CG shadows, to avoid losing subpixel antialiasing.
        graphicsContext.clearShadow();
        Color fillColor = graphicsContext.fillColor();
        Color shadowFillColor(shadowColor.red(), shadowColor.green(), shadowColor.blue(), shadowColor.alpha() * fillColor.alpha() / 255);
        graphicsContext.setFillColor(shadowFillColor);
        float shadowTextX = point.x() + shadowOffset.width();
        // If shadows are ignoring transforms, then we haven't applied the Y coordinate flip yet, so down is negative.
        float shadowTextY = point.y() + shadowOffset.height() * (graphicsContext.shadowsIgnoreTransforms() ? -1 : 1);
        CGContextSetTextPosition(cgContext, shadowTextX, shadowTextY);
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
        if (font.syntheticBoldOffset()) {
            CGContextSetTextPosition(cgContext, point.x() + shadowOffset.width() + font.syntheticBoldOffset(), point.y() + shadowOffset.height());
            CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
        }
        graphicsContext.setFillColor(fillColor);
    }

    CGContextSetTextPosition(cgContext, point.x(), point.y());
    CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
    if (font.syntheticBoldOffset()) {
        CGContextSetTextPosition(cgContext, point.x() + font.syntheticBoldOffset(), point.y());
        CGContextShowGlyphsWithAdvances(cgContext, glyphBuffer.glyphs(from), static_cast<const CGSize*>(glyphBuffer.advances(from)), numGlyphs);
    }

    if (hasSimpleShadow)
        graphicsContext.setShadow(shadowOffset, shadowBlur, shadowColor);

    FontCascade::setFontSmoothingStyle(cgContext, oldFontSmoothingStyle);
    CGContextSetTextMatrix(cgContext, savedMatrix);
}

constexpr uint32_t kCGFontSmoothingStyleMinimum = (1 << 4);
constexpr uint32_t kCGFontSmoothingStyleLight = (2 << 4);
constexpr uint32_t kCGFontSmoothingStyleMedium = (3 << 4);
constexpr uint32_t kCGFontSmoothingStyleHeavy = (4 << 4);

constexpr int fontSmoothingLevelMedium = 2;
constexpr CGFloat antialiasingGamma = 2.3;

double FontCascade::s_fontSmoothingContrast = 2;
uint32_t FontCascade::s_fontSmoothingType = kCGFontSmoothingStyleMedium;
int FontCascade::s_fontSmoothingLevel = fontSmoothingLevelMedium;
bool FontCascade::s_systemFontSmoothingEnabled;
uint32_t FontCascade::s_systemFontSmoothingType;
bool FontCascade::s_systemFontSmoothingSet;

void FontCascade::setFontSmoothingLevel(int level)
{
    const uint32_t smoothingType[] = { 
        0, // FontSmoothingTypeStandard
        kCGFontSmoothingStyleLight, // FontSmoothingTypeLight
        kCGFontSmoothingStyleMedium, // FontSmoothingTypeMedium
        kCGFontSmoothingStyleHeavy, // FontSmoothingTypeStrong
    };

    if (level < 0 || static_cast<size_t>(level) > ARRAYSIZE(smoothingType))
        return;

    s_fontSmoothingType = smoothingType[level];
    s_fontSmoothingLevel = level;
}

static void setCGFontSmoothingStyle(CGContextRef cgContext, uint32_t smoothingType, bool fontAllowsSmoothing = true)
{
    if (smoothingType) {
        CGContextSetShouldSmoothFonts(cgContext, fontAllowsSmoothing);
        CGContextSetFontSmoothingStyle(cgContext, smoothingType);
    } else
        CGContextSetShouldSmoothFonts(cgContext, false);
}

uint32_t FontCascade::setFontSmoothingStyle(CGContextRef cgContext, bool fontAllowsSmoothing)
{
    uint32_t oldFontSmoothingStyle = 0;
    if (CGContextGetShouldSmoothFonts(cgContext))
        oldFontSmoothingStyle = CGContextGetFontSmoothingStyle(cgContext);
    setCGFontSmoothingStyle(cgContext, s_fontSmoothingType, fontAllowsSmoothing);

    return oldFontSmoothingStyle;
}

void FontCascade::setFontSmoothingContrast(CGFloat contrast)
{
    s_fontSmoothingContrast = contrast;
}

static float clearTypeContrast()
{
    const WCHAR referenceCharacter = '\\';
    static UINT lastContrast = 2000;
    static float gamma = 2;
    UINT contrast;

    if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0, &contrast, 0) || contrast == lastContrast)
        return gamma;

    lastContrast = contrast;

    auto dc = adoptGDIObject(::CreateCompatibleDC(0));

    HGDIOBJ oldHFONT = ::SelectObject(dc.get(), GetStockObject(DEFAULT_GUI_FONT));
    GLYPHMETRICS glyphMetrics;

    static const MAT2 identity = { 0, 1,  0, 0,  0, 0,  0, 1 };
    if (::GetGlyphOutline(dc.get(), referenceCharacter, GGO_METRICS, &glyphMetrics, 0, 0, &identity) == GDI_ERROR)
        return contrast / 1000.0f;

    BITMAPINFO bitmapInfo;
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    bitmapInfo.bmiHeader.biClrImportant = 0;
    bitmapInfo.bmiHeader.biWidth = glyphMetrics.gmBlackBoxX;
    bitmapInfo.bmiHeader.biHeight = -static_cast<int>(glyphMetrics.gmBlackBoxY);
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biClrUsed = 0;

    uint8_t* pixels = nullptr;
    auto bitmap = adoptGDIObject(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels), 0, 0));
    if (!bitmap)
        return contrast / 1000.0f;

    HGDIOBJ oldBitmap = ::SelectObject(dc.get(), bitmap.get());

    BITMAP bmpInfo;
    ::GetObject(bitmap.get(), sizeof(bmpInfo), &bmpInfo);
    memset(pixels, 0, glyphMetrics.gmBlackBoxY * bmpInfo.bmWidthBytes);

    ::SetBkMode(dc.get(), OPAQUE);
    ::SetTextAlign(dc.get(), TA_LEFT | TA_TOP);

    ::SetTextColor(dc.get(), RGB(255, 255, 255));
    ::SetBkColor(dc.get(), RGB(0, 0, 0));
    ::ExtTextOutW(dc.get(), 0, 0, 0, 0, &referenceCharacter, 1, 0);

    uint8_t* referencePixel = nullptr;
    uint8_t whiteReferenceValue = 0;
    for (size_t i = 0; i < glyphMetrics.gmBlackBoxY && !referencePixel; ++i) {
        for (size_t j = 0; j < 4 * glyphMetrics.gmBlackBoxX; ++j) {
            whiteReferenceValue = pixels[i * bmpInfo.bmWidthBytes + j];
            // Look for a pixel value in the range that allows us to estimate
            // gamma within 0.1 without an error.
            if (whiteReferenceValue > 32 && whiteReferenceValue < 240) {
                referencePixel = pixels + i * bmpInfo.bmWidthBytes + j;
                break;
            }
        }
    }

    if (referencePixel) {
        ::SetTextColor(dc.get(), RGB(0, 0, 0));
        ::SetBkColor(dc.get(), RGB(255, 255, 255));
        ::ExtTextOutW(dc.get(), 0, 0, 0, 0, &referenceCharacter, 1, 0);
        uint8_t blackReferenceValue = *referencePixel;

        float minDelta = 1;
        for (float g = 1; g < 2.3f; g += 0.1f) {
            float delta = fabs(powf((whiteReferenceValue / 255.0f), g) + powf((blackReferenceValue / 255.0f), g) - 1);
            if (delta < minDelta) {
                minDelta = delta;
                gamma = g;
            }
        }
    } else
        gamma = contrast / 1000.0f;

    ::SelectObject(dc.get(), oldBitmap);
    ::SelectObject(dc.get(), oldHFONT);

    return gamma;
}

void FontCascade::systemFontSmoothingChanged()
{
    ::SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &s_systemFontSmoothingEnabled, 0);
    ::SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &s_systemFontSmoothingType, 0);
    s_fontSmoothingContrast = clearTypeContrast();
    s_systemFontSmoothingSet = true;
}

void FontCascade::setCGContextFontRenderingStyle(CGContextRef cgContext, bool isSystemFont, bool /*isPrinterFont*/, bool usePlatformNativeGlyphs)
{
    bool shouldAntialias = true;
    bool maySubpixelPosition = true;
    CGFloat contrast = 2;
    if (usePlatformNativeGlyphs) {
        // <rdar://6564501> GDI can't subpixel-position, so don't bother asking.
        maySubpixelPosition = false;
        if (!s_systemFontSmoothingSet)
            systemFontSmoothingChanged();
        contrast = s_fontSmoothingContrast;
        shouldAntialias = s_systemFontSmoothingEnabled;
        if (s_systemFontSmoothingType == FE_FONTSMOOTHINGSTANDARD) {
            CGContextSetFontSmoothingStyle(cgContext, kCGFontSmoothingStyleMinimum);
            contrast = antialiasingGamma;
        }
    }
    CGContextSetFontSmoothingContrast(cgContext, contrast);
    CGContextSetShouldUsePlatformNativeGlyphs(cgContext, usePlatformNativeGlyphs);
    CGContextSetShouldAntialiasFonts(cgContext, shouldAntialias);
    CGAffineTransform contextTransform = CGContextGetCTM(cgContext);
    bool isPureTranslation = contextTransform.a == 1 && (contextTransform.d == 1 || contextTransform.d == -1) && !contextTransform.b && !contextTransform.c;
    CGContextSetShouldSubpixelPositionFonts(cgContext, maySubpixelPosition && (isSystemFont || !isPureTranslation));
    CGContextSetShouldSubpixelQuantizeFonts(cgContext, isPureTranslation);
}

static inline CGFontRenderingStyle renderingStyleForFont(bool isSystemFont, bool isPrinterFont)
{
    // FIXME: Need to support a minimum antialiased font size.

    if (isSystemFont || isPrinterFont)
        return kCGFontRenderingStyleAntialiasing | kCGFontRenderingStyleSubpixelPositioning | kCGFontRenderingStyleSubpixelQuantization;

    return kCGFontRenderingStyleAntialiasing;
}

void FontCascade::getPlatformGlyphAdvances(CGFontRef font, const CGAffineTransform& m, bool isSystemFont, bool isPrinterFont, CGGlyph glyph, CGSize& advance)
{
    CGFontRenderingStyle style = renderingStyleForFont(isSystemFont, isPrinterFont);
    CGFontGetGlyphAdvancesForStyle(font, &m, style, &glyph, 1, &advance);

    // <rdar://problem/7761165> The GDI back end in Core Graphics sometimes returns advances that
    // differ from what the font's hmtx table specifies. The following code corrects that.
    CFDataRef hmtxTable = CGFontCopyTableForTag(font, 'hmtx');
    if (!hmtxTable)
        return;
    CFDataRef hheaTable = CGFontCopyTableForTag(font, 'hhea');
    if (!hheaTable) {
        CFRelease(hmtxTable);
        return;
    }

    const CFIndex hheaTableSize = 36;
    const ptrdiff_t hheaTableNumberOfHMetricsOffset = 34;
    if (CFDataGetLength(hheaTable) < hheaTableSize) {
        CFRelease(hmtxTable);
        CFRelease(hheaTable);
        return;
    }

    unsigned short numberOfHMetrics = *reinterpret_cast<const unsigned short*>(CFDataGetBytePtr(hheaTable) + hheaTableNumberOfHMetricsOffset);
    numberOfHMetrics = ((numberOfHMetrics & 0xFF) << 8) | (numberOfHMetrics >> 8);
    if (!numberOfHMetrics) {
        CFRelease(hmtxTable);
        CFRelease(hheaTable);
        return;
    }

    if (glyph >= numberOfHMetrics)
        glyph = numberOfHMetrics - 1;

    if (CFDataGetLength(hmtxTable) < 4 * (glyph + 1)) {
        CFRelease(hmtxTable);
        CFRelease(hheaTable);
        return;
    }

    unsigned short advanceInDesignUnits = *reinterpret_cast<const unsigned short*>(CFDataGetBytePtr(hmtxTable) + 4 * glyph);
    advanceInDesignUnits = ((advanceInDesignUnits & 0xFF) << 8) | (advanceInDesignUnits >> 8);
    CGSize horizontalAdvance = CGSizeMake(static_cast<CGFloat>(advanceInDesignUnits) / CGFontGetUnitsPerEm(font), 0);
    horizontalAdvance = CGSizeApplyAffineTransform(horizontalAdvance, m);
    advance.width = horizontalAdvance.width;
    if (!(style & kCGFontRenderingStyleSubpixelPositioning))
        advance.width = roundf(advance.width);

    CFRelease(hheaTable);
    CFRelease(hmtxTable);
}

}

#endif
