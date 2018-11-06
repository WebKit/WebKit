/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FontCascade.h"

#include "CharacterProperties.h"
#include "ComplexTextController.h"
#include "DisplayListRecorder.h"
#include "FloatRect.h"
#include "FontCache.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "LayoutRect.h"
#include "SurrogatePairAwareTextIterator.h"
#include "TextRun.h"
#include "WidthIterator.h"
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(WIN)
#include "UniscribeController.h"
#endif

namespace WebCore {
using namespace WTF;
using namespace Unicode;

static bool useBackslashAsYenSignForFamily(const AtomicString& family)
{
    if (family.isEmpty())
        return false;
    static const auto set = makeNeverDestroyed([] {
        HashSet<AtomicString> set;
        auto add = [&set] (const char* name, std::initializer_list<UChar> unicodeName) {
            unsigned nameLength = strlen(name);
            set.add(AtomicString { name, nameLength, AtomicString::ConstructFromLiteral });
            unsigned unicodeNameLength = unicodeName.size();
            set.add(AtomicString { unicodeName.begin(), unicodeNameLength });
        };
        add("MS PGothic", { 0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x30B4, 0x30B7, 0x30C3, 0x30AF });
        add("MS PMincho", { 0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x660E, 0x671D });
        add("MS Gothic", { 0xFF2D, 0xFF33, 0x0020, 0x30B4, 0x30B7, 0x30C3, 0x30AF });
        add("MS Mincho", { 0xFF2D, 0xFF33, 0x0020, 0x660E, 0x671D });
        add("Meiryo", { 0x30E1, 0x30A4, 0x30EA, 0x30AA });
        return set;
    }());
    return set.get().contains(family);
}

FontCascade::CodePath FontCascade::s_codePath = Auto;

// ============================================================================================
// FontCascade Implementation (Cross-Platform Portion)
// ============================================================================================

FontCascade::FontCascade()
{
}

FontCascade::FontCascade(FontCascadeDescription&& fd, float letterSpacing, float wordSpacing)
    : m_fontDescription(WTFMove(fd))
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
    , m_useBackslashAsYenSymbol(useBackslashAsYenSignForFamily(m_fontDescription.firstFamily()))
    , m_enableKerning(computeEnableKerning())
    , m_requiresShaping(computeRequiresShaping())
{
}

// FIXME: We should make this constructor platform-independent.
FontCascade::FontCascade(const FontPlatformData& fontData, FontSmoothingMode fontSmoothingMode)
    : m_fonts(FontCascadeFonts::createForPlatformFont(fontData))
    , m_enableKerning(computeEnableKerning())
    , m_requiresShaping(computeRequiresShaping())
{
    m_fontDescription.setFontSmoothing(fontSmoothingMode);
#if PLATFORM(IOS_FAMILY)
    m_fontDescription.setSpecifiedSize(CTFontGetSize(fontData.font()));
    m_fontDescription.setComputedSize(CTFontGetSize(fontData.font()));
    m_fontDescription.setIsItalic(CTFontGetSymbolicTraits(fontData.font()) & kCTFontTraitItalic);
    m_fontDescription.setWeight((CTFontGetSymbolicTraits(fontData.font()) & kCTFontTraitBold) ? boldWeightValue() : normalWeightValue());
#endif
}

FontCascade::FontCascade(const FontCascade& other)
    : m_fontDescription(other.m_fontDescription)
    , m_fonts(other.m_fonts)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_useBackslashAsYenSymbol(other.m_useBackslashAsYenSymbol)
    , m_enableKerning(computeEnableKerning())
    , m_requiresShaping(computeRequiresShaping())
{
}

FontCascade& FontCascade::operator=(const FontCascade& other)
{
    m_fontDescription = other.m_fontDescription;
    m_fonts = other.m_fonts;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_useBackslashAsYenSymbol = other.m_useBackslashAsYenSymbol;
    m_enableKerning = other.m_enableKerning;
    m_requiresShaping = other.m_requiresShaping;
    return *this;
}

bool FontCascade::operator==(const FontCascade& other) const
{
    if (isLoadingCustomFonts() || other.isLoadingCustomFonts())
        return false;

    if (m_fontDescription != other.m_fontDescription || m_letterSpacing != other.m_letterSpacing || m_wordSpacing != other.m_wordSpacing)
        return false;
    if (m_fonts == other.m_fonts)
        return true;
    if (!m_fonts || !other.m_fonts)
        return false;
    if (m_fonts->fontSelector() != other.m_fonts->fontSelector())
        return false;
    // Can these cases actually somehow occur? All fonts should get wiped out by full style recalc.
    if (m_fonts->fontSelectorVersion() != other.m_fonts->fontSelectorVersion())
        return false;
    if (m_fonts->generation() != other.m_fonts->generation())
        return false;
    return true;
}

struct FontCascadeCacheKey {
    FontDescriptionKey fontDescriptionKey; // Shared with the lower level FontCache (caching Font objects)
    Vector<AtomicString, 3> families;
    unsigned fontSelectorId;
    unsigned fontSelectorVersion;
};

struct FontCascadeCacheEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontCascadeCacheEntry(FontCascadeCacheKey&& key, Ref<FontCascadeFonts>&& fonts)
        : key(WTFMove(key))
        , fonts(WTFMove(fonts))
    { }
    FontCascadeCacheKey key;
    Ref<FontCascadeFonts> fonts;
};

// FIXME: Should make hash traits for FontCascadeCacheKey instead of using a hash as the key (so we hash a hash).
typedef HashMap<unsigned, std::unique_ptr<FontCascadeCacheEntry>, AlreadyHashed> FontCascadeCache;

static bool keysMatch(const FontCascadeCacheKey& a, const FontCascadeCacheKey& b)
{
    if (a.fontDescriptionKey != b.fontDescriptionKey)
        return false;
    if (a.fontSelectorId != b.fontSelectorId || a.fontSelectorVersion != b.fontSelectorVersion)
        return false;
    unsigned size = a.families.size();
    if (size != b.families.size())
        return false;
    for (unsigned i = 0; i < size; ++i) {
        if (!FontCascadeDescription::familyNamesAreEqual(a.families[i], b.families[i]))
            return false;
    }
    return true;
}

static FontCascadeCache& fontCascadeCache()
{
    static NeverDestroyed<FontCascadeCache> cache;
    return cache.get();
}

void invalidateFontCascadeCache()
{
    fontCascadeCache().clear();
}

void clearWidthCaches()
{
    for (auto& value : fontCascadeCache().values())
        value->fonts.get().widthCache().clear();
}

static FontCascadeCacheKey makeFontCascadeCacheKey(const FontCascadeDescription& description, FontSelector* fontSelector)
{
    FontCascadeCacheKey key;
    key.fontDescriptionKey = FontDescriptionKey(description);
    unsigned familyCount = description.familyCount();
    key.families.reserveInitialCapacity(familyCount);
    for (unsigned i = 0; i < familyCount; ++i)
        key.families.uncheckedAppend(description.familyAt(i));
    key.fontSelectorId = fontSelector ? fontSelector->uniqueId() : 0;
    key.fontSelectorVersion = fontSelector ? fontSelector->version() : 0;
    return key;
}

static unsigned computeFontCascadeCacheHash(const FontCascadeCacheKey& key)
{
    // FIXME: Should hash the key and the family name characters rather than making a hash out of other hashes.
    IntegerHasher hasher;
    hasher.add(key.fontDescriptionKey.computeHash());
    hasher.add(key.fontSelectorId);
    hasher.add(key.fontSelectorVersion);
    for (unsigned i = 0; i < key.families.size(); ++i) {
        auto& family = key.families[i];
        hasher.add(family.isNull() ? 0 : FontCascadeDescription::familyNameHash(family));
    }
    return hasher.hash();
}

void pruneUnreferencedEntriesFromFontCascadeCache()
{
    fontCascadeCache().removeIf([](auto& entry) {
        return entry.value->fonts.get().hasOneRef();
    });
}

void pruneSystemFallbackFonts()
{
    for (auto& entry : fontCascadeCache().values())
        entry->fonts->pruneSystemFallbacks();
}

static Ref<FontCascadeFonts> retrieveOrAddCachedFonts(const FontCascadeDescription& fontDescription, RefPtr<FontSelector>&& fontSelector)
{
    auto key = makeFontCascadeCacheKey(fontDescription, fontSelector.get());

    unsigned hash = computeFontCascadeCacheHash(key);
    auto addResult = fontCascadeCache().add(hash, nullptr);
    if (!addResult.isNewEntry && keysMatch(addResult.iterator->value->key, key))
        return addResult.iterator->value->fonts.get();

    auto& newEntry = addResult.iterator->value;
    newEntry = std::make_unique<FontCascadeCacheEntry>(WTFMove(key), FontCascadeFonts::create(WTFMove(fontSelector)));
    Ref<FontCascadeFonts> glyphs = newEntry->fonts.get();

    static const unsigned unreferencedPruneInterval = 50;
    static const int maximumEntries = 400;
    static unsigned pruneCounter;
    // Referenced FontCascadeFonts would exist anyway so pruning them saves little memory.
    if (!(++pruneCounter % unreferencedPruneInterval))
        pruneUnreferencedEntriesFromFontCascadeCache();
    // Prevent pathological growth.
    if (fontCascadeCache().size() > maximumEntries)
        fontCascadeCache().remove(fontCascadeCache().random());
    return glyphs;
}

void FontCascade::update(RefPtr<FontSelector>&& fontSelector) const
{
    m_fonts = retrieveOrAddCachedFonts(m_fontDescription, WTFMove(fontSelector));
    m_useBackslashAsYenSymbol = useBackslashAsYenSignForFamily(firstFamily());
    m_enableKerning = computeEnableKerning();
    m_requiresShaping = computeRequiresShaping();
}

float FontCascade::glyphBufferForTextRun(CodePath codePathToUse, const TextRun& run, unsigned from, unsigned to, GlyphBuffer& glyphBuffer) const
{
    if (codePathToUse != Complex)
        return getGlyphsAndAdvancesForSimpleText(run, from, to, glyphBuffer);
    return getGlyphsAndAdvancesForComplexText(run, from, to, glyphBuffer);
}

float FontCascade::drawText(GraphicsContext& context, const TextRun& run, const FloatPoint& point, unsigned from, std::optional<unsigned> to, CustomFontNotReadyAction customFontNotReadyAction) const
{
    unsigned destination = to.value_or(run.length());
    GlyphBuffer glyphBuffer;
    float startX = point.x() + glyphBufferForTextRun(codePath(run, from, to), run, from, destination, glyphBuffer);
    // We couldn't generate any glyphs for the run. Give up.
    if (glyphBuffer.isEmpty())
        return 0;
    // Draw the glyph buffer now at the starting point returned in startX.
    FloatPoint startPoint(startX, point.y());
    drawGlyphBuffer(context, glyphBuffer, startPoint, customFontNotReadyAction);
    return startPoint.x() - startX;
}

void FontCascade::drawEmphasisMarks(GraphicsContext& context, const TextRun& run, const AtomicString& mark, const FloatPoint& point, unsigned from, std::optional<unsigned> to) const
{
    if (isLoadingCustomFonts())
        return;

    unsigned destination = to.value_or(run.length());
    if (codePath(run, from, to) != Complex)
        drawEmphasisMarksForSimpleText(context, run, mark, point, from, destination);
    else
        drawEmphasisMarksForComplexText(context, run, mark, point, from, destination);
}

std::unique_ptr<DisplayList::DisplayList> FontCascade::displayListForTextRun(GraphicsContext& context, const TextRun& run, unsigned from, std::optional<unsigned> to, CustomFontNotReadyAction customFontNotReadyAction) const
{
    ASSERT(!context.paintingDisabled());
    unsigned destination = to.value_or(run.length());
    
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    CodePath codePathToUse = codePath(run);
    if (codePathToUse != Complex && (enableKerning() || requiresShaping()) && (from || destination != run.length()))
        codePathToUse = Complex;
    
    GlyphBuffer glyphBuffer;
    float startX = glyphBufferForTextRun(codePathToUse, run, from, destination, glyphBuffer);
    // We couldn't generate any glyphs for the run. Give up.
    if (glyphBuffer.isEmpty())
        return nullptr;
    
    std::unique_ptr<DisplayList::DisplayList> displayList = std::make_unique<DisplayList::DisplayList>();
    GraphicsContext recordingContext([&](GraphicsContext& displayListContext) {
        return std::make_unique<DisplayList::Recorder>(displayListContext, *displayList, context.state(), FloatRect(), AffineTransform());
    });
    
    FloatPoint startPoint(startX, 0);
    drawGlyphBuffer(recordingContext, glyphBuffer, startPoint, customFontNotReadyAction);
    return displayList;
}
    
float FontCascade::widthOfTextRange(const TextRun& run, unsigned from, unsigned to, HashSet<const Font*>* fallbackFonts, float* outWidthBeforeRange, float* outWidthAfterRange) const
{
    ASSERT(from <= to);
    ASSERT(to <= run.length());

    if (!run.length())
        return 0;

    float offsetBeforeRange = 0;
    float offsetAfterRange = 0;
    float totalWidth = 0;

    auto codePathToUse = codePath(run);
    if (codePathToUse == Complex) {
#if PLATFORM(WIN)
        UniscribeController it(this, run);
        it.advance(from);
        offsetBeforeRange = it.runWidthSoFar();
        it.advance(to);
        offsetAfterRange = it.runWidthSoFar();
        it.advance(to);
        totalWidth = it.runWidthSoFar();
#else
        ComplexTextController complexIterator(*this, run, false, fallbackFonts);
        complexIterator.advance(from, nullptr, IncludePartialGlyphs, fallbackFonts);
        offsetBeforeRange = complexIterator.runWidthSoFar();
        complexIterator.advance(to, nullptr, IncludePartialGlyphs, fallbackFonts);
        offsetAfterRange = complexIterator.runWidthSoFar();
        complexIterator.advance(run.length(), nullptr, IncludePartialGlyphs, fallbackFonts);
        totalWidth = complexIterator.runWidthSoFar();
#endif
    } else {
        WidthIterator simpleIterator(this, run, fallbackFonts);
        simpleIterator.advance(from, nullptr);
        offsetBeforeRange = simpleIterator.runWidthSoFar();
        simpleIterator.advance(to, nullptr);
        offsetAfterRange = simpleIterator.runWidthSoFar();
        simpleIterator.advance(run.length(), nullptr);
        totalWidth = simpleIterator.runWidthSoFar();
    }

    if (outWidthBeforeRange)
        *outWidthBeforeRange = offsetBeforeRange;

    if (outWidthAfterRange)
        *outWidthAfterRange = totalWidth - offsetAfterRange;

    return offsetAfterRange - offsetBeforeRange;
}

float FontCascade::width(const TextRun& run, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    if (!run.length())
        return 0;

    CodePath codePathToUse = codePath(run);
    if (codePathToUse != Complex) {
        // The complex path is more restrictive about returning fallback fonts than the simple path, so we need an explicit test to make their behaviors match.
        if (!canReturnFallbackFontsForComplexText())
            fallbackFonts = nullptr;
        // The simple path can optimize the case where glyph overflow is not observable.
        if (codePathToUse != SimpleWithGlyphOverflow && (glyphOverflow && !glyphOverflow->computeBounds))
            glyphOverflow = nullptr;
    }

    bool hasWordSpacingOrLetterSpacing = wordSpacing() || letterSpacing();
    float* cacheEntry = m_fonts->widthCache().add(run, std::numeric_limits<float>::quiet_NaN(), enableKerning() || requiresShaping(), hasWordSpacingOrLetterSpacing, glyphOverflow);
    if (cacheEntry && !std::isnan(*cacheEntry))
        return *cacheEntry;

    HashSet<const Font*> localFallbackFonts;
    if (!fallbackFonts)
        fallbackFonts = &localFallbackFonts;

    float result;
    if (codePathToUse == Complex)
        result = floatWidthForComplexText(run, fallbackFonts, glyphOverflow);
    else
        result = floatWidthForSimpleText(run, fallbackFonts, glyphOverflow);

    if (cacheEntry && fallbackFonts->isEmpty())
        *cacheEntry = result;
    return result;
}

float FontCascade::widthForSimpleText(StringView text) const
{
    if (text.isNull() || text.isEmpty())
        return 0;
    ASSERT(codePath(TextRun(text)) != FontCascade::Complex);
    float* cacheEntry = m_fonts->widthCache().add(text, std::numeric_limits<float>::quiet_NaN());
    if (cacheEntry && !std::isnan(*cacheEntry))
        return *cacheEntry;

    Vector<GlyphBufferGlyph, 16> glyphs;
    Vector<GlyphBufferAdvance, 16> advances;
    bool hasKerningOrLigatures = enableKerning() || requiresShaping();
    float runWidth = 0;
    auto& font = primaryFont();
    for (unsigned i = 0; i < text.length(); ++i) {
        auto glyph = glyphDataForCharacter(text[i], false).glyph;
        auto glyphWidth = font.widthForGlyph(glyph);
        runWidth += glyphWidth;
        if (!hasKerningOrLigatures)
            continue;
        glyphs.append(glyph);
        advances.append(FloatSize(glyphWidth, 0));
    }
    if (hasKerningOrLigatures) {
        font.applyTransforms(&glyphs[0], &advances[0], glyphs.size(), enableKerning(), requiresShaping());
        // This is needed only to match the result of the slow path. Same glyph widths but different floating point arithmentics can
        // produce different run width.
        float runWidthDifferenceWithTransformApplied = -runWidth;
        for (auto& advance : advances)
            runWidthDifferenceWithTransformApplied += advance.width();
        runWidth += runWidthDifferenceWithTransformApplied;
    }

    if (cacheEntry)
        *cacheEntry = runWidth;
    return runWidth;
}

GlyphData FontCascade::glyphDataForCharacter(UChar32 c, bool mirror, FontVariant variant) const
{
    if (variant == AutoVariant) {
        if (m_fontDescription.variantCaps() == FontVariantCaps::Small) {
            UChar32 upperC = u_toupper(c);
            if (upperC != c) {
                c = upperC;
                variant = SmallCapsVariant;
            } else
                variant = NormalVariant;
        } else
            variant = NormalVariant;
    }

    if (mirror)
        c = u_charMirror(c);

    return m_fonts->glyphDataForCharacter(c, m_fontDescription, variant);
}

// For font families where any of the fonts don't have a valid entry in the OS/2 table
// for avgCharWidth, fallback to the legacy webkit behavior of getting the avgCharWidth
// from the width of a '0'. This only seems to apply to a fixed number of Mac fonts,
// but, in order to get similar rendering across platforms, we do this check for
// all platforms.
bool FontCascade::hasValidAverageCharWidth() const
{
    const AtomicString& family = firstFamily();
    if (family.isEmpty())
        return false;

#if PLATFORM(COCOA)
    // Internal fonts on macOS and iOS also have an invalid entry in the table for avgCharWidth.
    if (primaryFontIsSystemFont())
        return false;
#endif

    static const auto map = makeNeverDestroyed(HashSet<AtomicString> {
        "American Typewriter",
        "Arial Hebrew",
        "Chalkboard",
        "Cochin",
        "Corsiva Hebrew",
        "Courier",
        "Euphemia UCAS",
        "Geneva",
        "Gill Sans",
        "Hei",
        "Helvetica",
        "Hoefler Text",
        "InaiMathi",
        "Kai",
        "Lucida Grande",
        "Marker Felt",
        "Monaco",
        "Mshtakan",
        "New Peninim MT",
        "Osaka",
        "Raanana",
        "STHeiti",
        "Symbol",
        "Times",
        "Apple Braille",
        "Apple LiGothic",
        "Apple LiSung",
        "Apple Symbols",
        "AppleGothic",
        "AppleMyungjo",
        "#GungSeo",
        "#HeadLineA",
        "#PCMyungjo",
        "#PilGi",
    });
    return !map.get().contains(family);
}

bool FontCascade::fastAverageCharWidthIfAvailable(float& width) const
{
    bool success = hasValidAverageCharWidth();
    if (success)
        width = roundf(primaryFont().avgCharWidth()); // FIXME: primaryFont() might not correspond to firstFamily().
    return success;
}

void FontCascade::adjustSelectionRectForText(const TextRun& run, LayoutRect& selectionRect, unsigned from, std::optional<unsigned> to) const
{
    unsigned destination = to.value_or(run.length());
    if (codePath(run, from, to) != Complex)
        return adjustSelectionRectForSimpleText(run, selectionRect, from, destination);

    return adjustSelectionRectForComplexText(run, selectionRect, from, destination);
}

int FontCascade::offsetForPosition(const TextRun& run, float x, bool includePartialGlyphs) const
{
    if (codePath(run, x) != Complex)
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);

    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

template <typename CharacterType>
static inline String normalizeSpacesInternal(const CharacterType* characters, unsigned length)
{
    StringBuilder normalized;
    normalized.reserveCapacity(length);

    for (unsigned i = 0; i < length; ++i)
        normalized.append(FontCascade::normalizeSpaces(characters[i]));

    return normalized.toString();
}

String FontCascade::normalizeSpaces(const LChar* characters, unsigned length)
{
    return normalizeSpacesInternal(characters, length);
}

String FontCascade::normalizeSpaces(const UChar* characters, unsigned length)
{
    return normalizeSpacesInternal(characters, length);
}

static bool shouldUseFontSmoothing = true;

void FontCascade::setShouldUseSmoothing(bool shouldUseSmoothing)
{
    ASSERT(isMainThread());
    shouldUseFontSmoothing = shouldUseSmoothing;
}

bool FontCascade::shouldUseSmoothing()
{
    return shouldUseFontSmoothing;
}

void FontCascade::setCodePath(CodePath p)
{
    s_codePath = p;
}

FontCascade::CodePath FontCascade::codePath()
{
    return s_codePath;
}

FontCascade::CodePath FontCascade::codePath(const TextRun& run, std::optional<unsigned> from, std::optional<unsigned> to) const
{
    if (s_codePath != Auto)
        return s_codePath;

#if !USE(FREETYPE)
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if ((enableKerning() || requiresShaping()) && (from.value_or(0) || to.value_or(run.length()) != run.length()))
        return Complex;
#else
    UNUSED_PARAM(from);
    UNUSED_PARAM(to);
#endif

#if PLATFORM(COCOA) || USE(FREETYPE)
    // Because Font::applyTransforms() doesn't know which features to enable/disable in the simple code path, it can't properly handle feature or variant settings.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=150791: @font-face features should also cause this to be complex.
    if (m_fontDescription.featureSettings().size() > 0 || !m_fontDescription.variantSettings().isAllNormal())
        return Complex;

#else

    if (run.length() > 1 && (enableKerning() || requiresShaping()))
        return Complex;
#endif

    if (!run.characterScanForCodePath())
        return Simple;

    if (run.is8Bit())
        return Simple;

    // Start from 0 since drawing and highlighting also measure the characters before run->from.
    return characterRangeCodePath(run.characters16(), run.length());
}

FontCascade::CodePath FontCascade::characterRangeCodePath(const UChar* characters, unsigned len)
{
    // FIXME: Should use a UnicodeSet in ports where ICU is used. Note that we 
    // can't simply use UnicodeCharacter Property/class because some characters
    // are not 'combining', but still need to go to the complex path.
    // Alternatively, we may as well consider binary search over a sorted
    // list of ranges.
    CodePath result = Simple;
    bool previousCharacterIsEmojiGroupCandidate = false;
    for (unsigned i = 0; i < len; i++) {
        const UChar c = characters[i];
        if (c == zeroWidthJoiner && previousCharacterIsEmojiGroupCandidate)
            return Complex;
        
        previousCharacterIsEmojiGroupCandidate = false;
        if (c < 0x2E5) // U+02E5 through U+02E9 (Modifier Letters : Tone letters)  
            continue;
        if (c <= 0x2E9) 
            return Complex;

        if (c < 0x300) // U+0300 through U+036F Combining diacritical marks
            continue;
        if (c <= 0x36F)
            return Complex;

        if (c < 0x0591 || c == 0x05BE) // U+0591 through U+05CF excluding U+05BE Hebrew combining marks, Hebrew punctuation Paseq, Sof Pasuq and Nun Hafukha
            continue;
        if (c <= 0x05CF)
            return Complex;

        // U+0600 through U+109F Arabic, Syriac, Thaana, NKo, Samaritan, Mandaic,
        // Devanagari, Bengali, Gurmukhi, Gujarati, Oriya, Tamil, Telugu, Kannada, 
        // Malayalam, Sinhala, Thai, Lao, Tibetan, Myanmar
        if (c < 0x0600) 
            continue;
        if (c <= 0x109F)
            return Complex;

        // U+1100 through U+11FF Hangul Jamo (only Ancient Korean should be left here if you precompose;
        // Modern Korean will be precomposed as a result of step A)
        if (c < 0x1100)
            continue;
        if (c <= 0x11FF)
            return Complex;

        if (c < 0x135D) // U+135D through U+135F Ethiopic combining marks
            continue;
        if (c <= 0x135F)
            return Complex;

        if (c < 0x1700) // U+1780 through U+18AF Tagalog, Hanunoo, Buhid, Taghanwa,Khmer, Mongolian
            continue;
        if (c <= 0x18AF)
            return Complex;

        if (c < 0x1900) // U+1900 through U+194F Limbu (Unicode 4.0)
            continue;
        if (c <= 0x194F)
            return Complex;

        if (c < 0x1980) // U+1980 through U+19DF New Tai Lue
            continue;
        if (c <= 0x19DF)
            return Complex;

        if (c < 0x1A00) // U+1A00 through U+1CFF Buginese, Tai Tham, Balinese, Batak, Lepcha, Vedic
            continue;
        if (c <= 0x1CFF)
            return Complex;

        if (c < 0x1DC0) // U+1DC0 through U+1DFF Comining diacritical mark supplement
            continue;
        if (c <= 0x1DFF)
            return Complex;

        // U+1E00 through U+2000 characters with diacritics and stacked diacritics
        if (c <= 0x2000) {
            result = SimpleWithGlyphOverflow;
            continue;
        }

        if (c < 0x20D0) // U+20D0 through U+20FF Combining marks for symbols
            continue;
        if (c <= 0x20FF)
            return Complex;

        if (c < 0x26F9)
            continue;
        if (c < 0x26FA)
            return Complex;

        if (c < 0x2CEF) // U+2CEF through U+2CF1 Combining marks for Coptic
            continue;
        if (c <= 0x2CF1)
            return Complex;

        if (c < 0x302A) // U+302A through U+302F Ideographic and Hangul Tone marks
            continue;
        if (c <= 0x302F)
            return Complex;

        if (c < 0xA67C) // U+A67C through U+A67D Combining marks for old Cyrillic
            continue;
        if (c <= 0xA67D)
            return Complex;

        if (c < 0xA6F0) // U+A6F0 through U+A6F1 Combining mark for Bamum
            continue;
        if (c <= 0xA6F1)
            return Complex;

        // U+A800 through U+ABFF Nagri, Phags-pa, Saurashtra, Devanagari Extended,
        // Hangul Jamo Ext. A, Javanese, Myanmar Extended A, Tai Viet, Meetei Mayek,
        if (c < 0xA800) 
            continue;
        if (c <= 0xABFF)
            return Complex;

        if (c < 0xD7B0) // U+D7B0 through U+D7FF Hangul Jamo Ext. B
            continue;
        if (c <= 0xD7FF)
            return Complex;

        if (c <= 0xDBFF) {
            // High surrogate

            if (i == len - 1)
                continue;

            UChar next = characters[++i];
            if (!U16_IS_TRAIL(next))
                continue;

            UChar32 supplementaryCharacter = U16_GET_SUPPLEMENTARY(c, next);

            if (supplementaryCharacter < 0x10A00)
                continue;
            if (supplementaryCharacter < 0x10A60) // Kharoshthi
                return Complex;
            if (supplementaryCharacter < 0x11000)
                continue;
            if (supplementaryCharacter < 0x11080) // Brahmi
                return Complex;
            if (supplementaryCharacter < 0x110D0) // Kaithi
                return Complex;
            if (supplementaryCharacter < 0x11100)
                continue;
            if (supplementaryCharacter < 0x11150) // Chakma
                return Complex;
            if (supplementaryCharacter < 0x11180) // Mahajani
                return Complex;
            if (supplementaryCharacter < 0x111E0) // Sharada
                return Complex;
            if (supplementaryCharacter < 0x11200)
                continue;
            if (supplementaryCharacter < 0x11250) // Khojki
                return Complex;
            if (supplementaryCharacter < 0x112B0)
                continue;
            if (supplementaryCharacter < 0x11300) // Khudawadi
                return Complex;
            if (supplementaryCharacter < 0x11380) // Grantha
                return Complex;
            if (supplementaryCharacter < 0x11400)
                continue;
            if (supplementaryCharacter < 0x11480) // Newa
                return Complex;
            if (supplementaryCharacter < 0x114E0) // Tirhuta
                return Complex;
            if (supplementaryCharacter < 0x11580)
                continue;
            if (supplementaryCharacter < 0x11600) // Siddham
                return Complex;
            if (supplementaryCharacter < 0x11660) // Modi
                return Complex;
            if (supplementaryCharacter < 0x11680)
                continue;
            if (supplementaryCharacter < 0x116D0) // Takri
                return Complex;
            if (supplementaryCharacter < 0x11C00)
                continue;
            if (supplementaryCharacter < 0x11C70) // Bhaiksuki
                return Complex;
            if (supplementaryCharacter < 0x11CC0) // Marchen
                return Complex;
            if (supplementaryCharacter < 0x1E900)
                continue;
            if (supplementaryCharacter < 0x1E960) // Adlam
                return Complex;
            if (supplementaryCharacter < 0x1F1E6) // U+1F1E6 through U+1F1FF Regional Indicator Symbols
                continue;
            if (supplementaryCharacter <= 0x1F1FF)
                return Complex;

            if (isEmojiFitzpatrickModifier(supplementaryCharacter))
                return Complex;
            if (isEmojiGroupCandidate(supplementaryCharacter)) {
                previousCharacterIsEmojiGroupCandidate = true;
                continue;
            }

            if (supplementaryCharacter < 0xE0000)
                continue;
            if (supplementaryCharacter < 0xE0080) // Tags
                return Complex;
            if (supplementaryCharacter < 0xE0100) // U+E0100 through U+E01EF Unicode variation selectors.
                continue;
            if (supplementaryCharacter <= 0xE01EF)
                return Complex;

            // FIXME: Check for Brahmi (U+11000 block), Kaithi (U+11080 block) and other complex scripts
            // in plane 1 or higher.

            continue;
        }

        if (c < 0xFE00) // U+FE00 through U+FE0F Unicode variation selectors
            continue;
        if (c <= 0xFE0F)
            return Complex;

        if (c < 0xFE20) // U+FE20 through U+FE2F Combining half marks
            continue;
        if (c <= 0xFE2F)
            return Complex;
    }
    return result;
}

bool FontCascade::isCJKIdeograph(UChar32 c)
{
    // The basic CJK Unified Ideographs block.
    if (c >= 0x4E00 && c <= 0x9FFF)
        return true;
    
    // CJK Unified Ideographs Extension A.
    if (c >= 0x3400 && c <= 0x4DBF)
        return true;
    
    // CJK Radicals Supplement.
    if (c >= 0x2E80 && c <= 0x2EFF)
        return true;
    
    // Kangxi Radicals.
    if (c >= 0x2F00 && c <= 0x2FDF)
        return true;
    
    // CJK Strokes.
    if (c >= 0x31C0 && c <= 0x31EF)
        return true;
    
    // CJK Compatibility Ideographs.
    if (c >= 0xF900 && c <= 0xFAFF)
        return true;

    // CJK Unified Ideographs Extension B.
    if (c >= 0x20000 && c <= 0x2A6DF)
        return true;

    // CJK Unified Ideographs Extension C.
    if (c >= 0x2A700 && c <= 0x2B73F)
        return true;
    
    // CJK Unified Ideographs Extension D.
    if (c >= 0x2B740 && c <= 0x2B81F)
        return true;
    
    // CJK Compatibility Ideographs Supplement.
    if (c >= 0x2F800 && c <= 0x2FA1F)
        return true;

    return false;
}

bool FontCascade::isCJKIdeographOrSymbol(UChar32 c)
{
    // 0x2C7 Caron, Mandarin Chinese 3rd Tone
    // 0x2CA Modifier Letter Acute Accent, Mandarin Chinese 2nd Tone
    // 0x2CB Modifier Letter Grave Access, Mandarin Chinese 4th Tone 
    // 0x2D9 Dot Above, Mandarin Chinese 5th Tone 
    if ((c == 0x2C7) || (c == 0x2CA) || (c == 0x2CB) || (c == 0x2D9))
        return true;

    if ((c == 0x2020) || (c == 0x2021) || (c == 0x2030) || (c == 0x203B) || (c == 0x203C)
        || (c == 0x2042) || (c == 0x2047) || (c == 0x2048) || (c == 0x2049) || (c == 0x2051)
        || (c == 0x20DD) || (c == 0x20DE) || (c == 0x2100) || (c == 0x2103) || (c == 0x2105)
        || (c == 0x2109) || (c == 0x210A) || (c == 0x2113) || (c == 0x2116) || (c == 0x2121)
        || (c == 0x212B) || (c == 0x213B) || (c == 0x2150) || (c == 0x2151) || (c == 0x2152))
        return true;

    if (c >= 0x2156 && c <= 0x215A)
        return true;

    if (c >= 0x2160 && c <= 0x216B)
        return true;

    if (c >= 0x2170 && c <= 0x217B)
        return true;

    if ((c == 0x217F) || (c == 0x2189) || (c == 0x2307) || (c == 0x2312) || (c == 0x23BE) || (c == 0x23BF))
        return true;

    if (c >= 0x23C0 && c <= 0x23CC)
        return true;

    if ((c == 0x23CE) || (c == 0x2423))
        return true;

    if (c >= 0x2460 && c <= 0x2492)
        return true;

    if (c >= 0x249C && c <= 0x24FF)
        return true;

    if ((c == 0x25A0) || (c == 0x25A1) || (c == 0x25A2) || (c == 0x25AA) || (c == 0x25AB))
        return true;

    if ((c == 0x25B1) || (c == 0x25B2) || (c == 0x25B3) || (c == 0x25B6) || (c == 0x25B7) || (c == 0x25BC) || (c == 0x25BD))
        return true;
    
    if ((c == 0x25C0) || (c == 0x25C1) || (c == 0x25C6) || (c == 0x25C7) || (c == 0x25C9) || (c == 0x25CB) || (c == 0x25CC))
        return true;

    if (c >= 0x25CE && c <= 0x25D3)
        return true;

    if (c >= 0x25E2 && c <= 0x25E6)
        return true;

    if (c == 0x25EF)
        return true;

    if (c >= 0x2600 && c <= 0x2603)
        return true;

    if ((c == 0x2605) || (c == 0x2606) || (c == 0x260E) || (c == 0x2616) || (c == 0x2617) || (c == 0x2640) || (c == 0x2642))
        return true;

    if (c >= 0x2660 && c <= 0x266F)
        return true;

    if (c >= 0x2672 && c <= 0x267D)
        return true;

    if ((c == 0x26A0) || (c == 0x26BD) || (c == 0x26BE) || (c == 0x2713) || (c == 0x271A) || (c == 0x273F) || (c == 0x2740) || (c == 0x2756))
        return true;

    if (c >= 0x2776 && c <= 0x277F)
        return true;

    if (c == 0x2B1A)
        return true;

    // Ideographic Description Characters.
    if (c >= 0x2FF0 && c <= 0x2FFF)
        return true;

    // CJK Symbols and Punctuation, excluding 0x3030.
    if (c >= 0x3000 && c < 0x3030)
        return true;

    if (c > 0x3030 && c <= 0x303F)
        return true;

    // Hiragana
    if (c >= 0x3040 && c <= 0x309F)
        return true;

    // Katakana 
    if (c >= 0x30A0 && c <= 0x30FF)
        return true;

    // Bopomofo
    if (c >= 0x3100 && c <= 0x312F)
        return true;

    if (c >= 0x3190 && c <= 0x319F)
        return true;

    // Bopomofo Extended
    if (c >= 0x31A0 && c <= 0x31BF)
        return true;

    // Enclosed CJK Letters and Months.
    if (c >= 0x3200 && c <= 0x32FF)
        return true;
    
    // CJK Compatibility.
    if (c >= 0x3300 && c <= 0x33FF)
        return true;

    if (c >= 0xF860 && c <= 0xF862)
        return true;

    // CJK Compatibility Forms.
    if (c >= 0xFE30 && c <= 0xFE4F)
        return true;

    if ((c == 0xFE10) || (c == 0xFE11) || (c == 0xFE12) || (c == 0xFE19))
        return true;

    if ((c == 0xFF0D) || (c == 0xFF1B) || (c == 0xFF1C) || (c == 0xFF1E))
        return false;

    // Halfwidth and Fullwidth Forms
    // Usually only used in CJK
    if (c >= 0xFF00 && c <= 0xFFEF)
        return true;

    // Emoji.
    if (c == 0x1F100)
        return true;

    if (c >= 0x1F110 && c <= 0x1F129)
        return true;

    if (c >= 0x1F130 && c <= 0x1F149)
        return true;

    if (c >= 0x1F150 && c <= 0x1F169)
        return true;

    if (c >= 0x1F170 && c <= 0x1F189)
        return true;

    if (c >= 0x1F200 && c <= 0x1F6C5)
        return true;

    return isCJKIdeograph(c);
}

std::pair<unsigned, bool> FontCascade::expansionOpportunityCountInternal(const LChar* characters, unsigned length, TextDirection direction, ExpansionBehavior expansionBehavior)
{
    unsigned count = 0;
    bool isAfterExpansion = (expansionBehavior & LeadingExpansionMask) == ForbidLeadingExpansion;
    if ((expansionBehavior & LeadingExpansionMask) == ForceLeadingExpansion) {
        ++count;
        isAfterExpansion = true;
    }
    if (direction == TextDirection::LTR) {
        for (unsigned i = 0; i < length; ++i) {
            if (treatAsSpace(characters[i])) {
                count++;
                isAfterExpansion = true;
            } else
                isAfterExpansion = false;
        }
    } else {
        for (unsigned i = length; i > 0; --i) {
            if (treatAsSpace(characters[i - 1])) {
                count++;
                isAfterExpansion = true;
            } else
                isAfterExpansion = false;
        }
    }
    if (!isAfterExpansion && (expansionBehavior & TrailingExpansionMask) == ForceTrailingExpansion) {
        ++count;
        isAfterExpansion = true;
    } else if (isAfterExpansion && (expansionBehavior & TrailingExpansionMask) == ForbidTrailingExpansion) {
        ASSERT(count);
        --count;
        isAfterExpansion = false;
    }
    return std::make_pair(count, isAfterExpansion);
}

std::pair<unsigned, bool> FontCascade::expansionOpportunityCountInternal(const UChar* characters, unsigned length, TextDirection direction, ExpansionBehavior expansionBehavior)
{
    static bool expandAroundIdeographs = canExpandAroundIdeographsInComplexText();
    unsigned count = 0;
    bool isAfterExpansion = (expansionBehavior & LeadingExpansionMask) == ForbidLeadingExpansion;
    if ((expansionBehavior & LeadingExpansionMask) == ForceLeadingExpansion) {
        ++count;
        isAfterExpansion = true;
    }
    if (direction == TextDirection::LTR) {
        for (unsigned i = 0; i < length; ++i) {
            UChar32 character = characters[i];
            if (treatAsSpace(character)) {
                count++;
                isAfterExpansion = true;
                continue;
            }
            if (U16_IS_LEAD(character) && i + 1 < length && U16_IS_TRAIL(characters[i + 1])) {
                character = U16_GET_SUPPLEMENTARY(character, characters[i + 1]);
                i++;
            }
            if (expandAroundIdeographs && isCJKIdeographOrSymbol(character)) {
                if (!isAfterExpansion)
                    count++;
                count++;
                isAfterExpansion = true;
                continue;
            }
            isAfterExpansion = false;
        }
    } else {
        for (unsigned i = length; i > 0; --i) {
            UChar32 character = characters[i - 1];
            if (treatAsSpace(character)) {
                count++;
                isAfterExpansion = true;
                continue;
            }
            if (U16_IS_TRAIL(character) && i > 1 && U16_IS_LEAD(characters[i - 2])) {
                character = U16_GET_SUPPLEMENTARY(characters[i - 2], character);
                i--;
            }
            if (expandAroundIdeographs && isCJKIdeographOrSymbol(character)) {
                if (!isAfterExpansion)
                    count++;
                count++;
                isAfterExpansion = true;
                continue;
            }
            isAfterExpansion = false;
        }
    }
    if (!isAfterExpansion && (expansionBehavior & TrailingExpansionMask) == ForceTrailingExpansion) {
        ++count;
        isAfterExpansion = true;
    } else if (isAfterExpansion && (expansionBehavior & TrailingExpansionMask) == ForbidTrailingExpansion) {
        ASSERT(count);
        --count;
        isAfterExpansion = false;
    }
    return std::make_pair(count, isAfterExpansion);
}

std::pair<unsigned, bool> FontCascade::expansionOpportunityCount(const StringView& stringView, TextDirection direction, ExpansionBehavior expansionBehavior)
{
    // For each character, iterating from left to right:
    //   If it is recognized as a space, insert an opportunity after it
    //   If it is an ideograph, insert one opportunity before it and one opportunity after it
    // Do this such a way so that there are not two opportunities next to each other.
    if (stringView.is8Bit())
        return expansionOpportunityCountInternal(stringView.characters8(), stringView.length(), direction, expansionBehavior);
    return expansionOpportunityCountInternal(stringView.characters16(), stringView.length(), direction, expansionBehavior);
}

bool FontCascade::leadingExpansionOpportunity(const StringView& stringView, TextDirection direction)
{
    if (!stringView.length())
        return false;

    UChar32 initialCharacter;
    if (direction == TextDirection::LTR) {
        initialCharacter = stringView[0];
        if (U16_IS_LEAD(initialCharacter) && stringView.length() > 1 && U16_IS_TRAIL(stringView[1]))
            initialCharacter = U16_GET_SUPPLEMENTARY(initialCharacter, stringView[1]);
    } else {
        initialCharacter = stringView[stringView.length() - 1];
        if (U16_IS_TRAIL(initialCharacter) && stringView.length() > 1 && U16_IS_LEAD(stringView[stringView.length() - 2]))
            initialCharacter = U16_GET_SUPPLEMENTARY(stringView[stringView.length() - 2], initialCharacter);
    }

    return canExpandAroundIdeographsInComplexText() && isCJKIdeographOrSymbol(initialCharacter);
}

bool FontCascade::trailingExpansionOpportunity(const StringView& stringView, TextDirection direction)
{
    if (!stringView.length())
        return false;

    UChar32 finalCharacter;
    if (direction == TextDirection::LTR) {
        finalCharacter = stringView[stringView.length() - 1];
        if (U16_IS_TRAIL(finalCharacter) && stringView.length() > 1 && U16_IS_LEAD(stringView[stringView.length() - 2]))
            finalCharacter = U16_GET_SUPPLEMENTARY(stringView[stringView.length() - 2], finalCharacter);
    } else {
        finalCharacter = stringView[0];
        if (U16_IS_LEAD(finalCharacter) && stringView.length() > 1 && U16_IS_TRAIL(stringView[1]))
            finalCharacter = U16_GET_SUPPLEMENTARY(finalCharacter, stringView[1]);
    }

    return treatAsSpace(finalCharacter) || (canExpandAroundIdeographsInComplexText() && isCJKIdeographOrSymbol(finalCharacter));
}

bool FontCascade::canReceiveTextEmphasis(UChar32 c)
{
    if (U_GET_GC_MASK(c) & (U_GC_Z_MASK | U_GC_CN_MASK | U_GC_CC_MASK | U_GC_CF_MASK))
        return false;

    // Additional word-separator characters listed in CSS Text Level 3 Editor's Draft 3 November 2010.
    if (c == ethiopicWordspace || c == aegeanWordSeparatorLine || c == aegeanWordSeparatorDot
        || c == ugariticWordDivider || c == tibetanMarkIntersyllabicTsheg || c == tibetanMarkDelimiterTshegBstar)
        return false;

    return true;
}

bool FontCascade::isLoadingCustomFonts() const
{
    return m_fonts && m_fonts->isLoadingCustomFonts();
}

enum class GlyphUnderlineType : uint8_t {
    SkipDescenders,
    SkipGlyph,
    DrawOverGlyph
};
    
static GlyphUnderlineType computeUnderlineType(const TextRun& textRun, const GlyphBuffer& glyphBuffer, unsigned index)
{
    // In general, we want to skip descenders. However, skipping descenders on CJK characters leads to undesirable renderings,
    // so we want to draw through CJK characters (on a character-by-character basis).
    // FIXME: The CSS spec says this should instead be done by the user-agent stylesheet using the lang= attribute.
    UChar32 baseCharacter;
    unsigned offsetInString = glyphBuffer.offsetInString(index);

    if (offsetInString == GlyphBuffer::noOffset || offsetInString >= textRun.length()) {
        // We have no idea which character spawned this glyph. Bail.
        ASSERT_WITH_SECURITY_IMPLICATION(offsetInString < textRun.length());
        return GlyphUnderlineType::DrawOverGlyph;
    }
    
    if (textRun.is8Bit())
        baseCharacter = textRun.characters8()[offsetInString];
    else
        U16_NEXT(textRun.characters16(), offsetInString, textRun.length(), baseCharacter);
    
    // u_getIntPropertyValue with UCHAR_IDEOGRAPHIC doesn't return true for Japanese or Korean codepoints.
    // Instead, we can use the "Unicode allocation block" for the character.
    UBlockCode blockCode = ublock_getCode(baseCharacter);
    switch (blockCode) {
    case UBLOCK_CJK_RADICALS_SUPPLEMENT:
    case UBLOCK_CJK_SYMBOLS_AND_PUNCTUATION:
    case UBLOCK_ENCLOSED_CJK_LETTERS_AND_MONTHS:
    case UBLOCK_CJK_COMPATIBILITY:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_A:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS:
    case UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS:
    case UBLOCK_CJK_COMPATIBILITY_FORMS:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_B:
    case UBLOCK_CJK_COMPATIBILITY_IDEOGRAPHS_SUPPLEMENT:
    case UBLOCK_CJK_STROKES:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_C:
    case UBLOCK_CJK_UNIFIED_IDEOGRAPHS_EXTENSION_D:
    case UBLOCK_IDEOGRAPHIC_DESCRIPTION_CHARACTERS:
    case UBLOCK_LINEAR_B_IDEOGRAMS:
    case UBLOCK_ENCLOSED_IDEOGRAPHIC_SUPPLEMENT:
    case UBLOCK_HIRAGANA:
    case UBLOCK_KATAKANA:
    case UBLOCK_BOPOMOFO:
    case UBLOCK_BOPOMOFO_EXTENDED:
    case UBLOCK_HANGUL_JAMO:
    case UBLOCK_HANGUL_COMPATIBILITY_JAMO:
    case UBLOCK_HANGUL_SYLLABLES:
    case UBLOCK_HANGUL_JAMO_EXTENDED_A:
    case UBLOCK_HANGUL_JAMO_EXTENDED_B:
        return GlyphUnderlineType::DrawOverGlyph;
    default:
        return GlyphUnderlineType::SkipDescenders;
    }
}

// FIXME: This function may not work if the emphasis mark uses a complex script, but none of the
// standard emphasis marks do so.
std::optional<GlyphData> FontCascade::getEmphasisMarkGlyphData(const AtomicString& mark) const
{
    if (mark.isEmpty())
        return std::nullopt;

    UChar32 character;
    if (!mark.is8Bit()) {
        SurrogatePairAwareTextIterator iterator(mark.characters16(), 0, mark.length(), mark.length());
        unsigned clusterLength;
        if (!iterator.consume(character, clusterLength))
            return std::nullopt;
    } else
        character = mark[0];

    std::optional<GlyphData> glyphData(glyphDataForCharacter(character, false, EmphasisMarkVariant));
    return glyphData.value().isValid() ? glyphData : std::nullopt;
}

int FontCascade::emphasisMarkAscent(const AtomicString& mark) const
{
    std::optional<GlyphData> markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return 0;

    const Font* markFontData = markGlyphData.value().font;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().ascent();
}

int FontCascade::emphasisMarkDescent(const AtomicString& mark) const
{
    std::optional<GlyphData> markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return 0;

    const Font* markFontData = markGlyphData.value().font;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().descent();
}

int FontCascade::emphasisMarkHeight(const AtomicString& mark) const
{
    std::optional<GlyphData> markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return 0;

    const Font* markFontData = markGlyphData.value().font;
    ASSERT(markFontData);
    if (!markFontData)
        return 0;

    return markFontData->fontMetrics().height();
}

float FontCascade::getGlyphsAndAdvancesForSimpleText(const TextRun& run, unsigned from, unsigned to, GlyphBuffer& glyphBuffer, ForTextEmphasisOrNot forTextEmphasis) const
{
    float initialAdvance;

    WidthIterator it(this, run, 0, false, forTextEmphasis);
    // FIXME: Using separate glyph buffers for the prefix and the suffix is incorrect when kerning or
    // ligatures are enabled.
    GlyphBuffer localGlyphBuffer;
    it.advance(from, &localGlyphBuffer);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to, &glyphBuffer);

    if (glyphBuffer.isEmpty())
        return 0;

    float afterWidth = it.m_runWidthSoFar;

    if (run.rtl()) {
        float finalRoundingWidth = it.m_finalRoundingWidth;
        it.advance(run.length(), &localGlyphBuffer);
        initialAdvance = finalRoundingWidth + it.m_runWidthSoFar - afterWidth;
    } else
        initialAdvance = beforeWidth;

    if (run.rtl())
        glyphBuffer.reverse(0, glyphBuffer.size());

    return initialAdvance;
}

#if !PLATFORM(WIN)
float FontCascade::getGlyphsAndAdvancesForComplexText(const TextRun& run, unsigned from, unsigned to, GlyphBuffer& glyphBuffer, ForTextEmphasisOrNot forTextEmphasis) const
{
    float initialAdvance;

    ComplexTextController controller(*this, run, false, 0, forTextEmphasis);
    GlyphBuffer dummyGlyphBuffer;
    controller.advance(from, &dummyGlyphBuffer);
    controller.advance(to, &glyphBuffer);

    if (glyphBuffer.isEmpty())
        return 0;

    if (run.rtl()) {
        // Exploit the fact that the sum of the paint advances is equal to
        // the sum of the layout advances.
        initialAdvance = controller.totalWidth();
        for (unsigned i = 0; i < dummyGlyphBuffer.size(); ++i)
            initialAdvance -= dummyGlyphBuffer.advanceAt(i).width();
        for (unsigned i = 0; i < glyphBuffer.size(); ++i)
            initialAdvance -= glyphBuffer.advanceAt(i).width();
        glyphBuffer.reverse(0, glyphBuffer.size());
    } else {
        initialAdvance = dummyGlyphBuffer.initialAdvance().width();
        for (unsigned i = 0; i < dummyGlyphBuffer.size(); ++i)
            initialAdvance += dummyGlyphBuffer.advanceAt(i).width();
    }

    return initialAdvance;
}
#endif

void FontCascade::drawEmphasisMarksForSimpleText(GraphicsContext& context, const TextRun& run, const AtomicString& mark, const FloatPoint& point, unsigned from, unsigned to) const
{
    GlyphBuffer glyphBuffer;
    float initialAdvance = getGlyphsAndAdvancesForSimpleText(run, from, to, glyphBuffer, ForTextEmphasis);

    if (glyphBuffer.isEmpty())
        return;

    drawEmphasisMarks(context, glyphBuffer, mark, FloatPoint(point.x() + initialAdvance, point.y()));
}

inline bool shouldDrawIfLoading(const Font& font, FontCascade::CustomFontNotReadyAction customFontNotReadyAction)
{
    // Don't draw anything while we are using custom fonts that are in the process of loading,
    // except if the 'customFontNotReadyAction' argument is set to UseFallbackIfFontNotReady
    // (in which case "font" will be a fallback font).
    return !font.isInterstitial() || font.visibility() == Font::Visibility::Visible || customFontNotReadyAction == FontCascade::CustomFontNotReadyAction::UseFallbackIfFontNotReady;
}

void FontCascade::drawGlyphBuffer(GraphicsContext& context, const GlyphBuffer& glyphBuffer, FloatPoint& point, CustomFontNotReadyAction customFontNotReadyAction) const
{
    // Draw each contiguous run of glyphs that use the same font data.
    const Font* fontData = glyphBuffer.fontAt(0);
#if PLATFORM(WIN)
    FloatPoint startPoint(point.x() + glyphBuffer.initialAdvance().width(), point.y() + glyphBuffer.initialAdvance().height());
#else
    // FIXME: Why do we subtract the initial advance's height but not its width???
    // We should use the line above from Windows instead.
    FloatPoint startPoint(point.x(), point.y() - glyphBuffer.initialAdvance().height());
#endif
    float nextX = startPoint.x() + glyphBuffer.advanceAt(0).width();
    float nextY = startPoint.y() + glyphBuffer.advanceAt(0).height();
    unsigned lastFrom = 0;
    unsigned nextGlyph = 1;
    while (nextGlyph < glyphBuffer.size()) {
        const Font* nextFontData = glyphBuffer.fontAt(nextGlyph);

        if (nextFontData != fontData) {
            if (shouldDrawIfLoading(*fontData, customFontNotReadyAction))
                context.drawGlyphs(*fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint, m_fontDescription.fontSmoothing());

            lastFrom = nextGlyph;
            fontData = nextFontData;
            startPoint.setX(nextX);
            startPoint.setY(nextY);
        }
        nextX += glyphBuffer.advanceAt(nextGlyph).width();
        nextY += glyphBuffer.advanceAt(nextGlyph).height();
        nextGlyph++;
    }

    if (shouldDrawIfLoading(*fontData, customFontNotReadyAction))
        context.drawGlyphs(*fontData, glyphBuffer, lastFrom, nextGlyph - lastFrom, startPoint, m_fontDescription.fontSmoothing());
    point.setX(nextX);
}

inline static float offsetToMiddleOfGlyph(const Font* fontData, Glyph glyph)
{
    if (fontData->platformData().orientation() == FontOrientation::Horizontal) {
        FloatRect bounds = fontData->boundsForGlyph(glyph);
        return bounds.x() + bounds.width() / 2;
    }
    // FIXME: Use glyph bounds once they make sense for vertical fonts.
    return fontData->widthForGlyph(glyph) / 2;
}

inline static float offsetToMiddleOfGlyphAtIndex(const GlyphBuffer& glyphBuffer, unsigned i)
{
    return offsetToMiddleOfGlyph(glyphBuffer.fontAt(i), glyphBuffer.glyphAt(i));
}

void FontCascade::drawEmphasisMarks(GraphicsContext& context, const GlyphBuffer& glyphBuffer, const AtomicString& mark, const FloatPoint& point) const
{
    std::optional<GlyphData> markGlyphData = getEmphasisMarkGlyphData(mark);
    if (!markGlyphData)
        return;

    const Font* markFontData = markGlyphData.value().font;
    ASSERT(markFontData);
    if (!markFontData)
        return;

    Glyph markGlyph = markGlyphData.value().glyph;
    Glyph spaceGlyph = markFontData->spaceGlyph();

    float middleOfLastGlyph = offsetToMiddleOfGlyphAtIndex(glyphBuffer, 0);
    FloatPoint startPoint(point.x() + middleOfLastGlyph - offsetToMiddleOfGlyph(markFontData, markGlyph), point.y());

    GlyphBuffer markBuffer;
    for (unsigned i = 0; i + 1 < glyphBuffer.size(); ++i) {
        float middleOfNextGlyph = offsetToMiddleOfGlyphAtIndex(glyphBuffer, i + 1);
        float advance = glyphBuffer.advanceAt(i).width() - middleOfLastGlyph + middleOfNextGlyph;
        markBuffer.add(glyphBuffer.glyphAt(i) ? markGlyph : spaceGlyph, markFontData, advance);
        middleOfLastGlyph = middleOfNextGlyph;
    }
    markBuffer.add(glyphBuffer.glyphAt(glyphBuffer.size() - 1) ? markGlyph : spaceGlyph, markFontData, 0);

    drawGlyphBuffer(context, markBuffer, startPoint, CustomFontNotReadyAction::DoNotPaintIfFontNotReady);
}

float FontCascade::floatWidthForSimpleText(const TextRun& run, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    WidthIterator it(this, run, fallbackFonts, glyphOverflow);
    GlyphBuffer glyphBuffer;
    it.advance(run.length(), (enableKerning() || requiresShaping()) ? &glyphBuffer : nullptr);

    if (glyphOverflow) {
        glyphOverflow->top = std::max<int>(glyphOverflow->top, ceilf(-it.minGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().ascent()));
        glyphOverflow->bottom = std::max<int>(glyphOverflow->bottom, ceilf(it.maxGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().descent()));
        glyphOverflow->left = ceilf(it.firstGlyphOverflow());
        glyphOverflow->right = ceilf(it.lastGlyphOverflow());
    }

    return it.m_runWidthSoFar;
}

#if !PLATFORM(WIN)
float FontCascade::floatWidthForComplexText(const TextRun& run, HashSet<const Font*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    ComplexTextController controller(*this, run, true, fallbackFonts);
    if (glyphOverflow) {
        glyphOverflow->top = std::max<int>(glyphOverflow->top, ceilf(-controller.minGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().ascent()));
        glyphOverflow->bottom = std::max<int>(glyphOverflow->bottom, ceilf(controller.maxGlyphBoundingBoxY()) - (glyphOverflow->computeBounds ? 0 : fontMetrics().descent()));
        glyphOverflow->left = std::max<int>(0, ceilf(-controller.minGlyphBoundingBoxX()));
        glyphOverflow->right = std::max<int>(0, ceilf(controller.maxGlyphBoundingBoxX() - controller.totalWidth()));
    }
    return controller.totalWidth();
}
#endif

void FontCascade::adjustSelectionRectForSimpleText(const TextRun& run, LayoutRect& selectionRect, unsigned from, unsigned to) const
{
    GlyphBuffer glyphBuffer;
    WidthIterator it(this, run);
    it.advance(from, &glyphBuffer);
    float beforeWidth = it.m_runWidthSoFar;
    it.advance(to, &glyphBuffer);
    float afterWidth = it.m_runWidthSoFar;
    float totalWidth = -1;

    if (run.rtl()) {
        it.advance(run.length(), &glyphBuffer);
        totalWidth = it.m_runWidthSoFar;
        selectionRect.move(totalWidth - afterWidth, 0);
    } else
        selectionRect.move(beforeWidth, 0);
    selectionRect.setWidth(LayoutUnit::fromFloatCeil(afterWidth - beforeWidth));
}

#if !PLATFORM(WIN)
void FontCascade::adjustSelectionRectForComplexText(const TextRun& run, LayoutRect& selectionRect, unsigned from, unsigned to) const
{
    ComplexTextController controller(*this, run);
    controller.advance(from);
    float beforeWidth = controller.runWidthSoFar();
    controller.advance(to);
    float afterWidth = controller.runWidthSoFar();

    if (run.rtl())
        selectionRect.move(controller.totalWidth() - afterWidth, 0);
    else
        selectionRect.move(beforeWidth, 0);
    selectionRect.setWidth(LayoutUnit::fromFloatCeil(afterWidth - beforeWidth));
}
#endif

int FontCascade::offsetForPositionForSimpleText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    float delta = x;

    WidthIterator it(this, run);
    GlyphBuffer localGlyphBuffer;
    unsigned offset;
    if (run.rtl()) {
        delta -= floatWidthForSimpleText(run);
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, localGlyphBuffer))
                break;
            delta += w;
            if (includePartialGlyphs) {
                if (delta - w / 2 >= 0)
                    break;
            } else {
                if (delta >= 0)
                    break;
            }
        }
    } else {
        while (1) {
            offset = it.m_currentCharacter;
            float w;
            if (!it.advanceOneCharacter(w, localGlyphBuffer))
                break;
            delta -= w;
            if (includePartialGlyphs) {
                if (delta + w / 2 <= 0)
                    break;
            } else {
                if (delta <= 0)
                    break;
            }
        }
    }

    return offset;
}

#if !PLATFORM(WIN)
int FontCascade::offsetForPositionForComplexText(const TextRun& run, float x, bool includePartialGlyphs) const
{
    ComplexTextController controller(*this, run);
    return controller.offsetForPosition(x, includePartialGlyphs);
}
#endif

#if !PLATFORM(COCOA) && !USE(HARFBUZZ)
// FIXME: Unify this with the macOS and iOS implementation.
const Font* FontCascade::fontForCombiningCharacterSequence(const UChar* characters, size_t length) const
{
    UChar32 baseCharacter;
    size_t baseCharacterLength = 0;
    U16_NEXT(characters, baseCharacterLength, length, baseCharacter);
    GlyphData baseCharacterGlyphData = glyphDataForCharacter(baseCharacter, false, NormalVariant);

    if (!baseCharacterGlyphData.glyph)
        return nullptr;
    return baseCharacterGlyphData.font;
}
#endif

void FontCascade::drawEmphasisMarksForComplexText(GraphicsContext& context, const TextRun& run, const AtomicString& mark, const FloatPoint& point, unsigned from, unsigned to) const
{
    GlyphBuffer glyphBuffer;
    float initialAdvance = getGlyphsAndAdvancesForComplexText(run, from, to, glyphBuffer, ForTextEmphasis);

    if (glyphBuffer.isEmpty())
        return;

    drawEmphasisMarks(context, glyphBuffer, mark, FloatPoint(point.x() + initialAdvance, point.y()));
}

struct GlyphIterationState {
    FloatPoint startingPoint;
    FloatPoint currentPoint;
    float y1;
    float y2;
    float minX;
    float maxX;
};

static std::optional<float> findIntersectionPoint(float y, FloatPoint p1, FloatPoint p2)
{
    if ((p1.y() < y && p2.y() > y) || (p1.y() > y && p2.y() < y))
        return p1.x() + (y - p1.y()) * (p2.x() - p1.x()) / (p2.y() - p1.y());
    return std::nullopt;
}

static void updateX(GlyphIterationState& state, float x)
{
    state.minX = std::min(state.minX, x);
    state.maxX = std::max(state.maxX, x);
}

// This function is called by CGPathApply and is therefore invoked for each
// contour in a glyph. This function models each contours as a straight line
// and calculates the intersections between each pseudo-contour and
// two horizontal lines (the upper and lower bounds of an underline) found in
// GlyphIterationState::y1 and GlyphIterationState::y2. It keeps track of the
// leftmost and rightmost intersection in GlyphIterationState::minX and
// GlyphIterationState::maxX.
static void findPathIntersections(GlyphIterationState& state, const PathElement& element)
{
    bool doIntersection = false;
    FloatPoint point = FloatPoint();
    switch (element.type) {
    case PathElementMoveToPoint:
        state.startingPoint = element.points[0];
        state.currentPoint = element.points[0];
        break;
    case PathElementAddLineToPoint:
        doIntersection = true;
        point = element.points[0];
        break;
    case PathElementAddQuadCurveToPoint:
        doIntersection = true;
        point = element.points[1];
        break;
    case PathElementAddCurveToPoint:
        doIntersection = true;
        point = element.points[2];
        break;
    case PathElementCloseSubpath:
        doIntersection = true;
        point = state.startingPoint;
        break;
    }
    if (!doIntersection)
        return;
    if (auto intersectionPoint = findIntersectionPoint(state.y1, state.currentPoint, point))
        updateX(state, *intersectionPoint);
    if (auto intersectionPoint = findIntersectionPoint(state.y2, state.currentPoint, point))
        updateX(state, *intersectionPoint);
    if ((state.currentPoint.y() >= state.y1 && state.currentPoint.y() <= state.y2)
        || (state.currentPoint.y() <= state.y1 && state.currentPoint.y() >= state.y2))
        updateX(state, state.currentPoint.x());
    state.currentPoint = point;
}

class GlyphToPathTranslator {
public:
    GlyphToPathTranslator(const TextRun& textRun, const GlyphBuffer& glyphBuffer, const FloatPoint& textOrigin)
        : m_index(0)
        , m_textRun(textRun)
        , m_glyphBuffer(glyphBuffer)
        , m_fontData(glyphBuffer.fontAt(m_index))
        , m_translation(AffineTransform::translation(textOrigin.x(), textOrigin.y()).scale(1, -1))
    {
    }

    bool containsMorePaths() { return m_index != m_glyphBuffer.size(); }
    Path path();
    std::pair<float, float> extents();
    GlyphUnderlineType underlineType();
    void advance();

private:
    unsigned m_index;
    const TextRun& m_textRun;
    const GlyphBuffer& m_glyphBuffer;
    const Font* m_fontData;
    AffineTransform m_translation;
};

Path GlyphToPathTranslator::path()
{
    Path path = m_fontData->pathForGlyph(m_glyphBuffer.glyphAt(m_index));
    path.transform(m_translation);
    return path;
}

std::pair<float, float> GlyphToPathTranslator::extents()
{
    auto beginning = m_translation.mapPoint(FloatPoint(0, 0));
    auto advance = m_glyphBuffer.advanceAt(m_index);
    auto end = m_translation.mapSize(FloatSize(advance.width(), advance.height()));
    return std::make_pair(beginning.x(), beginning.x() + end.width());
}

auto GlyphToPathTranslator::underlineType() -> GlyphUnderlineType
{
    return computeUnderlineType(m_textRun, m_glyphBuffer, m_index);
}

void GlyphToPathTranslator::advance()
{
    GlyphBufferAdvance advance = m_glyphBuffer.advanceAt(m_index);
    m_translation.translate(FloatSize(advance.width(), advance.height()));
    ++m_index;
    if (m_index < m_glyphBuffer.size())
        m_fontData = m_glyphBuffer.fontAt(m_index);
}

DashArray FontCascade::dashesForIntersectionsWithRect(const TextRun& run, const FloatPoint& textOrigin, const FloatRect& lineExtents) const
{
    if (isLoadingCustomFonts())
        return DashArray();

    GlyphBuffer glyphBuffer;
    glyphBuffer.saveOffsetsInString();
    float deltaX;
    if (codePath(run) != FontCascade::Complex)
        deltaX = getGlyphsAndAdvancesForSimpleText(run, 0, run.length(), glyphBuffer);
    else
        deltaX = getGlyphsAndAdvancesForComplexText(run, 0, run.length(), glyphBuffer);

    if (!glyphBuffer.size())
        return DashArray();

    FloatPoint origin = FloatPoint(textOrigin.x() + deltaX, textOrigin.y());
    GlyphToPathTranslator translator(run, glyphBuffer, origin);
    DashArray result;
    for (unsigned index = 0; translator.containsMorePaths(); ++index, translator.advance()) {
        GlyphIterationState info = { FloatPoint(0, 0), FloatPoint(0, 0), lineExtents.y(), lineExtents.y() + lineExtents.height(), lineExtents.x() + lineExtents.width(), lineExtents.x() };
        const Font* localFont = glyphBuffer.fontAt(index);
        if (!localFont) {
            // The advances will get all messed up if we do anything other than bail here.
            result.clear();
            break;
        }
        switch (translator.underlineType()) {
        case GlyphUnderlineType::SkipDescenders: {
            Path path = translator.path();
            path.apply([&](const PathElement& element) {
                findPathIntersections(info, element);
            });
            if (info.minX < info.maxX) {
                result.append(info.minX - lineExtents.x());
                result.append(info.maxX - lineExtents.x());
            }
            break;
        }
        case GlyphUnderlineType::SkipGlyph: {
            std::pair<float, float> extents = translator.extents();
            result.append(extents.first - lineExtents.x());
            result.append(extents.second - lineExtents.x());
            break;
        }
        case GlyphUnderlineType::DrawOverGlyph:
            // Nothing to do
            break;
        }
    }
    return result;
}

}
