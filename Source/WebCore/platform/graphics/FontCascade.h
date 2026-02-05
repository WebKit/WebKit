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

#include <WebCore/FloatSegment.h>
#include <WebCore/Font.h>
#include <WebCore/FontCascadeDescription.h>
#include <WebCore/FontCascadeFonts.h>
#include <WebCore/Path.h>
#include <WebCore/TextSpacing.h>
#include <optional>
#include <wtf/CheckedRef.h>
#include <wtf/HashSet.h>
#include <wtf/Platform.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/CharacterProperties.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

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

    WEBCORE_EXPORT bool operator==(const FontCascade& other) const;

    const FontCascadeDescription& fontDescription() const { return m_fontDescription; }
    FontCascadeDescription& mutableFontDescription() const { return m_fontDescription; }

    float size() const { return fontDescription().computedSize(); }

    bool isCurrent(const FontSelector&) const;
    void updateFonts(Ref<FontCascadeFonts>&&) const;
    WEBCORE_EXPORT void update(RefPtr<FontSelector>&& = nullptr) const;
    unsigned fontSelectorVersion() const;

    enum class CustomFontNotReadyAction : bool { DoNotPaintIfFontNotReady, UseFallbackIfFontNotReady };
    WEBCORE_EXPORT FloatSize drawText(GraphicsContext&, const TextRun&, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt, CustomFontNotReadyAction = CustomFontNotReadyAction::DoNotPaintIfFontNotReady) const;
    static void drawGlyphs(GraphicsContext&, const Font&, std::span<const GlyphBufferGlyph>, std::span<const GlyphBufferAdvance>, const FloatPoint&, FontSmoothingMode);
    void drawEmphasisMarks(GraphicsContext&, const TextRun&, const AtomString& mark, const FloatPoint&, unsigned from = 0, std::optional<unsigned> to = std::nullopt) const;

    Vector<FloatSegment> lineSegmentsForIntersectionsWithRect(const TextRun&, const FloatPoint& textOrigin, const FloatRect& lineExtents) const;

    float widthOfTextRange(const TextRun&, unsigned from, unsigned to, float& outWidthBeforeRange, float& outWidthAfterRange) const;
    WEBCORE_EXPORT float width(const TextRun&, SingleThreadWeakHashSet<const Font>* fallbackFonts = nullptr, GlyphOverflow* = nullptr) const;
    WEBCORE_EXPORT float width(StringView) const;
    float widthForTextUsingSimplifiedMeasuring(StringView text, TextDirection = TextDirection::LTR) const;
    WEBCORE_EXPORT float widthForSimpleTextWithFixedPitch(StringView text, bool whitespaceIsCollapsed) const;
    float widthForCharacterInRun(const TextRun&, unsigned) const;

    std::unique_ptr<TextLayout, TextLayoutDeleter> createLayout(RenderText&, float xPos, bool collapseWhiteSpace) const;
    float widthOfSpaceString() const
    {
        return width(StringView(WTF::span(space)));
    }

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
    bool isFixedPitch() const;
    bool canTakeFixedPitchFastContentMeasuring() const;

    bool enableKerning() const { return m_enableKerning; }
    bool requiresShaping() const { return m_requiresShaping; }

    const AtomString& firstFamily() const { return m_fontDescription.firstFamily(); }
    unsigned familyCount() const { return m_fontDescription.familyCount(); }
    const AtomString& familyAt(unsigned i) const { return m_fontDescription.familyAt(i); }

    // A std::nullopt return value indicates "font-style: normal".
    std::optional<FontSelectionValue> fontStyleSlope() const { return m_fontDescription.fontStyleSlope(); }
    FontSelectionValue weight() const { return m_fontDescription.weight(); }
    FontWidthVariant widthVariant() const { return m_fontDescription.widthVariant(); }

    bool isPlatformFont() const { return m_fonts->isForPlatformFont(); }

    const FontMetrics& metricsOfPrimaryFont() const { return primaryFont()->fontMetrics(); }
    float zeroWidth() const;
    float tabWidth(const Font&, const TabSize&, float, Font::SyntheticBoldInclusion) const;
    bool hasValidAverageCharWidth() const;
    bool fastAverageCharWidthIfAvailable(float &width) const; // returns true on success

    int emphasisMarkAscent(const AtomString&) const;
    int emphasisMarkDescent(const AtomString&) const;
    int emphasisMarkHeight(const AtomString&) const;
    float floatEmphasisMarkHeight(const AtomString&) const;

    Ref<const Font> primaryFont() const;
    const FontRanges& fallbackRangesAt(unsigned) const;
    WEBCORE_EXPORT GlyphData glyphDataForCharacter(char32_t, bool mirror, FontVariant = AutoVariant, std::optional<ResolvedEmojiPolicy> = std::nullopt) const;
    bool canUseSimplifiedTextMeasuring(char32_t, FontVariant, bool whitespaceIsCollapsed, const Font&) const;

    RefPtr<const Font> fontForCombiningCharacterSequence(StringView) const;

    static bool isCJKIdeograph(char32_t);
    static bool isCJKIdeographOrSymbol(char32_t);

    static bool canUseGlyphDisplayList(const RenderStyle&);

    // Returns (the number of opportunities, whether the last expansion is a trailing expansion)
    // If there are no opportunities, the bool will be true iff we are forbidding leading expansions.
    static std::pair<unsigned, bool> expansionOpportunityCount(StringView, TextDirection, ExpansionBehavior);

    // Whether or not there is an expansion opportunity just before the first character
    // Note that this does not take a isAfterExpansion flag; this assumes that isAfterExpansion is false
    static bool leftExpansionOpportunity(StringView, TextDirection);
    static bool rightExpansionOpportunity(StringView, TextDirection);

    WEBCORE_EXPORT static void setDisableFontSubpixelAntialiasingForTesting(bool);
    WEBCORE_EXPORT static bool shouldDisableFontSubpixelAntialiasingForTesting();

    enum class CodePath : uint8_t { Simple, Complex, SimpleWithGlyphOverflow };
    WEBCORE_EXPORT CodePath codePath(const TextRun&, std::optional<unsigned> from = std::nullopt, std::optional<unsigned> to = std::nullopt) const;

    static CodePath characterRangeCodePath(std::span<const Latin1Character>) { return CodePath::Simple; }
    WEBCORE_EXPORT static CodePath characterRangeCodePath(std::span<const char16_t>);

    bool primaryFontIsSystemFont() const;

    static constexpr float syntheticObliqueAngle() { return 14; }

    RefPtr<const DisplayList::DisplayList> displayListForTextRun(GraphicsContext&, const TextRun&, unsigned from = 0, std::optional<unsigned> to = { }, CustomFontNotReadyAction = CustomFontNotReadyAction::DoNotPaintIfFontNotReady) const;

    unsigned generation() const { return m_generation; }

private:
    enum class ForTextEmphasisOrNot : bool { NotForTextEmphasis, ForTextEmphasis };

    GlyphBuffer layoutText(CodePath, const TextRun&, unsigned from, unsigned to, ForTextEmphasisOrNot = ForTextEmphasisOrNot::NotForTextEmphasis) const;
    GlyphBuffer layoutSimpleText(const TextRun&, unsigned from, unsigned to, ForTextEmphasisOrNot = ForTextEmphasisOrNot::NotForTextEmphasis) const;
    void drawGlyphBuffer(GraphicsContext&, const GlyphBuffer&, FloatPoint&, CustomFontNotReadyAction) const;
    void drawEmphasisMarks(GraphicsContext&, const GlyphBuffer&, const AtomString&, const FloatPoint&) const;
    int offsetForPositionForSimpleText(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForSimpleText(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;
    void adjustSelectionRectForSimpleTextWithFixedPitch(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;
    float width(CodePath, const TextRun&, SingleThreadWeakHashSet<const Font>* fallbackFonts = nullptr, GlyphOverflow* = nullptr) const;
    WEBCORE_EXPORT float widthForSimpleTextSlow(StringView text, TextDirection, FontCascadeFonts::GlyphGeometryCacheEntry*) const;
    ALWAYS_INLINE bool canHandleRunAsSimpleText(const TextRun&, unsigned from, unsigned to) const;

    std::optional<GlyphData> getEmphasisMarkGlyphData(const AtomString&) const;
    const Font* fontForEmphasisMark(const AtomString&) const;

    static constexpr bool canReturnFallbackFontsForComplexText();
    static constexpr bool canExpandAroundIdeographsInComplexText();

    GlyphBuffer layoutComplexText(const TextRun&, unsigned from, unsigned to, ForTextEmphasisOrNot = ForTextEmphasisOrNot::NotForTextEmphasis) const;
    int offsetForPositionForComplexText(const TextRun&, float position, bool includePartialGlyphs) const;
    void adjustSelectionRectForComplexText(const TextRun&, LayoutRect& selectionRect, unsigned from, unsigned to) const;

    static std::pair<unsigned, bool> expansionOpportunityCountInternal(std::span<const Latin1Character>, TextDirection, ExpansionBehavior);
    static std::pair<unsigned, bool> expansionOpportunityCountInternal(std::span<const char16_t>, TextDirection, ExpansionBehavior);

    friend struct WidthIterator;
    friend class ComplexTextController;

public:
#if ENABLE(TEXT_AUTOSIZING)
    bool equalForTextAutoSizing(const FontCascade& other) const
    {
        return m_fontDescription.equalForTextAutoSizing(other.m_fontDescription)
            && m_spacing == other.m_spacing;
    }
#endif

    // Useful for debugging the different font rendering code paths.
    WEBCORE_EXPORT static void setForcedCodePath(Markable<CodePath>);
    static Markable<CodePath> forcedCodePath();
    static Markable<CodePath> s_forcedCodePath;

    FontSelector* fontSelector() const;

    static bool isInvisibleReplacementObjectCharacter(char32_t character)
    {
        if (character != objectReplacementCharacter)
            return false;
#if PLATFORM(COCOA)
        // We make an exception for Books because some already available books when converted to EPUBS might contain object replacement character that should not be visible to the user.
        return WTF::CocoaApplication::isAppleBooks();
#else
        return false;
#endif
    }
    static bool treatAsSpace(char32_t c) { return c == space || c == tabCharacter || c == newlineCharacter || c == noBreakSpace; }
    static bool isCharacterWhoseGlyphsShouldBeDeletedForTextRendering(char32_t character)
    {
        // https://www.w3.org/TR/css-text-3/#white-space-processing
        // "Control characters (Unicode category Cc)—other than tabs (U+0009), line feeds (U+000A), carriage returns (U+000D) and sequences that form a segment break—must be rendered as a visible glyph"
        if (character == tabCharacter || character == newlineCharacter || character == carriageReturn)
            return true;
        // Also, we're omitting Null (U+0000) from this set because Chrome and Firefox do so and it's needed for compat. See https://github.com/w3c/csswg-drafts/pull/6983.
        if (character == nullCharacter)
            return true;
        if (isControlCharacter(character))
            return false;
        // "Unsupported Default_ignorable characters must be ignored for text rendering."
        return isDefaultIgnorableCodePoint(character) || isInvisibleReplacementObjectCharacter(character);
    }
    // FIXME: Callers of treatAsZeroWidthSpace() and treatAsZeroWidthSpaceInComplexScript() should probably be calling isCharacterWhoseGlyphsShouldBeDeletedForTextRendering() instead.
    static bool treatAsZeroWidthSpace(char32_t c) { return treatAsZeroWidthSpaceInComplexScript(c) || c == zeroWidthNonJoiner || c == zeroWidthJoiner; }
    static bool treatAsZeroWidthSpaceInComplexScript(char32_t c) { return c < space || (c >= deleteCharacter && c < noBreakSpace) || c == softHyphen || c == zeroWidthSpace || (c >= leftToRightMark && c <= rightToLeftMark) || (c >= leftToRightEmbed && c <= rightToLeftOverride) || c == zeroWidthNoBreakSpace || isInvisibleReplacementObjectCharacter(c); }
    static bool canReceiveTextEmphasis(char32_t);

    static inline char16_t normalizeSpaces(char16_t character)
    {
        if (treatAsSpace(character))
            return space;

        if (treatAsZeroWidthSpace(character))
            return zeroWidthSpace;

        return character;
    }

    static String normalizeSpaces(std::span<const Latin1Character>);
    static String normalizeSpaces(std::span<const char16_t>);
    static String normalizeSpaces(StringView);

    bool useBackslashAsYenSymbol() const { return m_useBackslashAsYenSymbol; }
    FontCascadeFonts* fonts() const { return m_fonts.get(); }
    RefPtr<FontCascadeFonts> protectedFonts() const { return m_fonts; }
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

inline Ref<const Font> FontCascade::primaryFont() const
{
    ASSERT(m_fonts);
    Ref font = protectedFonts()->primaryFont(m_fontDescription, protect(fontSelector()).get());
    m_fontDescription.resolveFontSizeAdjustFromFontIfNeeded(font);
    return font;
}

inline const FontRanges& FontCascade::fallbackRangesAt(unsigned index) const
{
    ASSERT(m_fonts);
    return protectedFonts()->realizeFallbackRangesAt(m_fontDescription, protect(fontSelector()).get(), index);
}

inline bool FontCascade::isFixedPitch() const
{
    ASSERT(m_fonts);
    return protectedFonts()->isFixedPitch(m_fontDescription, protect(fontSelector()).get());
}

inline bool FontCascade::canTakeFixedPitchFastContentMeasuring() const
{
    ASSERT(m_fonts);
    return protectedFonts()->canTakeFixedPitchFastContentMeasuring(m_fontDescription, protect(fontSelector()).get());
}

inline FontSelector* FontCascade::fontSelector() const
{
    return m_fontSelector.get();
}

inline float FontCascade::tabWidth(const Font& font, const TabSize& tabSize, float position, Font::SyntheticBoldInclusion syntheticBoldInclusion) const
{
    float baseTabWidth = tabSize.widthInPixels(font.spaceWidth());
    float result = 0;
    if (!baseTabWidth)
        result = letterSpacing();
    else {
        result = baseTabWidth - fmodf(position, baseTabWidth);
        if (result < font.spaceWidth() / 2)
            result += baseTabWidth;
    }
    // If our caller passes in SyntheticBoldInclusion::Exclude, that means they're going to apply synthetic bold themselves later.
    // However, regardless of that, the space characters that are fed into the width calculation need to have their correct width, including the synthetic bold.
    // So, we've already got synthetic bold applied, so if we're supposed to exclude it, we need to subtract it out here.
    return result - (syntheticBoldInclusion == Font::SyntheticBoldInclusion::Exclude ? font.syntheticBoldOffset() : 0);
}

inline float FontCascade::widthForTextUsingSimplifiedMeasuring(StringView text, TextDirection textDirection) const
{
    if (text.isEmpty())
        return 0;
    ASSERT(codePath(TextRun(text)) != CodePath::Complex);
    auto* cacheEntry = fonts()->glyphGeometryCache().add(text, { });
    if (cacheEntry && cacheEntry->width)
        return *cacheEntry->width;

    return widthForSimpleTextSlow(text, textDirection, cacheEntry);
}

bool shouldSynthesizeSmallCaps(bool, const Font*, char32_t, std::optional<char32_t>, FontVariantCaps, bool);
std::optional<char32_t> capitalized(char32_t);

WTF::TextStream& operator<<(WTF::TextStream&, const FontCascade&);

} // namespace WebCore
