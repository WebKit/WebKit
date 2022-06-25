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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "Font.h"

#if USE(CG)

#include "FloatRect.h"
#include "FontCache.h"
#include "FontCascade.h"
#include "FontDescription.h"
#include "GlyphPage.h"
#include "HWndDC.h"
#include "OpenTypeCG.h"
#include <mlang.h>
#include <pal/spi/win/CoreTextSPIWin.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <winsock2.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

void Font::platformInit()
{
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;
    m_scriptCache = 0;
    m_scriptFontProperties = 0;

    if (m_platformData.useGDI())
        return initGDIFont();

    CGFontRef font = m_platformData.cgFont();
    int iAscent = CGFontGetAscent(font);
    int iDescent = CGFontGetDescent(font);
    int iLineGap = CGFontGetLeading(font);
    int iCapHeight = CGFontGetCapHeight(font);

    // The Open Font Format describes the OS/2 USE_TYPO_METRICS flag as follows:
    // "If set, it is strongly recommended to use OS/2.sTypoAscender - OS/2.sTypoDescender+ OS/2.sTypoLineGap as a value for default line spacing for this font."
    short typoAscent, typoDescent, typoLineGap;
    if (OpenType::tryGetTypoMetrics(adoptCF(CTFontCreateWithGraphicsFont(m_platformData.cgFont(), m_platformData.size(), nullptr, nullptr)).get(), typoAscent, typoDescent, typoLineGap)) {
        iAscent = typoAscent;
        iDescent = typoDescent;
        iLineGap = typoLineGap;
    }

    unsigned unitsPerEm = CGFontGetUnitsPerEm(font);
    float pointSize = m_platformData.size();
    float fAscent = scaleEmToUnits(iAscent, unitsPerEm) * pointSize;
    float fDescent = -scaleEmToUnits(iDescent, unitsPerEm) * pointSize;
    float fCapHeight = scaleEmToUnits(iCapHeight, unitsPerEm) * pointSize;
    float fLineGap = scaleEmToUnits(iLineGap, unitsPerEm) * pointSize;

    if (origin() == Origin::Local) {
        HWndDC dc(0);
        HGDIOBJ oldFont = SelectObject(dc, m_platformData.hfont());
        int faceLength = GetTextFace(dc, 0, 0);
        Vector<WCHAR> faceName(faceLength);
        GetTextFace(dc, faceLength, faceName.data());
        SelectObject(dc, oldFont);

        fAscent = ascentConsideringMacAscentHack(faceName.data(), fAscent, fDescent);
    }

    m_fontMetrics.setAscent(fAscent);
    m_fontMetrics.setDescent(fDescent);
    m_fontMetrics.setCapHeight(fCapHeight);
    m_fontMetrics.setLineGap(fLineGap);
    m_fontMetrics.setLineSpacing(lroundf(fAscent) + lroundf(fDescent) + lroundf(fLineGap));

    Glyph xGlyph = glyphDataForCharacter('x').glyph;
    if (xGlyph) {
        // Measure the actual character "x", since it's possible for it to extend below the baseline, and we need the
        // reported x-height to only include the portion of the glyph that is above the baseline.
        CGRect xBox;
        CGFontGetGlyphBBoxes(font, &xGlyph, 1, &xBox);
        m_fontMetrics.setXHeight(scaleEmToUnits(CGRectGetMaxY(xBox), unitsPerEm) * pointSize);
    } else {
        int iXHeight = CGFontGetXHeight(font);
        m_fontMetrics.setXHeight(scaleEmToUnits(iXHeight, unitsPerEm) * pointSize);
    }

    m_fontMetrics.setUnitsPerEm(unitsPerEm);
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    if (!platformData().size())
        return 0;

    if (m_platformData.useGDI())
        return widthForGDIGlyph(glyph);

    CGFontRef font = m_platformData.cgFont();
    float pointSize = m_platformData.size();
    CGSize advance;
    CGAffineTransform m = CGAffineTransformMakeScale(pointSize, pointSize);
 
    bool isPrinterFont = false;
    FontCascade::getPlatformGlyphAdvances(font, m, m_platformData.isSystemFont(), isPrinterFont, glyph, advance);

    return advance.width + m_syntheticBoldOffset;
}

}

#endif
