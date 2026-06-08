/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include <WebCore/Font.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/FontCascadeEnums.h>
#include <WebCore/GlyphBuffer.h>
#include <optional>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

// "X11/X.h" defines Complex to 0 and conflicts
// with Complex value in CodePath enum.
#ifdef Complex
#undef Complex
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class GraphicsContext;
class FontCascadeFonts;
class FontSelector;
class LayoutRect;
class RenderStyle;
class RenderText;
class TextLayout;
class TextRun;

namespace DisplayList {
class DisplayList;
}

struct GlyphData;
struct GlyphGeometryCacheEntry;
struct GlyphOverflow;
struct FloatSegment;
struct TabSize;

#if USE(CORE_TEXT)
AffineTransform computeBaseOverallTextMatrix(const std::optional<AffineTransform>& syntheticOblique);
AffineTransform computeOverallTextMatrix(const Font&);
AffineTransform computeBaseVerticalTextMatrix(const AffineTransform& previousTextMatrix);
AffineTransform computeVerticalTextMatrix(const Font&, const AffineTransform& previousTextMatrix);
#endif

class TextLayoutDeleter {
public:
    void operator()(TextLayout*) const;
};

struct TextShapingResult {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(TextShapingResult);
public:
    float width { 0.f };
    GlyphBuffer glyphBuffer;
};

enum class ForTextEmphasis : bool { No, Yes };

class FontCascade final : public CanMakeWeakPtr<FontCascade>, public CanMakeCheckedPtr<FontCascade, WTF::DefaultedOperatorEqual::No, WTF::CheckedPtrDeleteCheckException::Yes> {
    WTF_MAKE_TZONE_ALLOCATED(FontCascade);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FontCascade);
public:
    WEBCORE_EXPORT FontCascade();
    WEBCORE_EXPORT FontCascade(FontCascadeDescription&&);
    WEBCORE_EXPORT FontCascade(FontCascadeDescription&&, const FontCascade&);
    // This constructor is only used if the platform wants to start with a native font.
    WEBCORE_EXPORT FontCascade(const FontPlatformData&, FontSmoothingMode = FontSmoothingMode::Auto);

    WEBCORE_EXPORT FontCascade(const FontCascade&);
    WEBCORE_EXPORT FontCascade& operator=(const FontCascade&);

    WEBCORE_EXPORT ~FontCascade();

    WEBCORE_EXPORT bool operator==(const FontCascade& other) const;

    const FontCascadeDescription& fontDescription() const LIFETIME_BOUND { return m_fontDescription; }
    FontCascadeDescription& mutableFontDescription() const LIFETIME_BOUND { return m_fontDescription; }

    float size() const { return fontDescription().computedSize(); }

    bool isCurrent(const FontSelector&) const;
    void updateFonts(Ref<FontCascadeFonts>&&) const;
    WEBCORE_EXPORT void update(RefPtr<FontSelector>&& = nullptr) const;
    unsigned fontSelectorVersion() const;

    using CustomFontNotReadyAction = FontCascadeCustomFontNotReadyAction;
    WEBCORE_EXPORT FloatSize drawText(GraphicsContext&, const TextRun&, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt, CustomFontNotReadyAction = CustomFontNotReadyAction::DoNotPaintIfFontNotReady) const;
    static void drawGlyphs(GraphicsContext&, const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint&, FontSmoothingMode);
    void drawEmphasisMarks(GraphicsContext&, const TextRun&, const AtomString& mark, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt) const;

    Vector<FloatSegment> lineSegmentsForIntersectionsWithRect(const TextRun&, const FloatPoint& textOrigin, const FloatRect& lineExtents) const;

    float widthOfTextRange(const TextRun&, unsigned from, unsigned to, float& outWidthBeforeRange, float& outWidthAfterRange) const;
    WEBCORE_EXPORT float width(const TextRun&, SingleThreadWeakHashSet<const Font>* fallbackFonts = nullptr, GlyphOverflow* = nullptr) const;
    WEBCORE_EXPORT float width(StringView) const;
    float widthForTextUsingSimplifiedMeasuring(StringView text, TextDirection = TextDirection::LTR) const;
    WEBCORE_EXPORT float widthForSimpleTextWithFixedPitch(StringView text, bool whitespaceIsCollapsed) const;

    std::unique_ptr<TextLayout, TextLayoutDeleter> createLayout(RenderText&, float xPos, bool collapseWhiteSpace) const;
    inline float widthOfSpaceString() const; // Defined in FontCascadeInlines.h

    int offsetForPosition(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForText(bool canUseSimplifiedTextMeasuring, const TextRun&, LayoutRect& selectionRect, unsigned from = 0, std::optional<unsigned> to = std::nullopt) const;

    Vector<LayoutRect> characterSelectionRectsForText(const TextRun&, const LayoutRect& selectionRect, unsigned from, std::optional<unsigned> to) const;

    bool isSmallCaps() const { return m_fontDescription.variantCaps() == FontVariantCaps::Small; }

    float letterSpacing() const { return m_spacing.letter; }
    float wordSpacing() const { return m_spacing.word; }
    void setLetterSpacing(float spacing) { m_spacing.letter = spacing; }
    void setWordSpacing(float spacing) { m_spacing.word = spacing; }
    TextSpacingTrim textSpacingTrim() const { return m_fontDescription.textSpacingTrim(); }
    TextAutospace textAutospace() const { return m_fontDescription.textAutospace(); }
    inline bool isFixedPitch() const; // Defined in FontCascadeInlines.h
    inline bool canTakeFixedPitchFastContentMeasuring() const; // Defined in FontCascadeInlines.h

    bool enableKerning() const { return m_enableKerning; }
    bool requiresShaping() const { return m_requiresShaping; }

    const FontFamily& firstFamily() const LIFETIME_BOUND { return m_fontDescription.firstFamily(); }
    unsigned familyCount() const { return m_fontDescription.familyCount(); }
    const FontFamily& familyAt(unsigned i) const LIFETIME_BOUND { return m_fontDescription.familyAt(i); }

    // A std::nullopt return value indicates "font-style: normal".
    std::optional<FontSelectionValue> fontStyleSlope() const { return m_fontDescription.fontStyleSlope(); }
    FontSelectionValue weight() const { return m_fontDescription.weight(); }
    FontWidthVariant widthVariant() const { return m_fontDescription.widthVariant(); }

    inline bool isPlatformFont() const; // Defined in FontCascadeInlines.h

    inline const FontMetrics& metricsOfPrimaryFont() const; // Defined in FontCascadeInlines.h
    float zeroWidth() const;
    float tabWidth(const Font&, const TabSize&, float, Font::SyntheticBoldInclusion) const;
    bool hasValidAverageCharWidth() const;
    bool fastAverageCharWidthIfAvailable(float &width) const; // returns true on success

    int emphasisMarkAscent(const AtomString&) const;
    int emphasisMarkDescent(const AtomString&) const;
    float floatEmphasisMarkHeight(const AtomString&) const;

    inline const Font& primaryFont() const; // Defined in FontCascadeInlines.h
    inline const FontRanges& fallbackRangesAt(unsigned) const; // Defined in FontCascadeInlines.h
    WEBCORE_EXPORT GlyphData glyphDataForCharacter(char32_t, bool mirror, FontVariant = FontVariant::Auto, std::optional<ResolvedEmojiPolicy> = std::nullopt) const;
    bool canUseSimplifiedTextMeasuring(char32_t, FontVariant, bool whitespaceIsCollapsed, const Font&) const;

    RefPtr<const Font> fontForCombiningCharacterSequence(StringView) const;

    static bool NODELETE isCJKIdeograph(char32_t);
    static bool NODELETE isCJKIdeographOrSymbol(char32_t);

    static bool canUseGlyphDisplayList(const RenderStyle&);

    // Returns (the number of opportunities, whether the last expansion is a trailing expansion)
    // If there are no opportunities, the bool will be true iff we are forbidding leading expansions.
    static std::pair<unsigned, bool> expansionOpportunityCount(StringView, TextDirection, ExpansionBehavior);

    WEBCORE_EXPORT static void NODELETE setDisableFontSubpixelAntialiasingForTesting(bool);
    WEBCORE_EXPORT static bool NODELETE shouldDisableFontSubpixelAntialiasingForTesting();

    using CodePath = FontCascadeCodePath;
    WEBCORE_EXPORT CodePath codePath(const TextRun&, std::optional<unsigned> from = std::nullopt, std::optional<unsigned> to = std::nullopt) const;

    static CodePath characterRangeCodePath(std::span<const Latin1Character>) { return CodePath::Simple; }
    WEBCORE_EXPORT static CodePath characterRangeCodePath(std::span<const char16_t>);

    bool primaryFontIsSystemFont() const;

    static constexpr float syntheticObliqueAngle() { return 14; }

    RefPtr<const DisplayList::DisplayList> displayListForTextRun(GraphicsContext&, const TextRun&, unsigned from = 0, std::optional<unsigned> to = { }, CustomFontNotReadyAction = CustomFontNotReadyAction::DoNotPaintIfFontNotReady) const;
    RefPtr<const DisplayList::DisplayList> displayListForGlyphBuffer(GraphicsContext&, const GlyphBuffer&, CustomFontNotReadyAction) const;

    unsigned generation() const { return m_generation; }

    TextShapingResult layoutText(CodePath, const TextRun&, unsigned from, unsigned to, ForTextEmphasis = ForTextEmphasis::No) const;
    void drawGlyphBuffer(GraphicsContext&, const GlyphBuffer&, FloatPoint&, CustomFontNotReadyAction) const;

private:

    TextShapingResult layoutSimpleText(const TextRun&, unsigned from, unsigned to, ForTextEmphasis = ForTextEmphasis::No) const;
    void drawEmphasisMarks(GraphicsContext&, const GlyphBuffer&, const AtomString&, const FloatPoint&) const;
    int offsetForPositionForSimpleText(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForSimpleText(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;
    void adjustSelectionRectForSimpleTextWithFixedPitch(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;
    float width(CodePath, const TextRun&, SingleThreadWeakHashSet<const Font>* fallbackFonts = nullptr, GlyphOverflow* = nullptr) const;
    WEBCORE_EXPORT float widthForSimpleTextSlow(StringView text, TextDirection, GlyphGeometryCacheEntry*) const;
    ALWAYS_INLINE bool NODELETE canHandleRunAsSimpleText(const TextRun&, unsigned from, unsigned to) const;

    std::optional<GlyphData> getEmphasisMarkGlyphData(const AtomString&) const;
    const Font* fontForEmphasisMark(const AtomString&) const;

    static constexpr bool canReturnFallbackFontsForComplexText();
    static constexpr bool canExpandAroundIdeographsInComplexText();

    TextShapingResult layoutComplexText(const TextRun&, unsigned from, unsigned to, ForTextEmphasis = ForTextEmphasis::No) const;
    int offsetForPositionForComplexText(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForComplexText(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;

    static std::pair<unsigned, bool> expansionOpportunityCountInternal(std::span<const Latin1Character>, TextDirection, ExpansionBehavior);
    static std::pair<unsigned, bool> NODELETE expansionOpportunityCountInternal(std::span<const char16_t>, TextDirection, ExpansionBehavior);

    friend struct WidthIterator;
    friend class ComplexTextController;
    friend class FontCascadeFonts;

public:
#if ENABLE(TEXT_AUTOSIZING)
    bool equalForTextAutoSizing(const FontCascade& other) const
    {
        return m_fontDescription.equalForTextAutoSizing(other.m_fontDescription)
            && m_spacing == other.m_spacing;
    }
#endif

    // Useful for debugging the different font rendering code paths.
    WEBCORE_EXPORT static void NODELETE setForcedCodePath(Markable<CodePath>);
    static Markable<CodePath> NODELETE forcedCodePath();
    static Markable<CodePath> s_forcedCodePath;

    bool hasFontSelector() const { return !!m_fontSelector; }
    inline FontSelector* fontSelector() const; // Defined in FontCascadeInlines.h

    inline static bool isInvisibleReplacementObjectCharacter(char32_t); // Defined in FontCascadeInlines.h
    inline static bool treatAsSpace(char32_t); // Defined in FontCascadeInlines.h
    inline static bool isCharacterWhoseGlyphsShouldBeDeletedForTextRendering(char32_t); // Defined in FontCascadeInlines.h
    // FIXME: Callers of treatAsZeroWidthSpace() and treatAsZeroWidthSpaceInComplexScript() should probably be calling isCharacterWhoseGlyphsShouldBeDeletedForTextRendering() instead.
    inline static bool treatAsZeroWidthSpace(char32_t); // Defined in FontCascadeInlines.h
    inline static bool treatAsZeroWidthSpaceInComplexScript(char32_t); // Defined in FontCascadeInlines.h
    static bool canReceiveTextEmphasis(char32_t);

    inline static char16_t normalizeSpaces(char16_t); // Defined in FontCascadeInlines.h

    static String normalizeSpaces(std::span<const Latin1Character>);
    static String normalizeSpaces(std::span<const char16_t>);
    static String normalizeSpaces(StringView);

    bool useBackslashAsYenSymbol() const { return m_useBackslashAsYenSymbol; }
    FontCascadeFonts* fonts() const { return m_fonts.get(); }
    bool isLoadingCustomFonts() const;

    static ResolvedEmojiPolicy resolveEmojiPolicy(FontVariantEmoji, char32_t);

    void updateUseBackslashAsYenSymbol() { m_useBackslashAsYenSymbol = computeUseBackslashAsYenSymbol(); }
    void updateEnableKerning() { m_enableKerning = computeEnableKerning(); }
    void updateRequiresShaping() { m_requiresShaping = computeRequiresShaping(); }

#if PLATFORM(GTK) || PLATFORM(WPE)
    bool shouldUseComplexTextControllerForSimpleText() const;
#endif

private:

    bool computeUseBackslashAsYenSymbol() const;

    bool advancedTextRenderingMode() const
    {
        return m_fontDescription.textRenderingMode() != TextRenderingMode::OptimizeSpeed;
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
        if (!m_fontDescription.variantSettings().isAllNormal())
            return true;
        if (m_fontDescription.featureSettings().size())
            return true;
        return advancedTextRenderingMode();
    }

    bool shouldUseComplexTextController(CodePath codePathToUse) const
    {
        switch (codePathToUse) {
        case CodePath::Complex:
            return true;
        case CodePath::Simple:
        case CodePath::SimpleWithGlyphOverflow:
#if PLATFORM(GTK) || PLATFORM(WPE)
            return shouldUseComplexTextControllerForSimpleText();
#else
            return false;
#endif
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    struct Spacing {
        float letter { 0 };
        float word { 0 };
        constexpr bool operator==(const Spacing&) const = default;
    };

    static constexpr unsigned bitsPerCharacterInCanUseSimplifiedTextMeasuringForAutoVariantCache = 2;

    mutable FontCascadeDescription m_fontDescription;
    Spacing m_spacing;
    mutable RefPtr<FontCascadeFonts> m_fonts;
    mutable RefPtr<FontSelector> m_fontSelector;
    mutable unsigned m_generation { 0 };
    bool m_useBackslashAsYenSymbol { false };
    bool m_enableKerning { false }; // Computed from m_fontDescription.
    bool m_requiresShaping { false }; // Computed from m_fontDescription.
    mutable WTF::BitSet<256 * bitsPerCharacterInCanUseSimplifiedTextMeasuringForAutoVariantCache> m_canUseSimplifiedTextMeasuringForAutoVariantCache;
};

bool shouldSynthesizeSmallCaps(bool, const Font*, char32_t, std::optional<char32_t>, FontVariantCaps, bool);
std::optional<char32_t> capitalized(char32_t);
inline char32_t mirrorCharacterIfNeeded(char32_t); // Defined in FontCascadeInlines.h

WTF::TextStream& operator<<(WTF::TextStream&, const FontCascade&);

} // namespace WebCore
