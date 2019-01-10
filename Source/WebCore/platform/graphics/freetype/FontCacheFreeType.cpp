/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Igalia S.L.
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
#include "FontCache.h"

#include "CairoUniquePtr.h"
#include "CairoUtilities.h"
#include "FcUniquePtr.h"
#include "FloatConversion.h"
#include "Font.h"
#include "FontDescription.h"
#include "FontCacheFreeType.h"
#include "RefPtrCairo.h"
#include "RefPtrFontconfig.h"
#include "UTF16UChar32Iterator.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <wtf/Assertions.h>
#include <wtf/text/CString.h>

#if PLATFORM(GTK)
#include "GtkUtilities.h"
#endif

#if ENABLE(VARIATION_FONTS)
#include FT_MULTIPLE_MASTERS_H
#endif

namespace WebCore {

void FontCache::platformInit()
{
    // It's fine to call FcInit multiple times per the documentation.
    if (!FcInit())
        ASSERT_NOT_REACHED();
}

static int fontWeightToFontconfigWeight(FontSelectionValue weight)
{
    if (weight < FontSelectionValue(150))
        return FC_WEIGHT_THIN;
    if (weight < FontSelectionValue(250))
        return FC_WEIGHT_ULTRALIGHT;
    if (weight < FontSelectionValue(350))
        return FC_WEIGHT_LIGHT;
    if (weight < FontSelectionValue(450))
        return FC_WEIGHT_REGULAR;
    if (weight < FontSelectionValue(550))
        return FC_WEIGHT_MEDIUM;
    if (weight < FontSelectionValue(650))
        return FC_WEIGHT_SEMIBOLD;
    if (weight < FontSelectionValue(750))
        return FC_WEIGHT_BOLD;
    if (weight < FontSelectionValue(850))
        return FC_WEIGHT_EXTRABOLD;
    return FC_WEIGHT_ULTRABLACK;
}

static bool configurePatternForFontDescription(FcPattern* pattern, const FontDescription& fontDescription)
{
    if (!FcPatternAddInteger(pattern, FC_SLANT, fontDescription.italic() ? FC_SLANT_ITALIC : FC_SLANT_ROMAN))
        return false;
    if (!FcPatternAddInteger(pattern, FC_WEIGHT, fontWeightToFontconfigWeight(fontDescription.weight())))
        return false;
    if (!FcPatternAddDouble(pattern, FC_PIXEL_SIZE, fontDescription.computedPixelSize()))
        return false;
    return true;
}

static void getFontPropertiesFromPattern(FcPattern* pattern, const FontDescription& fontDescription, bool& fixedWidth, bool& syntheticBold, bool& syntheticOblique)
{
    fixedWidth = false;
    int spacing;
    if (FcPatternGetInteger(pattern, FC_SPACING, 0, &spacing) == FcResultMatch && spacing == FC_MONO)
        fixedWidth = true;

    syntheticBold = false;
    bool descriptionAllowsSyntheticBold = fontDescription.fontSynthesis() & FontSynthesisWeight;
    if (descriptionAllowsSyntheticBold && isFontWeightBold(fontDescription.weight())) {
        // The FC_EMBOLDEN property instructs us to fake the boldness of the font.
        FcBool fontConfigEmbolden = FcFalse;
        if (FcPatternGetBool(pattern, FC_EMBOLDEN, 0, &fontConfigEmbolden) == FcResultMatch)
            syntheticBold = fontConfigEmbolden;

        // Fallback fonts may not have FC_EMBOLDEN activated even though it's necessary.
        int weight = 0;
        if (!syntheticBold && FcPatternGetInteger(pattern, FC_WEIGHT, 0, &weight) == FcResultMatch)
            syntheticBold = syntheticBold || weight < FC_WEIGHT_DEMIBOLD;
    }

    // We requested an italic font, but Fontconfig gave us one that was neither oblique nor italic.
    syntheticOblique = false;
    int actualFontSlant;
    bool descriptionAllowsSyntheticOblique = fontDescription.fontSynthesis() & FontSynthesisStyle;
    if (descriptionAllowsSyntheticOblique && fontDescription.italic()
        && FcPatternGetInteger(pattern, FC_SLANT, 0, &actualFontSlant) == FcResultMatch) {
        syntheticOblique = actualFontSlant == FC_SLANT_ROMAN;
    }
}

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font*, IsForPlatformFont, PreferColoredFont preferColoredFont, const UChar* characters, unsigned length)
{
    FcUniquePtr<FcCharSet> fontConfigCharSet(FcCharSetCreate());
    UTF16UChar32Iterator iterator(characters, length);
    UChar32 character = iterator.next();
    while (character != iterator.end()) {
        FcCharSetAddChar(fontConfigCharSet.get(), character);
        character = iterator.next();
    }

    RefPtr<FcPattern> pattern = adoptRef(FcPatternCreate());
    FcPatternAddCharSet(pattern.get(), FC_CHARSET, fontConfigCharSet.get());

    FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
    if (preferColoredFont == PreferColoredFont::Yes)
        FcPatternAddBool(pattern.get(), FC_COLOR, FcTrue);

    if (!configurePatternForFontDescription(pattern.get(), description))
        return nullptr;

    FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
    cairo_ft_font_options_substitute(getDefaultCairoFontOptions(), pattern.get());
    FcDefaultSubstitute(pattern.get());

    FcResult fontConfigResult;
    RefPtr<FcPattern> resultPattern = adoptRef(FcFontMatch(nullptr, pattern.get(), &fontConfigResult));
    if (!resultPattern)
        return nullptr;

    bool fixedWidth, syntheticBold, syntheticOblique;
    getFontPropertiesFromPattern(resultPattern.get(), description, fixedWidth, syntheticBold, syntheticOblique);

    RefPtr<cairo_font_face_t> fontFace = adoptRef(cairo_ft_font_face_create_for_pattern(resultPattern.get()));
    FontPlatformData alternateFontData(fontFace.get(), resultPattern.get(), description.computedPixelSize(), fixedWidth, syntheticBold, syntheticOblique, description.orientation());
    return fontForPlatformData(alternateFontData);
}

static Vector<String> patternToFamilies(FcPattern& pattern)
{
    char* patternChars = reinterpret_cast<char*>(FcPatternFormat(&pattern, reinterpret_cast<const FcChar8*>("%{family}")));
    String patternString = String::fromUTF8(patternChars);
    free(patternChars);

    return patternString.split(',');
}

Vector<String> FontCache::systemFontFamilies()
{
    RefPtr<FcPattern> scalablesOnlyPattern = adoptRef(FcPatternCreate());
    FcPatternAddBool(scalablesOnlyPattern.get(), FC_SCALABLE, FcTrue);

    FcUniquePtr<FcObjectSet> familiesOnly(FcObjectSetBuild(FC_FAMILY, nullptr));
    FcUniquePtr<FcFontSet> fontSet(FcFontList(nullptr, scalablesOnlyPattern.get(), familiesOnly.get()));

    Vector<String> fontFamilies;
    for (int i = 0; i < fontSet->nfont; i++) {
        FcPattern* pattern = fontSet->fonts[i];
        FcChar8* family = nullptr;
        FcPatternGetString(pattern, FC_FAMILY, 0, &family);
        if (family)
            fontFamilies.appendVector(patternToFamilies(*pattern));
    }

    return fontFamilies;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    // We want to return a fallback font here, otherwise the logic preventing FontConfig
    // matches for non-fallback fonts might return 0. See isFallbackFontAllowed.
    static AtomicString timesStr("serif");
    if (RefPtr<Font> font = fontForFamily(fontDescription, timesStr))
        return *font;

    // This could be reached due to improperly-installed or misconfigured fontconfig.
    RELEASE_ASSERT_NOT_REACHED();
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomicString&, AllowUserInstalledFonts)
{
    return { };
}

static String getFamilyNameStringFromFamily(const AtomicString& family)
{
    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (family.length() && !family.startsWith("-webkit-"))
        return family.string();

    if (family == standardFamily || family == serifFamily)
        return "serif";
    if (family == sansSerifFamily)
        return "sans-serif";
    if (family == monospaceFamily)
        return "monospace";
    if (family == cursiveFamily)
        return "cursive";
    if (family == fantasyFamily)
        return "fantasy";

#if PLATFORM(GTK)
    if (family == systemUiFamily || family == "-webkit-system-font")
        return defaultGtkSystemFont();
#endif

    return "";
}

// This is based on Chromium BSD code from Skia (src/ports/SkFontMgr_fontconfig.cpp). It is a
// hack for lack of API in Fontconfig: https://bugs.freedesktop.org/show_bug.cgi?id=19375
// FIXME: This is horrible. It should be deleted once Fontconfig can do this itself.
enum class AliasStrength {
    Weak,
    Strong,
    Done
};

static AliasStrength strengthOfFirstAlias(const FcPattern& original)
{
    // Ideally there would exist a call like
    // FcResult FcPatternIsWeak(pattern, object, id, FcBool* isWeak);
    //
    // However, there is no such call and as of Fc 2.11.0 even FcPatternEquals ignores the weak bit.
    // Currently, the only reliable way of finding the weak bit is by its effect on matching.
    // The weak bit only affects the matching of FC_FAMILY and FC_POSTSCRIPT_NAME object values.
    // A element with the weak bit is scored after FC_LANG, without the weak bit is scored before.
    // Note that the weak bit is stored on the element, not on the value it holds.
    FcValue value;
    FcResult result = FcPatternGet(&original, FC_FAMILY, 0, &value);
    if (result != FcResultMatch)
        return AliasStrength::Done;

    RefPtr<FcPattern> pattern = adoptRef(FcPatternDuplicate(&original));
    FcBool hasMultipleFamilies = true;
    while (hasMultipleFamilies)
        hasMultipleFamilies = FcPatternRemove(pattern.get(), FC_FAMILY, 1);

    // Create a font set with two patterns.
    // 1. the same FC_FAMILY as pattern and a lang object with only 'nomatchlang'.
    // 2. a different FC_FAMILY from pattern and a lang object with only 'matchlang'.
    FcUniquePtr<FcFontSet> fontSet(FcFontSetCreate());

    FcUniquePtr<FcLangSet> strongLangSet(FcLangSetCreate());
    FcLangSetAdd(strongLangSet.get(), reinterpret_cast<const FcChar8*>("nomatchlang"));
    // Ownership of this FcPattern will be transferred with FcFontSetAdd.
    FcPattern* strong = FcPatternDuplicate(pattern.get());
    FcPatternAddLangSet(strong, FC_LANG, strongLangSet.get());

    FcUniquePtr<FcLangSet> weakLangSet(FcLangSetCreate());
    FcLangSetAdd(weakLangSet.get(), reinterpret_cast<const FcChar8*>("matchlang"));
    // Ownership of this FcPattern will be transferred via FcFontSetAdd.
    FcPattern* weak = FcPatternCreate();
    FcPatternAddString(weak, FC_FAMILY, reinterpret_cast<const FcChar8*>("nomatchstring"));
    FcPatternAddLangSet(weak, FC_LANG, weakLangSet.get());

    FcFontSetAdd(fontSet.get(), strong);
    FcFontSetAdd(fontSet.get(), weak);

    // Add 'matchlang' to the copy of the pattern.
    FcPatternAddLangSet(pattern.get(), FC_LANG, weakLangSet.get());

    // Run a match against the copy of the pattern.
    // If the first element was weak, then we should match the pattern with 'matchlang'.
    // If the first element was strong, then we should match the pattern with 'nomatchlang'.

    // Note that this config is only used for FcFontRenderPrepare, which we don't even want.
    // However, there appears to be no way to match/sort without it.
    RefPtr<FcConfig> config = adoptRef(FcConfigCreate());
    FcFontSet* fontSets[1] = { fontSet.get() };
    RefPtr<FcPattern> match = adoptRef(FcFontSetMatch(config.get(), fontSets, 1, pattern.get(), &result));

    FcLangSet* matchLangSet;
    FcPatternGetLangSet(match.get(), FC_LANG, 0, &matchLangSet);
    return FcLangEqual == FcLangSetHasLang(matchLangSet, reinterpret_cast<const FcChar8*>("matchlang"))
        ? AliasStrength::Weak : AliasStrength::Strong;
}

static Vector<String> strongAliasesForFamily(const String& family)
{
    RefPtr<FcPattern> pattern = adoptRef(FcPatternCreate());
    if (!FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(family.utf8().data())))
        return Vector<String>();

    FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
    cairo_ft_font_options_substitute(getDefaultCairoFontOptions(), pattern.get());
    FcDefaultSubstitute(pattern.get());

    FcUniquePtr<FcObjectSet> familiesOnly(FcObjectSetBuild(FC_FAMILY, nullptr));
    RefPtr<FcPattern> minimal = adoptRef(FcPatternFilter(pattern.get(), familiesOnly.get()));

    // We really want to match strong (preferred) and same (acceptable) only here.
    // If a family name was specified, assume that any weak matches after the last strong match
    // are weak (default) and ignore them.
    // The reason for is that after substitution the pattern for 'sans-serif' looks like
    // "wwwwwwwwwwwwwwswww" where there are many weak but preferred names, followed by defaults.
    // So it is possible to have weakly matching but preferred names.
    // In aliases, bindings are weak by default, so this is easy and common.
    // If no family name was specified, we'll probably only get weak matches, but that's ok.
    int lastStrongId = -1;
    int numIds = 0;
    for (int id = 0; ; ++id) {
        AliasStrength result = strengthOfFirstAlias(*minimal);
        if (result == AliasStrength::Done) {
            numIds = id;
            break;
        }
        if (result == AliasStrength::Strong)
            lastStrongId = id;
        if (!FcPatternRemove(minimal.get(), FC_FAMILY, 0))
            return Vector<String>();
    }

    // If they were all weak, then leave the pattern alone.
    if (lastStrongId < 0)
        return Vector<String>();

    // Remove everything after the last strong.
    for (int id = lastStrongId + 1; id < numIds; ++id) {
        if (!FcPatternRemove(pattern.get(), FC_FAMILY, lastStrongId + 1)) {
            ASSERT_NOT_REACHED();
            return Vector<String>();
        }
    }

    return patternToFamilies(*pattern);
}

static bool areStronglyAliased(const String& familyA, const String& familyB)
{
    for (auto& family : strongAliasesForFamily(familyA)) {
        if (family == familyB)
            return true;
    }
    return false;
}

static inline bool isCommonlyUsedGenericFamily(const String& familyNameString)
{
    return equalLettersIgnoringASCIICase(familyNameString, "sans")
        || equalLettersIgnoringASCIICase(familyNameString, "sans-serif")
        || equalLettersIgnoringASCIICase(familyNameString, "serif")
        || equalLettersIgnoringASCIICase(familyNameString, "monospace")
        || equalLettersIgnoringASCIICase(familyNameString, "fantasy")
#if PLATFORM(GTK)
        || equalLettersIgnoringASCIICase(familyNameString, "-webkit-system-font")
        || equalLettersIgnoringASCIICase(familyNameString, "-webkit-system-ui")
#endif
        || equalLettersIgnoringASCIICase(familyNameString, "cursive");
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family, const FontFeatureSettings*, const FontVariantSettings*, FontSelectionSpecifiedCapabilities)
{
    // The CSS font matching algorithm (http://www.w3.org/TR/css3-fonts/#font-matching-algorithm)
    // says that we must find an exact match for font family, slant (italic or oblique can be used)
    // and font weight (we only match bold/non-bold here).
    RefPtr<FcPattern> pattern = adoptRef(FcPatternCreate());
    // Never choose unscalable fonts, as they pixelate when displayed at different sizes.
    FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
#if ENABLE(VARIATION_FONTS)
    FcPatternAddBool(pattern.get(), FC_VARIABLE, FcDontCare);
#endif
    String familyNameString(getFamilyNameStringFromFamily(family));
    if (!FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(familyNameString.utf8().data())))
        return nullptr;

    if (!configurePatternForFontDescription(pattern.get(), fontDescription))
        return nullptr;

    // The strategy is originally from Skia (src/ports/SkFontHost_fontconfig.cpp):
    //
    // We do not normally allow fontconfig to substitute one font family for another, since this
    // would break CSS font family fallback: the website should be in control of fallback. During
    // normal font matching, the only font family substitution permitted is for generic families
    // (sans, serif, monospace) or for strongly-aliased fonts (which are to be treated as
    // effectively identical). This is because the font matching step is designed to always find a
    // match for the font, which we don't want.
    //
    // Fontconfig is used in two stages: (1) configuration and (2) matching. During the
    // configuration step, before any matching occurs, we allow arbitrary family substitutions,
    // since this is an exact matter of respecting the user's font configuration.
    FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
    cairo_ft_font_options_substitute(getDefaultCairoFontOptions(), pattern.get());
    FcDefaultSubstitute(pattern.get());

    FcChar8* fontConfigFamilyNameAfterConfiguration;
    FcPatternGetString(pattern.get(), FC_FAMILY, 0, &fontConfigFamilyNameAfterConfiguration);
    String familyNameAfterConfiguration = String::fromUTF8(reinterpret_cast<char*>(fontConfigFamilyNameAfterConfiguration));

    FcResult fontConfigResult;
    RefPtr<FcPattern> resultPattern = adoptRef(FcFontMatch(nullptr, pattern.get(), &fontConfigResult));
    if (!resultPattern) // No match.
        return nullptr;

    // Loop through each font family of the result to see if it fits the one we requested.
    bool matchedFontFamily = false;
    FcChar8* fontConfigFamilyNameAfterMatching;
    for (int i = 0; FcPatternGetString(resultPattern.get(), FC_FAMILY, i, &fontConfigFamilyNameAfterMatching) == FcResultMatch; ++i) {
        // If Fontconfig gave us a different font family than the one we requested, we should ignore it
        // and allow WebCore to give us the next font on the CSS fallback list. The exceptions are if
        // this family name is a commonly-used generic family, or if the families are strongly-aliased.
        // Checking for a strong alias comes last, since it is slow.
        String familyNameAfterMatching = String::fromUTF8(reinterpret_cast<char*>(fontConfigFamilyNameAfterMatching));
        if (equalIgnoringASCIICase(familyNameAfterConfiguration, familyNameAfterMatching) || isCommonlyUsedGenericFamily(familyNameString) || areStronglyAliased(familyNameAfterConfiguration, familyNameAfterMatching)) {
            matchedFontFamily = true;
            break;
        }
    }

    if (!matchedFontFamily)
        return nullptr;

    bool fixedWidth, syntheticBold, syntheticOblique;
    getFontPropertiesFromPattern(resultPattern.get(), fontDescription, fixedWidth, syntheticBold, syntheticOblique);

    RefPtr<cairo_font_face_t> fontFace = adoptRef(cairo_ft_font_face_create_for_pattern(resultPattern.get()));
#if ENABLE(VARIATION_FONTS)
    // Cairo doesn't have API to get the FT_Face of an unscaled font, so we need to
    // create a temporary scaled font to get the FT_Face.
    CairoUniquePtr<cairo_font_options_t> options(cairo_font_options_copy(getDefaultCairoFontOptions()));
    cairo_matrix_t matrix;
    cairo_matrix_init_identity(&matrix);
    RefPtr<cairo_scaled_font_t> scaledFont = adoptRef(cairo_scaled_font_create(fontFace.get(), &matrix, &matrix, options.get()));
    CairoFtFaceLocker cairoFtFaceLocker(scaledFont.get());
    if (FT_Face freeTypeFace = cairoFtFaceLocker.ftFace()) {
        auto variants = buildVariationSettings(freeTypeFace, fontDescription);
        if (!variants.isEmpty())
            FcPatternAddString(resultPattern.get(), FC_FONT_VARIATIONS, reinterpret_cast<const FcChar8*>(variants.utf8().data()));
    }
#endif
    auto platformData = std::make_unique<FontPlatformData>(fontFace.get(), resultPattern.get(), fontDescription.computedPixelSize(), fixedWidth, syntheticBold, syntheticOblique, fontDescription.orientation());
    // Verify that this font has an encoding compatible with Fontconfig. Fontconfig currently
    // supports three encodings in FcFreeTypeCharIndex: Unicode, Symbol and AppleRoman.
    // If this font doesn't have one of these three encodings, don't select it.
    if (!platformData->hasCompatibleCharmap())
        return nullptr;

    return platformData;
}

const AtomicString& FontCache::platformAlternateFamilyName(const AtomicString&)
{
    return nullAtom();
}

#if ENABLE(VARIATION_FONTS)
struct VariationDefaults {
    float defaultValue;
    float minimumValue;
    float maximumValue;
};

typedef HashMap<FontTag, VariationDefaults, FourCharacterTagHash, FourCharacterTagHashTraits> VariationDefaultsMap;
typedef HashMap<FontTag, float, FourCharacterTagHash, FourCharacterTagHashTraits> VariationsMap;

static VariationDefaultsMap defaultVariationValues(FT_Face face)
{
    VariationDefaultsMap result;
    FT_MM_Var* ftMMVar;
    if (FT_Get_MM_Var(face, &ftMMVar))
        return result;

    for (unsigned i = 0; i < ftMMVar->num_axis; ++i) {
        auto tag = ftMMVar->axis[i].tag;
        auto b1 = 0xFF & (tag >> 24);
        auto b2 = 0xFF & (tag >> 16);
        auto b3 = 0xFF & (tag >> 8);
        auto b4 = 0xFF & (tag >> 0);
        FontTag resultKey = {{ static_cast<char>(b1), static_cast<char>(b2), static_cast<char>(b3), static_cast<char>(b4) }};
        VariationDefaults resultValues = { narrowPrecisionToFloat(ftMMVar->axis[i].def / 65536.), narrowPrecisionToFloat(ftMMVar->axis[i].minimum / 65536.), narrowPrecisionToFloat(ftMMVar->axis[i].maximum / 65536.) };
        result.set(resultKey, resultValues);
    }
    FT_Done_MM_Var(face->glyph->library, ftMMVar);
    return result;
}

String buildVariationSettings(FT_Face face, const FontDescription& fontDescription)
{
    auto defaultValues = defaultVariationValues(face);
    const auto& variations = fontDescription.variationSettings();

    VariationsMap variationsToBeApplied;
    auto applyVariation = [&](const FontTag& tag, float value) {
        auto iterator = defaultValues.find(tag);
        if (iterator == defaultValues.end())
            return;
        float valueToApply = clampTo(value, iterator->value.minimumValue, iterator->value.maximumValue);
        variationsToBeApplied.set(tag, valueToApply);
    };

    for (auto& variation : variations)
        applyVariation(variation.tag(), variation.value());

    StringBuilder builder;
    for (auto& variation : variationsToBeApplied) {
        if (!builder.isEmpty())
            builder.append(',');
        builder.append(variation.key[0]);
        builder.append(variation.key[1]);
        builder.append(variation.key[2]);
        builder.append(variation.key[3]);
        builder.append('=');
        builder.appendNumber(variation.value);
    }
    return builder.toString();
}
#endif // ENABLE(VARIATION_FONTS)

}
