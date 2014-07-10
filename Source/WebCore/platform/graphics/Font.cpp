/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2010, 2011 Apple Inc. All rights reserved.
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
#include "Font.h"

#include "FloatRect.h"
#include "FontCache.h"
#include "GlyphBuffer.h"
#include "LayoutRect.h"
#include "TextRun.h"
#include "WidthIterator.h"
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/StringBuilder.h>

using namespace WTF;
using namespace Unicode;

namespace WTF {

// allow compilation of OwnPtr<TextLayout> in source files that don't have access to the TextLayout class definition
template <> void deleteOwnedPtr<WebCore::TextLayout>(WebCore::TextLayout* ptr)
{
    WebCore::Font::deleteLayout(ptr);
}

}

namespace WebCore {

static PassRef<FontGlyphs> retrieveOrAddCachedFontGlyphs(const FontDescription&, PassRefPtr<FontSelector>);

const uint8_t Font::s_roundingHackCharacterTable[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*\t*/, 1 /*\n*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*-*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 /*?*/,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1 /*no-break space*/, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static bool isDrawnWithSVGFont(const TextRun& run)
{
    return run.renderingContext();
}

static bool useBackslashAsYenSignForFamily(const AtomicString& family)
{
    if (family.isEmpty())
        return false;
    static HashSet<AtomicString>* set;
    if (!set) {
        set = new HashSet<AtomicString>;
        set->add("MS PGothic");
        UChar unicodeNameMSPGothic[] = {0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x30B4, 0x30B7, 0x30C3, 0x30AF};
        set->add(AtomicString(unicodeNameMSPGothic, WTF_ARRAY_LENGTH(unicodeNameMSPGothic)));

        set->add("MS PMincho");
        UChar unicodeNameMSPMincho[] = {0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x660E, 0x671D};
        set->add(AtomicString(unicodeNameMSPMincho, WTF_ARRAY_LENGTH(unicodeNameMSPMincho)));

        set->add("MS Gothic");
        UChar unicodeNameMSGothic[] = {0xFF2D, 0xFF33, 0x0020, 0x30B4, 0x30B7, 0x30C3, 0x30AF};
        set->add(AtomicString(unicodeNameMSGothic, WTF_ARRAY_LENGTH(unicodeNameMSGothic)));

        set->add("MS Mincho");
        UChar unicodeNameMSMincho[] = {0xFF2D, 0xFF33, 0x0020, 0x660E, 0x671D};
        set->add(AtomicString(unicodeNameMSMincho, WTF_ARRAY_LENGTH(unicodeNameMSMincho)));

        set->add("Meiryo");
        UChar unicodeNameMeiryo[] = {0x30E1, 0x30A4, 0x30EA, 0x30AA};
        set->add(AtomicString(unicodeNameMeiryo, WTF_ARRAY_LENGTH(unicodeNameMeiryo)));
    }
    return set->contains(family);
}

Font::CodePath Font::s_codePath = Auto;

TypesettingFeatures Font::s_defaultTypesettingFeatures = 0;

// ============================================================================================
// Font Implementation (Cross-Platform Portion)
// ============================================================================================

Font::Font()
    : m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_useBackslashAsYenSymbol(false)
    , m_typesettingFeatures(0)
{
}

Font::Font(const FontDescription& fd, float letterSpacing, float wordSpacing)
    : m_fontDescription(fd)
    , m_letterSpacing(letterSpacing)
    , m_wordSpacing(wordSpacing)
    , m_useBackslashAsYenSymbol(useBackslashAsYenSignForFamily(fd.firstFamily()))
    , m_typesettingFeatures(computeTypesettingFeatures())
{
}

// FIXME: We should make this constructor platform-independent.
Font::Font(const FontPlatformData& fontData, bool isPrinterFont, FontSmoothingMode fontSmoothingMode)
    : m_glyphs(FontGlyphs::createForPlatformFont(fontData))
    , m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_useBackslashAsYenSymbol(false)
    , m_typesettingFeatures(computeTypesettingFeatures())
{
    m_fontDescription.setUsePrinterFont(isPrinterFont);
    m_fontDescription.setFontSmoothing(fontSmoothingMode);
#if PLATFORM(IOS)
    m_fontDescription.setSpecifiedSize(CTFontGetSize(fontData.font()));
    m_fontDescription.setComputedSize(CTFontGetSize(fontData.font()));
    m_fontDescription.setItalic(CTFontGetSymbolicTraits(fontData.font()) & kCTFontTraitItalic);
    m_fontDescription.setWeight((CTFontGetSymbolicTraits(fontData.font()) & kCTFontTraitBold) ? FontWeightBold : FontWeightNormal);
#endif
}

// FIXME: We should make this constructor platform-independent.
#if PLATFORM(IOS)
Font::Font(const FontPlatformData& fontData, PassRefPtr<FontSelector> fontSelector)
    : m_glyphs(FontGlyphs::createForPlatformFont(fontData))
    , m_letterSpacing(0)
    , m_wordSpacing(0)
    , m_typesettingFeatures(computeTypesettingFeatures())
{
    CTFontRef primaryFont = fontData.font();
    m_fontDescription.setSpecifiedSize(CTFontGetSize(primaryFont));
    m_fontDescription.setComputedSize(CTFontGetSize(primaryFont));
    m_fontDescription.setItalic(CTFontGetSymbolicTraits(primaryFont) & kCTFontTraitItalic);
    m_fontDescription.setWeight((CTFontGetSymbolicTraits(primaryFont) & kCTFontTraitBold) ? FontWeightBold : FontWeightNormal);
    m_fontDescription.setUsePrinterFont(fontData.isPrinterFont());
    m_glyphs = retrieveOrAddCachedFontGlyphs(m_fontDescription, fontSelector.get());
}
#endif

Font::Font(const Font& other)
    : m_fontDescription(other.m_fontDescription)
    , m_glyphs(other.m_glyphs)
    , m_letterSpacing(other.m_letterSpacing)
    , m_wordSpacing(other.m_wordSpacing)
    , m_useBackslashAsYenSymbol(other.m_useBackslashAsYenSymbol)
    , m_typesettingFeatures(computeTypesettingFeatures())
{
}

Font& Font::operator=(const Font& other)
{
    m_fontDescription = other.m_fontDescription;
    m_glyphs = other.m_glyphs;
    m_letterSpacing = other.m_letterSpacing;
    m_wordSpacing = other.m_wordSpacing;
    m_useBackslashAsYenSymbol = other.m_useBackslashAsYenSymbol;
    m_typesettingFeatures = other.m_typesettingFeatures;
    return *this;
}

bool Font::operator==(const Font& other) const
{
    // Our FontData don't have to be checked, since checking the font description will be fine.
    // FIXME: This does not work if the font was made with the FontPlatformData constructor.
    if (loadingCustomFonts() || other.loadingCustomFonts())
        return false;

    if (m_fontDescription != other.m_fontDescription || m_letterSpacing != other.m_letterSpacing || m_wordSpacing != other.m_wordSpacing)
        return false;
    if (m_glyphs == other.m_glyphs)
        return true;
    if (!m_glyphs || !other.m_glyphs)
        return false;
    if (m_glyphs->fontSelector() != other.m_glyphs->fontSelector())
        return false;
    // Can these cases actually somehow occur? All fonts should get wiped out by full style recalc.
    if (m_glyphs->fontSelectorVersion() != other.m_glyphs->fontSelectorVersion())
        return false;
    if (m_glyphs->generation() != other.m_glyphs->generation())
        return false;
    return true;
}

struct FontGlyphsCacheKey {
    // This part of the key is shared with the lower level FontCache (caching FontData objects).
    FontDescriptionFontDataCacheKey fontDescriptionCacheKey;
    Vector<AtomicString, 3> families;
    unsigned fontSelectorId;
    unsigned fontSelectorVersion;
    unsigned fontSelectorFlags;
};

struct FontGlyphsCacheEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontGlyphsCacheEntry(FontGlyphsCacheKey&& k, PassRef<FontGlyphs> g) : key(WTF::move(k)), glyphs(WTF::move(g)) { }
    FontGlyphsCacheKey key;
    Ref<FontGlyphs> glyphs;
};

typedef HashMap<unsigned, OwnPtr<FontGlyphsCacheEntry>, AlreadyHashed> FontGlyphsCache;

static bool operator==(const FontGlyphsCacheKey& a, const FontGlyphsCacheKey& b)
{
    if (a.fontDescriptionCacheKey != b.fontDescriptionCacheKey)
        return false;
    if (a.fontSelectorId != b.fontSelectorId || a.fontSelectorVersion != b.fontSelectorVersion || a.fontSelectorFlags != b.fontSelectorFlags)
        return false;
    if (a.families.size() != b.families.size())
        return false;
    for (unsigned i = 0; i < a.families.size(); ++i) {
        if (!equalIgnoringCase(a.families[i].impl(), b.families[i].impl()))
            return false;
    }
    return true;
}

static FontGlyphsCache& fontGlyphsCache()
{
    DEPRECATED_DEFINE_STATIC_LOCAL(FontGlyphsCache, cache, ());
    return cache;
}

void invalidateFontGlyphsCache()
{
    fontGlyphsCache().clear();
}

void clearWidthCaches()
{
    for (auto it = fontGlyphsCache().begin(), end = fontGlyphsCache().end(); it != end; ++it)
        it->value->glyphs.get().widthCache().clear();
}

static unsigned makeFontSelectorFlags(const FontDescription& description)
{
    return static_cast<unsigned>(description.script()) << 1 | static_cast<unsigned>(description.smallCaps());
}

static void makeFontGlyphsCacheKey(FontGlyphsCacheKey& key, const FontDescription& description, FontSelector* fontSelector)
{
    key.fontDescriptionCacheKey = FontDescriptionFontDataCacheKey(description);
    for (unsigned i = 0; i < description.familyCount(); ++i)
        key.families.append(description.familyAt(i));
    key.fontSelectorId = fontSelector ? fontSelector->uniqueId() : 0;
    key.fontSelectorVersion = fontSelector ? fontSelector->version() : 0;
    key.fontSelectorFlags = fontSelector && fontSelector->resolvesFamilyFor(description) ? makeFontSelectorFlags(description) : 0;
}

static unsigned computeFontGlyphsCacheHash(const FontGlyphsCacheKey& key)
{
    Vector<unsigned, 7> hashCodes;
    hashCodes.reserveInitialCapacity(4 + key.families.size());

    hashCodes.uncheckedAppend(key.fontDescriptionCacheKey.computeHash());
    hashCodes.uncheckedAppend(key.fontSelectorId);
    hashCodes.uncheckedAppend(key.fontSelectorVersion);
    hashCodes.uncheckedAppend(key.fontSelectorFlags);
    for (unsigned i = 0; i < key.families.size(); ++i)
        hashCodes.uncheckedAppend(key.families[i].impl() ? CaseFoldingHash::hash(key.families[i]) : 0);

    return StringHasher::hashMemory(hashCodes.data(), hashCodes.size() * sizeof(unsigned));
}

void pruneUnreferencedEntriesFromFontGlyphsCache()
{
    Vector<unsigned, 50> toRemove;
    FontGlyphsCache::iterator end = fontGlyphsCache().end();
    for (FontGlyphsCache::iterator it = fontGlyphsCache().begin(); it != end; ++it) {
        if (it->value->glyphs.get().hasOneRef())
            toRemove.append(it->key);
    }
    for (unsigned i = 0; i < toRemove.size(); ++i)
        fontGlyphsCache().remove(toRemove[i]);
}

static PassRef<FontGlyphs> retrieveOrAddCachedFontGlyphs(const FontDescription& fontDescription, PassRefPtr<FontSelector> fontSelector)
{
    FontGlyphsCacheKey key;
    makeFontGlyphsCacheKey(key, fontDescription, fontSelector.get());

    unsigned hash = computeFontGlyphsCacheHash(key);
    FontGlyphsCache::AddResult addResult = fontGlyphsCache().add(hash, PassOwnPtr<FontGlyphsCacheEntry>());
    if (!addResult.isNewEntry && addResult.iterator->value->key == key)
        return addResult.iterator->value->glyphs.get();

    OwnPtr<FontGlyphsCacheEntry>& newEntry = addResult.iterator->value;
    newEntry = adoptPtr(new FontGlyphsCacheEntry(WTF::move(key), FontGlyphs::create(fontSelector)));
    PassRef<FontGlyphs> glyphs = newEntry->glyphs.get();

    static const unsigned unreferencedPruneInterval = 50;
    static const int maximumEntries = 400;
    static unsigned pruneCounter;
    // Referenced FontGlyphs would exist anyway so pruning them saves little memory.
    if (!(++pruneCounter % unreferencedPruneInterval))
        pruneUnreferencedEntriesFromFontGlyphsCache();
    // Prevent pathological growth.
    if (fontGlyphsCache().size() > maximumEntries)
        fontGlyphsCache().remove(fontGlyphsCache().begin());
    return glyphs;
}

void Font::update(PassRefPtr<FontSelector> fontSelector) const
{
    m_glyphs = retrieveOrAddCachedFontGlyphs(m_fontDescription, fontSelector.get());
    m_useBackslashAsYenSymbol = useBackslashAsYenSignForFamily(firstFamily());
    m_typesettingFeatures = computeTypesettingFeatures();
}

float Font::drawText(GraphicsContext* context, const TextRun& run, const FloatPoint& point, int from, int to, CustomFontNotReadyAction customFontNotReadyAction) const
{
    // Don't draw anything while we are using custom fonts that are in the process of loading,
    // except if the 'force' argument is set to true (in which case it will use a fallback
    // font).
    if (loadingCustomFonts() && customFontNotReadyAction == DoNotPaintIfFontNotReady)
        return 0;

    to = (to == -1 ? run.length() : to);

    CodePath codePathToUse = codePath(run);
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePathToUse != Complex && typesettingFeatures() && (from || to != run.length()) && !isDrawnWithSVGFont(run))
        codePathToUse = Complex;

    if (codePathToUse != Complex)
        return drawSimpleText(context, run, point, from, to);

    return drawComplexText(context, run, point, from, to);
}

void Font::drawEmphasisMarks(GraphicsContext* context, const TextRun& run, const AtomicString& mark, const FloatPoint& point, int from, int to) const
{
    if (loadingCustomFonts())
        return;

    if (to < 0)
        to = run.length();

    CodePath codePathToUse = codePath(run);
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePathToUse != Complex && typesettingFeatures() && (from || to != run.length()) && !isDrawnWithSVGFont(run))
        codePathToUse = Complex;

    if (codePathToUse != Complex)
        drawEmphasisMarksForSimpleText(context, run, mark, point, from, to);
    else
        drawEmphasisMarksForComplexText(context, run, mark, point, from, to);
}

float Font::width(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    CodePath codePathToUse = codePath(run);
    if (codePathToUse != Complex) {
        // The complex path is more restrictive about returning fallback fonts than the simple path, so we need an explicit test to make their behaviors match.
        if (!canReturnFallbackFontsForComplexText())
            fallbackFonts = 0;
        // The simple path can optimize the case where glyph overflow is not observable.
        if (codePathToUse != SimpleWithGlyphOverflow && (glyphOverflow && !glyphOverflow->computeBounds))
            glyphOverflow = 0;
    }

    bool hasKerningOrLigatures = typesettingFeatures() & (Kerning | Ligatures);
    bool hasWordSpacingOrLetterSpacing = wordSpacing() || letterSpacing();
    float* cacheEntry = m_glyphs->widthCache().add(run, std::numeric_limits<float>::quiet_NaN(), hasKerningOrLigatures, hasWordSpacingOrLetterSpacing, glyphOverflow);
    if (cacheEntry && !std::isnan(*cacheEntry))
        return *cacheEntry;

    HashSet<const SimpleFontData*> localFallbackFonts;
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

float Font::width(const TextRun& run, int& charsConsumed, String& glyphName) const
{
#if ENABLE(SVG_FONTS)
    if (isDrawnWithSVGFont(run))
        return run.renderingContext()->floatWidthUsingSVGFont(*this, run, charsConsumed, glyphName);
#endif

    charsConsumed = run.length();
    glyphName = "";
    return width(run);
}

#if !PLATFORM(COCOA)
PassOwnPtr<TextLayout> Font::createLayout(RenderText*, float, bool) const
{
    return nullptr;
}

void Font::deleteLayout(TextLayout*)
{
}

float Font::width(TextLayout&, unsigned, unsigned, HashSet<const SimpleFontData*>*)
{
    ASSERT_NOT_REACHED();
    return 0;
}
#endif



static const char* fontFamiliesWithInvalidCharWidth[] = {
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
};

// For font families where any of the fonts don't have a valid entry in the OS/2 table
// for avgCharWidth, fallback to the legacy webkit behavior of getting the avgCharWidth
// from the width of a '0'. This only seems to apply to a fixed number of Mac fonts,
// but, in order to get similar rendering across platforms, we do this check for
// all platforms.
bool Font::hasValidAverageCharWidth() const
{
    AtomicString family = firstFamily();
    if (family.isEmpty())
        return false;

#if PLATFORM(MAC) || PLATFORM(IOS)
    // Internal fonts on OS X and iOS also have an invalid entry in the table for avgCharWidth.
    if (primaryFontDataIsSystemFont())
        return false;
#endif

    static HashSet<AtomicString>* fontFamiliesWithInvalidCharWidthMap = 0;

    if (!fontFamiliesWithInvalidCharWidthMap) {
        fontFamiliesWithInvalidCharWidthMap = new HashSet<AtomicString>;

        for (size_t i = 0; i < WTF_ARRAY_LENGTH(fontFamiliesWithInvalidCharWidth); ++i)
            fontFamiliesWithInvalidCharWidthMap->add(AtomicString(fontFamiliesWithInvalidCharWidth[i]));
    }

    return !fontFamiliesWithInvalidCharWidthMap->contains(family);
}

bool Font::fastAverageCharWidthIfAvailable(float& width) const
{
    bool success = hasValidAverageCharWidth();
    if (success)
        width = roundf(primaryFont()->avgCharWidth()); // FIXME: primaryFont() might not correspond to firstFamily().
    return success;
}

void Font::adjustSelectionRectForText(const TextRun& run, LayoutRect& selectionRect, int from, int to) const
{
    to = (to == -1 ? run.length() : to);

    CodePath codePathToUse = codePath(run);
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePathToUse != Complex && typesettingFeatures() && (from || to != run.length()) && !isDrawnWithSVGFont(run))
        codePathToUse = Complex;

    if (codePathToUse != Complex)
        return adjustSelectionRectForSimpleText(run, selectionRect, from, to);

    return adjustSelectionRectForComplexText(run, selectionRect, from, to);
}

int Font::offsetForPosition(const TextRun& run, float x, bool includePartialGlyphs) const
{
    // FIXME: Use the fast code path once it handles partial runs with kerning and ligatures. See http://webkit.org/b/100050
    if (codePath(run) != Complex && (!typesettingFeatures() || isDrawnWithSVGFont(run)))
        return offsetForPositionForSimpleText(run, x, includePartialGlyphs);

    return offsetForPositionForComplexText(run, x, includePartialGlyphs);
}

template <typename CharacterType>
static inline String normalizeSpacesInternal(const CharacterType* characters, unsigned length)
{
    StringBuilder normalized;
    normalized.reserveCapacity(length);

    for (unsigned i = 0; i < length; ++i)
        normalized.append(Font::normalizeSpaces(characters[i]));

    return normalized.toString();
}

String Font::normalizeSpaces(const LChar* characters, unsigned length)
{
    return normalizeSpacesInternal(characters, length);
}

String Font::normalizeSpaces(const UChar* characters, unsigned length)
{
    return normalizeSpacesInternal(characters, length);
}

static bool shouldUseFontSmoothing = true;

void Font::setShouldUseSmoothing(bool shouldUseSmoothing)
{
    ASSERT(isMainThread());
    shouldUseFontSmoothing = shouldUseSmoothing;
}

bool Font::shouldUseSmoothing()
{
    return shouldUseFontSmoothing;
}

void Font::setCodePath(CodePath p)
{
    s_codePath = p;
}

Font::CodePath Font::codePath()
{
    return s_codePath;
}

void Font::setDefaultTypesettingFeatures(TypesettingFeatures typesettingFeatures)
{
    s_defaultTypesettingFeatures = typesettingFeatures;
}

TypesettingFeatures Font::defaultTypesettingFeatures()
{
    return s_defaultTypesettingFeatures;
}

Font::CodePath Font::codePath(const TextRun& run) const
{
    if (s_codePath != Auto)
        return s_codePath;

#if ENABLE(SVG_FONTS)
    if (isDrawnWithSVGFont(run))
        return Simple;
#endif

    if (m_fontDescription.featureSettings() && m_fontDescription.featureSettings()->size() > 0)
        return Complex;
    
    if (run.length() > 1 && !WidthIterator::supportsTypesettingFeatures(*this))
        return Complex;

    if (!run.characterScanForCodePath())
        return Simple;

    if (run.is8Bit())
        return Simple;

    // Start from 0 since drawing and highlighting also measure the characters before run->from.
    return characterRangeCodePath(run.characters16(), run.length());
}

Font::CodePath Font::characterRangeCodePath(const UChar* characters, unsigned len)
{
    // FIXME: Should use a UnicodeSet in ports where ICU is used. Note that we 
    // can't simply use UnicodeCharacter Property/class because some characters
    // are not 'combining', but still need to go to the complex path.
    // Alternatively, we may as well consider binary search over a sorted
    // list of ranges.
    CodePath result = Simple;
    for (unsigned i = 0; i < len; i++) {
        const UChar c = characters[i];
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

            if (supplementaryCharacter < 0x1F1E6) // U+1F1E6 through U+1F1FF Regional Indicator Symbols
                continue;
            if (supplementaryCharacter <= 0x1F1FF)
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

bool Font::isCJKIdeograph(UChar32 c)
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

bool Font::isCJKIdeographOrSymbol(UChar32 c)
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

unsigned Font::expansionOpportunityCount(const LChar* characters, size_t length, TextDirection direction, bool& isAfterExpansion)
{
    unsigned count = 0;
    if (direction == LTR) {
        for (size_t i = 0; i < length; ++i) {
            if (treatAsSpace(characters[i])) {
                count++;
                isAfterExpansion = true;
            } else
                isAfterExpansion = false;
        }
    } else {
        for (size_t i = length; i > 0; --i) {
            if (treatAsSpace(characters[i - 1])) {
                count++;
                isAfterExpansion = true;
            } else
                isAfterExpansion = false;
        }
    }
    return count;
}

unsigned Font::expansionOpportunityCount(const UChar* characters, size_t length, TextDirection direction, bool& isAfterExpansion)
{
    static bool expandAroundIdeographs = canExpandAroundIdeographsInComplexText();
    unsigned count = 0;
    if (direction == LTR) {
        for (size_t i = 0; i < length; ++i) {
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
        for (size_t i = length; i > 0; --i) {
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
    return count;
}

bool Font::canReceiveTextEmphasis(UChar32 c)
{
    if (U_GET_GC_MASK(c) & (U_GC_Z_MASK | U_GC_CN_MASK | U_GC_CC_MASK | U_GC_CF_MASK))
        return false;

    // Additional word-separator characters listed in CSS Text Level 3 Editor's Draft 3 November 2010.
    if (c == ethiopicWordspace || c == aegeanWordSeparatorLine || c == aegeanWordSeparatorDot
        || c == ugariticWordDivider || c == tibetanMarkIntersyllabicTsheg || c == tibetanMarkDelimiterTshegBstar)
        return false;

    return true;
}
    
GlyphToPathTranslator::GlyphUnderlineType computeUnderlineType(const TextRun& textRun, const GlyphBuffer& glyphBuffer, int index)
{
    // In general, we want to skip descenders. However, skipping descenders on CJK characters leads to undesirable renderings,
    // so we want to draw through CJK characters (on a character-by-character basis).
    UChar32 baseCharacter;
    int offsetInString = glyphBuffer.offsetInString(index);

    if (offsetInString == GlyphBuffer::kNoOffset) {
        // We have no idea which character spawned this glyph. Bail.
        return GlyphToPathTranslator::GlyphUnderlineType::DrawOverGlyph;
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
        return GlyphToPathTranslator::GlyphUnderlineType::DrawOverGlyph;
    default:
        return GlyphToPathTranslator::GlyphUnderlineType::SkipDescenders;
    }
}

}
