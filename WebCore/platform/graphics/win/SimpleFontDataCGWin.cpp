/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleFontData.h"

#include <winsock2.h>
#include "Font.h"
#include "FontCache.h"
#include "FloatRect.h"
#include "FontDescription.h"
#include "PlatformString.h"
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <ApplicationServices/ApplicationServices.h>
#include <WebKitSystemInterface/WebKitSystemInterface.h>
#include <mlang.h>
#include <tchar.h>

namespace WebCore {

using std::max;

static inline float scaleEmToUnits(float x, unsigned unitsPerEm) { return unitsPerEm ? x / static_cast<float>(unitsPerEm) : x; }

void SimpleFontData::platformInit()
{
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;
    m_scriptCache = 0;
    m_scriptFontProperties = 0;
    m_isSystemFont = false;

    if (m_platformData.useGDI())
       return initGDIFont();

    CGFontRef font = m_platformData.cgFont();
    int iAscent = CGFontGetAscent(font);
    int iDescent = CGFontGetDescent(font);
    int iLineGap = CGFontGetLeading(font);
    m_unitsPerEm = CGFontGetUnitsPerEm(font);
    float pointSize = m_platformData.size();
    float fAscent = scaleEmToUnits(iAscent, m_unitsPerEm) * pointSize;
    float fDescent = -scaleEmToUnits(iDescent, m_unitsPerEm) * pointSize;
    float fLineGap = scaleEmToUnits(iLineGap, m_unitsPerEm) * pointSize;

    if (!isCustomFont()) {
        HDC dc = GetDC(0);
        HGDIOBJ oldFont = SelectObject(dc, m_platformData.hfont());
        int faceLength = GetTextFace(dc, 0, 0);
        Vector<TCHAR> faceName(faceLength);
        GetTextFace(dc, faceLength, faceName.data());
        m_isSystemFont = !_tcscmp(faceName.data(), _T("Lucida Grande"));
        SelectObject(dc, oldFont);
        ReleaseDC(0, dc);

        if (shouldApplyMacAscentHack()) {
            // This code comes from FontDataMac.mm. We only ever do this when running regression tests so that our metrics will match Mac.

            // We need to adjust Times, Helvetica, and Courier to closely match the
            // vertical metrics of their Microsoft counterparts that are the de facto
            // web standard. The AppKit adjustment of 20% is too big and is
            // incorrectly added to line spacing, so we use a 15% adjustment instead
            // and add it to the ascent.
            if (!_tcscmp(faceName.data(), _T("Times")) || !_tcscmp(faceName.data(), _T("Helvetica")) || !_tcscmp(faceName.data(), _T("Courier")))
                fAscent += floorf(((fAscent + fDescent) * 0.15f) + 0.5f);
        }
    }

    m_ascent = lroundf(fAscent);
    m_descent = lroundf(fDescent);
    m_lineGap = lroundf(fLineGap);
    m_lineSpacing = m_ascent + m_descent + m_lineGap;

    // Measure the actual character "x", because AppKit synthesizes X height rather than getting it from the font.
    // Unfortunately, NSFont will round this for us so we don't quite get the right value.
    GlyphPage* glyphPageZero = GlyphPageTreeNode::getRootChild(this, 0)->page();
    Glyph xGlyph = glyphPageZero ? glyphPageZero->glyphDataForCharacter('x').glyph : 0;
    if (xGlyph) {
        CGRect xBox;
        CGFontGetGlyphBBoxes(font, &xGlyph, 1, &xBox);
        // Use the maximum of either width or height because "x" is nearly square
        // and web pages that foolishly use this metric for width will be laid out
        // poorly if we return an accurate height. Classic case is Times 13 point,
        // which has an "x" that is 7x6 pixels.
        m_xHeight = scaleEmToUnits(max(CGRectGetMaxX(xBox), CGRectGetMaxY(xBox)), m_unitsPerEm) * pointSize;
    } else {
        int iXHeight = CGFontGetXHeight(font);
        m_xHeight = scaleEmToUnits(iXHeight, m_unitsPerEm) * pointSize;
    }
}

void SimpleFontData::platformCharWidthInit()
{
    // GDI Fonts init charwidths in initGDIFont.
    if (!m_platformData.useGDI()) {
        m_avgCharWidth = 0.f;
        m_maxCharWidth = 0.f;
        initCharWidths();
    }
}

GlyphMetrics SimpleFontData::platformMetricsForGlyph(Glyph glyph, GlyphMetricsMode metricsMode) const
{
    if (m_platformData.useGDI())
       return metricsForGDIGlyph(glyph);

    CGFontRef font = m_platformData.cgFont();
    float pointSize = m_platformData.size();
    CGSize advance;
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
 
    // FIXME: Need to add real support for printer fonts.
    bool isPrinterFont = false;
    wkGetGlyphAdvances(font, m, m_isSystemFont, isPrinterFont, glyph, advance);
    GlyphMetrics metrics;
    metrics.horizontalAdvance = advance.width + m_syntheticBoldOffset;
    
    if (metricsMode == GlyphBoundingBox) {
        CGRect boundingBox;
        CGFontGetGlyphBBoxes(font, &glyph, 1, &boundingBox);
        CGFloat scale = pointSize / unitsPerEm();
        metrics.boundingBox = CGRectApplyAffineTransform(boundingBox, CGAffineTransformMakeScale(scale, -scale));
        if (m_syntheticBoldOffset)
            metrics.boundingBox.setWidth(metrics.boundingBox.width() + m_syntheticBoldOffset);
    }
    return metrics;
}

}
