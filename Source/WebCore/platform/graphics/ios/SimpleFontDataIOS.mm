/*
 * Copyright (C) 2005, 2006, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "SimpleFontData.h"

#import "BlockExceptions.h"
#import "Font.h"
#import "FontCache.h"
#import "FontDescription.h"
#import "FontServicesIOS.h"
#import <CoreGraphics/CGFontInfo.h>
#import <CoreText/CoreText.h>
#import <float.h>
#import <unicode/uchar.h>
#import <wtf/Assertions.h>
#import <wtf/StdLibExtras.h>

namespace WebCore {

static bool fontFamilyShouldNotBeUsedForArabic(CFStringRef fontFamilyName)
{
    if (!fontFamilyName)
        return false;

    // Times New Roman contains Arabic glyphs, but Core Text doesn't know how to shape them. <rdar://problem/9823975>
    // FIXME <rdar://problem/12096835> remove this function once the above bug is fixed.
    // Arial and Tahoma are have performance issues so don't use them as well.
    return (CFStringCompare(CFSTR("Times New Roman"), fontFamilyName, 0) == kCFCompareEqualTo)
           || (CFStringCompare(CFSTR("Arial"), fontFamilyName, 0) == kCFCompareEqualTo)
           || (CFStringCompare(CFSTR("Tahoma"), fontFamilyName, 0) == kCFCompareEqualTo);
}

static bool fontHasVerticalGlyphs(CTFontRef ctFont)
{
    // The check doesn't look neat but this is what AppKit does for vertical writing...
    RetainPtr<CFArrayRef> tableTags = adoptCF(CTFontCopyAvailableTables(ctFont, kCTFontTableOptionNoOptions));
    CFIndex numTables = CFArrayGetCount(tableTags.get());
    for (CFIndex index = 0; index < numTables; ++index) {
        CTFontTableTag tag = (CTFontTableTag)(uintptr_t)CFArrayGetValueAtIndex(tableTags.get(), index);
        if (tag == kCTFontTableVhea || tag == kCTFontTableVORG)
            return true;
    }
    return false;
}

void SimpleFontData::platformInit()
{
    m_syntheticBoldOffset = m_platformData.m_syntheticBold ? ceilf(m_platformData.size()  / 24.0f) : 0.f;
    m_spaceGlyph = 0;
    m_spaceWidth = 0;
    unsigned unitsPerEm;
    float ascent;
    float descent;
    float lineGap;
    float lineSpacing;
    float xHeight;
    RetainPtr<CFStringRef> familyName;
    if (CTFontRef ctFont = m_platformData.font()) {
        FontServicesIOS fontService(ctFont);
        ascent = ceilf(fontService.ascent());
        descent = ceilf(fontService.descent());
        lineSpacing = fontService.lineSpacing();
        lineGap = fontService.lineGap();
        xHeight = fontService.xHeight();
        unitsPerEm = fontService.unitsPerEm();
        familyName = adoptCF(CTFontCopyFamilyName(ctFont));
    } else {
        CGFontRef cgFont = m_platformData.cgFont();

        unitsPerEm = CGFontGetUnitsPerEm(cgFont);

        float pointSize = m_platformData.size();
        ascent = lroundf(scaleEmToUnits(CGFontGetAscent(cgFont), unitsPerEm) * pointSize);
        descent = lroundf(-scaleEmToUnits(-abs(CGFontGetDescent(cgFont)), unitsPerEm) * pointSize);
        lineGap = lroundf(scaleEmToUnits(CGFontGetLeading(cgFont), unitsPerEm) * pointSize);
        xHeight = scaleEmToUnits(CGFontGetXHeight(cgFont), unitsPerEm) * pointSize;

        lineSpacing = ascent + descent + lineGap;
        familyName = adoptCF(CGFontCopyFamilyName(cgFont));
    }

    m_fontMetrics.setUnitsPerEm(unitsPerEm);
    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);
    m_fontMetrics.setLineGap(lineGap);
    m_fontMetrics.setLineSpacing(lineSpacing);
    m_fontMetrics.setXHeight(xHeight);
    m_shouldNotBeUsedForArabic = fontFamilyShouldNotBeUsedForArabic(familyName.get());

    if (platformData().orientation() == Vertical && !isTextOrientationFallback())
        m_hasVerticalGlyphs = fontHasVerticalGlyphs(m_platformData.ctFont());

    if (!m_platformData.m_isEmoji)
        return;

    int thirdOfSize = m_platformData.size() / 3;
    m_fontMetrics.setAscent(thirdOfSize);
    m_fontMetrics.setDescent(thirdOfSize);
    m_fontMetrics.setLineGap(thirdOfSize);
    m_fontMetrics.setLineSpacing(0);
}

void SimpleFontData::platformCharWidthInit()
{
    m_avgCharWidth = 0;
    m_maxCharWidth = 0;

    // Fallback to a cross-platform estimate, which will populate these values if they are non-positive.
    initCharWidths();
}

PassRefPtr<SimpleFontData> SimpleFontData::platformCreateScaledFontData(const FontDescription&, float scaleFactor) const
{
    if (isCustomFont()) {
        FontPlatformData scaledFontData(m_platformData);
        scaledFontData.m_size = scaledFontData.m_size * scaleFactor;
        return SimpleFontData::create(scaledFontData, true, false);
    }

    float size = m_platformData.size() * scaleFactor;
    CTFontSymbolicTraits fontTraits = CTFontGetSymbolicTraits(m_platformData.font());
    RetainPtr<CTFontDescriptorRef> fontDescriptor = adoptCF(CTFontCopyFontDescriptor(m_platformData.font()));
    RetainPtr<CTFontRef> scaledFont = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), size, nullptr));
    FontPlatformData scaledFontData(scaledFont.get(), size, m_platformData.isPrinterFont(), false, false, m_platformData.orientation());

    if (scaledFontData.font()) {
        if (m_platformData.m_syntheticBold)
            fontTraits |= kCTFontBoldTrait;
        if (m_platformData.m_syntheticOblique)
            fontTraits |= kCTFontItalicTrait;

        CTFontSymbolicTraits scaledFontTraits = CTFontGetSymbolicTraits(scaledFontData.font());
        scaledFontData.m_syntheticBold = (fontTraits & kCTFontBoldTrait) && !(scaledFontTraits & kCTFontTraitBold);
        scaledFontData.m_syntheticOblique = (fontTraits & kCTFontItalicTrait) && !(scaledFontTraits & kCTFontTraitItalic);

        return fontCache().getCachedFontData(&scaledFontData);
    }

    return nullptr;
}

bool SimpleFontData::containsCharacters(const UChar*, int) const
{
    return false;
}

void SimpleFontData::determinePitch()
{
    CTFontRef ctFont = m_platformData.font();
    m_treatAsFixedPitch = false;
    if (!ctFont)
        return; // CTFont is null in the case of SVG fonts for example.

    RetainPtr<CFStringRef> fullName = adoptCF(CTFontCopyFullName(ctFont));
    RetainPtr<CFStringRef> familyName = adoptCF(CTFontCopyFamilyName(ctFont));

    m_treatAsFixedPitch = CGFontIsFixedPitch(m_platformData.cgFont()) || (fullName && (CFStringCompare(fullName.get(), CFSTR("Osaka-Mono"), kCFCompareCaseInsensitive) == kCFCompareEqualTo || CFStringCompare(fullName.get(), CFSTR("MS-PGothic"), kCFCompareCaseInsensitive) == kCFCompareEqualTo));
    if (familyName && CFStringCompare(familyName.get(), CFSTR("Courier New"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
        // Special case Courier New to not be treated as fixed pitch, as this will make use of a hacked space width which is undesireable for iPhone (see rdar://6269783).
        m_treatAsFixedPitch = false;
    }
}

CGFontRenderingStyle SimpleFontData::renderingStyle() const
{
    return kCGFontRenderingStyleAntialiasing | kCGFontRenderingStyleSubpixelPositioning | kCGFontRenderingStyleSubpixelQuantization | kCGFontAntialiasingStyleUnfiltered;
}

bool SimpleFontData::advanceForColorBitmapFont(Glyph, CGSize&) const
{
    return false;
}

} // namespace WebCore
