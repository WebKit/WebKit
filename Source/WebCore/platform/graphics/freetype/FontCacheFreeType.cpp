/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include "RefPtrCairo.h"
#include "RefPtrFontconfig.h"
#include "StyleFontSizeFunctions.h"
#include "UTF16UChar32Iterator.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <wtf/Assertions.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
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

bool FontCache::configurePatternForFontDescription(FcPattern* pattern, const FontDescription& fontDescription)
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
    bool descriptionAllowsSyntheticBold = fontDescription.hasAutoFontSynthesisWeight();
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
    bool descriptionAllowsSyntheticOblique = fontDescription.hasAutoFontSynthesisStyle();
    if (descriptionAllowsSyntheticOblique && fontDescription.italic()
        && FcPatternGetInteger(pattern, FC_SLANT, 0, &actualFontSlant) == FcResultMatch) {
        syntheticOblique = actualFontSlant == FC_SLANT_ROMAN;
    }
}

RefPtr<Font> FontCache::systemFallbackForCharacters(const FontDescription& description, const Font&, IsForPlatformFont, PreferColoredFont preferColoredFont, const UChar* characters, unsigned length)
{
    RefPtr<FcPattern> resultPattern = m_fontSetCache.bestForCharacters(description, preferColoredFont == PreferColoredFont::Yes, characters, length);
    if (!resultPattern)
        return nullptr;

    bool fixedWidth, syntheticBold, syntheticOblique;
    getFontPropertiesFromPattern(resultPattern.get(), description, fixedWidth, syntheticBold, syntheticOblique);

    RefPtr<cairo_font_face_t> fontFace = adoptRef(cairo_ft_font_face_create_for_pattern(resultPattern.get()));
    FontPlatformData alternateFontData(fontFace.get(), WTFMove(resultPattern), description.computedPixelSize(), fixedWidth, syntheticBold, syntheticOblique, description.orientation());
    return fontForPlatformData(alternateFontData);
}

void FontCache::platformPurgeInactiveFontData()
{
    m_fontSetCache.clear();
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

bool FontCache::isSystemFontForbiddenForEditing(const String&)
{
    return false;
}

Ref<Font> FontCache::lastResortFallbackFont(const FontDescription& fontDescription)
{
    // We want to return a fallback font here, otherwise the logic preventing FontConfig
    // matches for non-fallback fonts might return 0. See isFallbackFontAllowed.
    if (RefPtr<Font> font = fontForFamily(fontDescription, "serif"_s))
        return font.releaseNonNull();

    // This could be reached due to improperly-installed or misconfigured fontconfig.
    RELEASE_ASSERT_NOT_REACHED();
}

Vector<FontSelectionCapabilities> FontCache::getFontSelectionCapabilitiesInFamily(const AtomString&, AllowUserInstalledFonts)
{
    return { };
}

static String getFamilyNameStringFromFamily(const String& family)
{
    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (family.length() && !family.startsWith("-webkit-"_s))
        return family;

    if (family == familyNamesData->at(FamilyNamesIndex::StandardFamily) || family == familyNamesData->at(FamilyNamesIndex::SerifFamily))
        return "serif"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::SansSerifFamily))
        return "sans-serif"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::MonospaceFamily))
        return "monospace"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::CursiveFamily))
        return "cursive"_s;
    if (family == familyNamesData->at(FamilyNamesIndex::FantasyFamily))
        return "fantasy"_s;

#if PLATFORM(GTK)
    if (family == familyNamesData->at(FamilyNamesIndex::SystemUiFamily) || family == "-webkit-system-font"_s)
        return defaultGtkSystemFont();
#endif

    return emptyString();
}

#if FC_VERSION < 21395
// This is based on BSD-licensed code from Skia (src/ports/SkFontMgr_fontconfig.cpp).
// It is obsoleted by newer Fontconfig, see https://gitlab.freedesktop.org/fontconfig/fontconfig/-/issues/294.
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
#endif // FC_VERSION < 21395

static inline bool isCommonlyUsedGenericFamily(const String& familyNameString)
{
    return equalLettersIgnoringASCIICase(familyNameString, "sans"_s)
        || equalLettersIgnoringASCIICase(familyNameString, "sans-serif"_s)
        || equalLettersIgnoringASCIICase(familyNameString, "serif"_s)
        || equalLettersIgnoringASCIICase(familyNameString, "monospace"_s)
        || equalLettersIgnoringASCIICase(familyNameString, "fantasy"_s)
#if PLATFORM(GTK)
        || equalLettersIgnoringASCIICase(familyNameString, "-webkit-system-font"_s)
        || equalLettersIgnoringASCIICase(familyNameString, "-webkit-system-ui"_s)
#endif
        || equalLettersIgnoringASCIICase(familyNameString, "cursive"_s);
}

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomString& family, const FontCreationContext& fontCreationContext)
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

#if FC_VERSION < 21395
    bool matchedFontFamily = false;
    FcChar8* fontConfigFamilyNameAfterMatching;
    for (int i = 0; FcPatternGetString(resultPattern.get(), FC_FAMILY, i, &fontConfigFamilyNameAfterMatching) == FcResultMatch; ++i) {
        String familyNameAfterMatching = String::fromUTF8(reinterpret_cast<char*>(fontConfigFamilyNameAfterMatching));
        if (equalIgnoringASCIICase(familyNameAfterConfiguration, familyNameAfterMatching) || isCommonlyUsedGenericFamily(familyNameString) || areStronglyAliased(familyNameAfterConfiguration, familyNameAfterMatching)) {
            matchedFontFamily = true;
            break;
        }
    }
#else
    // Loop through each font family of the result to see if it fits the one we requested.
    bool matchedFontFamily = false;
    FcValue value;
    FcValueBinding binding;
    for (int i = 0; FcPatternGetWithBinding(resultPattern.get(), FC_FAMILY, i, &value, &binding) == FcResultMatch; ++i) {
        ASSERT(value.type == FcTypeString);
        String familyNameAfterMatching = String::fromUTF8(reinterpret_cast<const char*>(value.u.s));
        // If Fontconfig gave us a different font family than the one we requested, we should ignore it
        // and allow WebCore to give us the next font on the CSS fallback list. The exceptions are if
        // this family name is a commonly-used generic family, or if the families are strongly-aliased.
        if (binding == FcValueBindingStrong || equalIgnoringASCIICase(familyNameAfterConfiguration, familyNameAfterMatching) || isCommonlyUsedGenericFamily(familyNameString)) {
            matchedFontFamily = true;
            break;
        }
    }
#endif

    if (!matchedFontFamily)
        return nullptr;

    bool fixedWidth, syntheticBold, syntheticOblique;
    getFontPropertiesFromPattern(resultPattern.get(), fontDescription, fixedWidth, syntheticBold, syntheticOblique);

    if (fontCreationContext.fontFaceFeatures() && !fontCreationContext.fontFaceFeatures()->isEmpty()) {
        for (auto& fontFaceFeature : *fontCreationContext.fontFaceFeatures()) {
            if (fontFaceFeature.enabled()) {
                const auto& tag = fontFaceFeature.tag();
                const char buffer[] = { tag[0], tag[1], tag[2], tag[3], '\0' };
                FcPatternAddString(resultPattern.get(), FC_FONT_FEATURES, reinterpret_cast<const FcChar8*>(buffer));
            }
        }
    }

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

    auto size = fontDescription.computedPixelSize();
    FontPlatformData platformData(fontFace.get(), WTFMove(resultPattern), size, fixedWidth, syntheticBold, syntheticOblique, fontDescription.orientation());

    platformData.updateSizeWithFontSizeAdjust(fontDescription.fontSizeAdjust());
    auto platformDataUniquePtr = makeUnique<FontPlatformData>(platformData);

    // Verify that this font has an encoding compatible with Fontconfig. Fontconfig currently
    // supports three encodings in FcFreeTypeCharIndex: Unicode, Symbol and AppleRoman.
    // If this font doesn't have one of these three encodings, don't select it.
    if (!platformDataUniquePtr->hasCompatibleCharmap())
        return nullptr;

    return platformDataUniquePtr;
}

std::optional<ASCIILiteral> FontCache::platformAlternateFamilyName(const String&)
{
    return std::nullopt;
}

#if ENABLE(VARIATION_FONTS)
static String fontNameMapName(FT_Face face, unsigned id)
{
    auto nameCount = FT_Get_Sfnt_Name_Count(face);
    if (!nameCount)
        return { };

    auto decodeName = [](FT_SfntName name) -> String {
        switch (name.platform_id) {
        case TT_PLATFORM_MACINTOSH:
            if (name.encoding_id == TT_MAC_ID_ROMAN)
                return String(name.string, name.string_len);
            // FIXME: implement other macintosh encodings.
            break;
        case TT_PLATFORM_APPLE_UNICODE:
        case TT_PLATFORM_ISO:
        case TT_PLATFORM_MICROSOFT:
        case TT_PLATFORM_CUSTOM:
        case TT_PLATFORM_ADOBE:
            // FIXME: implement these platforms.
            break;
        }

        return { };
    };

    for (unsigned i = 0; i < nameCount; ++i) {
        FT_SfntName name;
        if (FT_Get_Sfnt_Name(face, i, &name))
            continue;

        if (name.name_id == id)
            return decodeName(name);
    }

    return { };
}

VariationDefaultsMap defaultVariationValues(FT_Face face, ShouldLocalizeAxisNames shouldLocalizeAxisNames)
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

        String axisName;
        if (shouldLocalizeAxisNames == ShouldLocalizeAxisNames::Yes)
            axisName = fontNameMapName(face, ftMMVar->axis[i].strid);
        if (axisName.isEmpty())
            axisName = String::fromUTF8(ftMMVar->axis[i].name);

        VariationDefaults resultValues = { WTFMove(axisName), narrowPrecisionToFloat(ftMMVar->axis[i].def / 65536.), narrowPrecisionToFloat(ftMMVar->axis[i].minimum / 65536.), narrowPrecisionToFloat(ftMMVar->axis[i].maximum / 65536.) };
        result.set(resultKey, resultValues);
    }
    FT_Done_MM_Var(face->glyph->library, ftMMVar);
    return result;
}

String buildVariationSettings(FT_Face face, const FontDescription& fontDescription)
{
    auto defaultValues = defaultVariationValues(face, ShouldLocalizeAxisNames::No);
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
        builder.append(FormattedNumber::fixedPrecision(variation.value));
    }
    return builder.toString();
}
#endif // ENABLE(VARIATION_FONTS)

void FontCache::platformInvalidate()
{
}

}
