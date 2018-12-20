/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "DashArray.h"
#include "Font.h"
#include "FontCascadeDescription.h"
#include "FontCascadeFonts.h"
#include "Path.h"
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/WeakPtr.h>
#include <wtf/unicode/CharacterNames.h>

// "X11/X.h" defines Complex to 0 and conflicts
// with Complex value in CodePath enum.
#ifdef Complex
#undef Complex
#endif

namespace WebCore {

class GraphicsContext;
class LayoutRect;
class RenderText;
class TextLayout;
class TextRun;

namespace DisplayList {
class DisplayList;
}
    
struct GlyphData;

struct GlyphOverflow {
    bool isEmpty() const
    {
        return !left && !right && !top && !bottom;
    }

    void extendTo(const GlyphOverflow& other)
    {
        left = std::max(left, other.left);
        right = std::max(right, other.right);
        top = std::max(top, other.top);
        bottom = std::max(bottom, other.bottom);
    }

    bool operator!=(const GlyphOverflow& other)
    {
        // FIXME: Probably should name this rather than making it the != operator since it ignores the value of computeBounds.
        return left != other.left || right != other.right || top != other.top || bottom != other.bottom;
    }

    int left { 0 };
    int right { 0 };
    int top { 0 };
    int bottom { 0 };
    bool computeBounds { false };
};

class TextLayoutDeleter {
public:
    void operator()(TextLayout*) const;
};

class FontCascade : public CanMakeWeakPtr<FontCascade> {
public:
    WEBCORE_EXPORT FontCascade();
    WEBCORE_EXPORT FontCascade(FontCascadeDescription&&, float letterSpacing = 0, float wordSpacing = 0);
    // This constructor is only used if the platform wants to start with a native font.
    WEBCORE_EXPORT FontCascade(const FontPlatformData&, FontSmoothingMode = FontSmoothingMode::AutoSmoothing);

    FontCascade(const FontCascade&);
    WEBCORE_EXPORT FontCascade& operator=(const FontCascade&);

    WEBCORE_EXPORT bool operator==(const FontCascade& other) const;
    bool operator!=(const FontCascade& other) const { return !(*this == other); }

    const FontCascadeDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }

    WEBCORE_EXPORT void update(RefPtr<FontSelector>&& = nullptr) const;

    enum CustomFontNotReadyAction { DoNotPaintIfFontNotReady, UseFallbackIfFontNotReady };
    WEBCORE_EXPORT float drawText(GraphicsContext&, const TextRun&, const FloatPoint&, unsigned from = 0, Optional<unsigned> to = WTF::nullopt, CustomFontNotReadyAction = DoNotPaintIfFontNotReady) const;
    static void drawGlyphs(GraphicsContext&, const Font&, const GlyphBuffer&, unsigned from, unsigned numGlyphs, const FloatPoint&, FontSmoothingMode);
    void drawEmphasisMarks(GraphicsContext&, const TextRun&, const AtomicString& mark, const FloatPoint&, unsigned from = 0, Optional<unsigned> to = WTF::nullopt) const;

    DashArray dashesForIntersectionsWithRect(const TextRun&, const FloatPoint& textOrigin, const FloatRect& lineExtents) const;

    float widthOfTextRange(const TextRun&, unsigned from, unsigned to, HashSet<const Font*>* fallbackFonts = 0, float* outWidthBeforeRange = nullptr, float* outWidthAfterRange = nullptr) const;
    WEBCORE_EXPORT float width(const TextRun&, HashSet<const Font*>* fallbackFonts = 0, GlyphOverflow* = 0) const;
    float widthForSimpleText(StringView text) const;

    std::unique_ptr<TextLayout, TextLayoutDeleter> createLayout(RenderText&, float xPos, bool collapseWhiteSpace) const;
    static float width(TextLayout&, unsigned from, unsigned len, HashSet<const Font*>* fallbackFonts = 0);

    int offsetForPosition(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForText(const TextRun&, LayoutRect& selectionRect, unsigned from = 0, Optional<unsigned> to = WTF::nullopt) const;

    bool isSmallCaps() const { return m_fontDescription.variantCaps() == FontVariantCaps::Small; }

    float wordSpacing() const { return m_wordSpacing; }
    float letterSpacing() const { return m_letterSpacing; }
    void setWordSpacing(float s) { m_wordSpacing = s; }
    void setLetterSpacing(float s) { m_letterSpacing = s; }
    bool isFixedPitch() const;
    
    FontRenderingMode renderingMode() const { return m_fontDescription.renderingMode(); }

    bool enableKerning() const { return m_enableKerning; }
    bool requiresShaping() const { return m_requiresShaping; }

    const AtomicString& firstFamily() const { return m_fontDescription.firstFamily(); }
    unsigned familyCount() const { return m_fontDescription.familyCount(); }
    const AtomicString& familyAt(unsigned i) const { return m_fontDescription.familyAt(i); }

    // A WTF::nullopt return value indicates "font-style: normal".
    Optional<FontSelectionValue> italic() const { return m_fontDescription.italic(); }
    FontSelectionValue weight() const { return m_fontDescription.weight(); }
    FontWidthVariant widthVariant() const { return m_fontDescription.widthVariant(); }

    bool isPlatformFont() const { return m_fonts->isForPlatformFont(); }

    const FontMetrics& fontMetrics() const { return primaryFont().fontMetrics(); }
    float spaceWidth() const { return primaryFont().spaceWidth() + m_letterSpacing; }
    float tabWidth(const Font&, unsigned tabSize, float position) const;
    float tabWidth(unsigned tabSize, float position) const { return tabWidth(primaryFont(), tabSize, position); }
    bool hasValidAverageCharWidth() const;
    bool fastAverageCharWidthIfAvailable(float &width) const; // returns true on success

    int emphasisMarkAscent(const AtomicString&) const;
    int emphasisMarkDescent(const AtomicString&) const;
    int emphasisMarkHeight(const AtomicString&) const;

    const Font& primaryFont() const;
    const FontRanges& fallbackRangesAt(unsigned) const;
    GlyphData glyphDataForCharacter(UChar32, bool mirror, FontVariant = AutoVariant) const;

    const Font* fontForCombiningCharacterSequence(const UChar*, size_t length) const;

    static bool isCJKIdeograph(UChar32);
    static bool isCJKIdeographOrSymbol(UChar32);

    // Returns (the number of opportunities, whether the last expansion is a trailing expansion)
    // If there are no opportunities, the bool will be true iff we are forbidding leading expansions.
    static std::pair<unsigned, bool> expansionOpportunityCount(const StringView&, TextDirection, ExpansionBehavior);

    // Whether or not there is an expansion opportunity just before the first character
    // Note that this does not take a isAfterExpansion flag; this assumes that isAfterExpansion is false
    // Here, "Leading" and "Trailing" are relevant after the line has been rearranged for bidi.
    // ("Leading" means "left" and "Trailing" means "right.")
    static bool leadingExpansionOpportunity(const StringView&, TextDirection);
    static bool trailingExpansionOpportunity(const StringView&, TextDirection);

    WEBCORE_EXPORT static void setShouldUseSmoothing(bool);
    WEBCORE_EXPORT static bool shouldUseSmoothing();

    static bool isSubpixelAntialiasingAvailable();

    enum CodePath { Auto, Simple, Complex, SimpleWithGlyphOverflow };
    CodePath codePath(const TextRun&, Optional<unsigned> from = WTF::nullopt, Optional<unsigned> to = WTF::nullopt) const;
    static CodePath characterRangeCodePath(const LChar*, unsigned) { return Simple; }
    static CodePath characterRangeCodePath(const UChar*, unsigned len);

    bool primaryFontIsSystemFont() const;

    std::unique_ptr<DisplayList::DisplayList> displayListForTextRun(GraphicsContext&, const TextRun&, unsigned from = 0, Optional<unsigned> to = { }, CustomFontNotReadyAction = CustomFontNotReadyAction::DoNotPaintIfFontNotReady) const;

#if PLATFORM(WIN) && USE(CG)
    static void setFontSmoothingLevel(int);
    static uint32_t setFontSmoothingStyle(CGContextRef, bool fontAllowsSmoothing);
    static void setFontSmoothingContrast(CGFloat);
    static void systemFontSmoothingChanged();
    static void setCGContextFontRenderingStyle(CGContextRef, bool isSystemFont, bool isPrinterFont, bool usePlatformNativeGlyphs);
    static void getPlatformGlyphAdvances(CGFontRef, const CGAffineTransform&, bool isSystemFont, bool isPrinterFont, CGGlyph, CGSize& advance);
#endif
private:
    enum ForTextEmphasisOrNot { NotForTextEmphasis, ForTextEmphasis };

    float glyphBufferForTextRun(CodePath, const TextRun&, unsigned from, unsigned to, GlyphBuffer&) const;
    // Returns the initial in-stream advance.
    float getGlyphsAndAdvancesForSimpleText(const TextRun&, unsigned from, unsigned to, GlyphBuffer&, ForTextEmphasisOrNot = NotForTextEmphasis) const;
    void drawEmphasisMarksForSimpleText(GraphicsContext&, const TextRun&, const AtomicString& mark, const FloatPoint&, unsigned from, unsigned to) const;
    void drawGlyphBuffer(GraphicsContext&, const GlyphBuffer&, FloatPoint&, CustomFontNotReadyAction) const;
    void drawEmphasisMarks(GraphicsContext&, const GlyphBuffer&, const AtomicString&, const FloatPoint&) const;
    float floatWidthForSimpleText(const TextRun&, HashSet<const Font*>* fallbackFonts = 0, GlyphOverflow* = 0) const;
    int offsetForPositionForSimpleText(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForSimpleText(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;

    Optional<GlyphData> getEmphasisMarkGlyphData(const AtomicString&) const;

    static bool canReturnFallbackFontsForComplexText();
    static bool canExpandAroundIdeographsInComplexText();

    // Returns the initial in-stream advance.
    float getGlyphsAndAdvancesForComplexText(const TextRun&, unsigned from, unsigned to, GlyphBuffer&, ForTextEmphasisOrNot = NotForTextEmphasis) const;
    void drawEmphasisMarksForComplexText(GraphicsContext&, const TextRun&, const AtomicString& mark, const FloatPoint&, unsigned from, unsigned to) const;
    float floatWidthForComplexText(const TextRun&, HashSet<const Font*>* fallbackFonts = 0, GlyphOverflow* = 0) const;
    int offsetForPositionForComplexText(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForComplexText(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;

    static std::pair<unsigned, bool> expansionOpportunityCountInternal(const LChar*, unsigned length, TextDirection, ExpansionBehavior);
    static std::pair<unsigned, bool> expansionOpportunityCountInternal(const UChar*, unsigned length, TextDirection, ExpansionBehavior);

    friend struct WidthIterator;

public:
#if ENABLE(TEXT_AUTOSIZING)
    bool equalForTextAutoSizing(const FontCascade& other) const
    {
        return m_fontDescription.equalForTextAutoSizing(other.m_fontDescription)
            && m_letterSpacing == other.m_letterSpacing
            && m_wordSpacing == other.m_wordSpacing;
    }
#endif

    // Useful for debugging the different font rendering code paths.
    WEBCORE_EXPORT static void setCodePath(CodePath);
    static CodePath codePath();
    static CodePath s_codePath;

    FontSelector* fontSelector() const;
    static bool treatAsSpace(UChar c) { return c == ' ' || c == '\t' || c == '\n' || c == noBreakSpace; }
    static bool treatAsZeroWidthSpace(UChar c) { return treatAsZeroWidthSpaceInComplexScript(c) || c == 0x200c || c == 0x200d; }
    static bool treatAsZeroWidthSpaceInComplexScript(UChar c) { return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == softHyphen || c == zeroWidthSpace || (c >= 0x200e && c <= 0x200f) || (c >= 0x202a && c <= 0x202e) || c == zeroWidthNoBreakSpace || c == objectReplacementCharacter; }
    static bool canReceiveTextEmphasis(UChar32);

    static inline UChar normalizeSpaces(UChar character)
    {
        if (treatAsSpace(character))
            return space;

        if (treatAsZeroWidthSpace(character))
            return zeroWidthSpace;

        return character;
    }

    static String normalizeSpaces(const LChar*, unsigned length);
    static String normalizeSpaces(const UChar*, unsigned length);

    bool useBackslashAsYenSymbol() const { return m_useBackslashAsYenSymbol; }
    FontCascadeFonts* fonts() const { return m_fonts.get(); }
    bool isLoadingCustomFonts() const;

private:

    bool advancedTextRenderingMode() const
    {
        auto textRenderingMode = m_fontDescription.textRenderingMode();
        if (textRenderingMode == TextRenderingMode::GeometricPrecision || textRenderingMode == TextRenderingMode::OptimizeLegibility)
            return true;
        if (textRenderingMode == TextRenderingMode::OptimizeSpeed)
            return false;
#if PLATFORM(COCOA) || USE(FREETYPE)
        return true;
#else
        return false;
#endif
    }

    bool computeEnableKerning() const
    {
        auto kerning = m_fontDescription.kerning();
        if (kerning == Kerning::Normal)
            return true;
        if (kerning == Kerning::NoShift)
            return false;
        return advancedTextRenderingMode();
    }

    bool computeRequiresShaping() const
    {
#if PLATFORM(COCOA) || USE(FREETYPE)
        if (!m_fontDescription.variantSettings().isAllNormal())
            return true;
        if (m_fontDescription.featureSettings().size())
            return true;
#endif
        return advancedTextRenderingMode();
    }

    static int syntheticObliqueAngle() { return 14; }

#if PLATFORM(WIN) && USE(CG)
    static double s_fontSmoothingContrast;
    static uint32_t s_fontSmoothingType;
    static int s_fontSmoothingLevel;
    static uint32_t s_systemFontSmoothingType;
    static bool s_systemFontSmoothingSet;
    static bool s_systemFontSmoothingEnabled;
#endif

    FontCascadeDescription m_fontDescription;
    mutable RefPtr<FontCascadeFonts> m_fonts;
    float m_letterSpacing { 0 };
    float m_wordSpacing { 0 };
    mutable bool m_useBackslashAsYenSymbol { false };
    mutable bool m_enableKerning { false }; // Computed from m_fontDescription.
    mutable bool m_requiresShaping { false }; // Computed from m_fontDescription.
};

void invalidateFontCascadeCache();
void pruneUnreferencedEntriesFromFontCascadeCache();
void pruneSystemFallbackFonts();
void clearWidthCaches();

inline const Font& FontCascade::primaryFont() const
{
    ASSERT(m_fonts);
    return m_fonts->primaryFont(m_fontDescription);
}

inline const FontRanges& FontCascade::fallbackRangesAt(unsigned index) const
{
    ASSERT(m_fonts);
    return m_fonts->realizeFallbackRangesAt(m_fontDescription, index);
}

inline bool FontCascade::isFixedPitch() const
{
    ASSERT(m_fonts);
    return m_fonts->isFixedPitch(m_fontDescription);
}

inline FontSelector* FontCascade::fontSelector() const
{
    return m_fonts ? m_fonts->fontSelector() : nullptr;
}

inline float FontCascade::tabWidth(const Font& font, unsigned tabSize, float position) const
{
    if (!tabSize)
        return letterSpacing();
    float tabWidth = tabSize * font.spaceWidth() + letterSpacing();
    float tabDeltaWidth = tabWidth - fmodf(position, tabWidth);
    return (tabDeltaWidth < font.spaceWidth() / 2) ? tabWidth : tabDeltaWidth;
}

}
