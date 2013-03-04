/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2012, 2013 Research In Motion Limited. All rights reserved.
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

#include "Font.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "ITypeUtils.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PlatformSupport.h"
#include "SimpleFontData.h"

#include <BlackBerryPlatformGraphicsContext.h>
#include <fontconfig/fontconfig.h>
#include <fs_api.h>
#include <unicode/locid.h>
#include <wtf/Assertions.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>

namespace WebCore {

void FontCache::platformInit()
{
    // It's fine to call FcInit multiple times per the documentation.
    if (!FcInit())
        CRASH();
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    icu::Locale locale = icu::Locale::getDefault();
    PlatformSupport::FontFamily family;
    PlatformSupport::getFontFamilyForCharacters(characters, length, locale.getLanguage(), font.fontDescription(), &family);
    if (family.name.isEmpty())
        return 0;

    AtomicString atomicFamily(family.name);
    // Changes weight and/or italic of given FontDescription depends on
    // the result of fontconfig so that keeping the correct font mapping
    // of the given characters. See http://crbug.com/32109 for details.
    bool shouldSetFakeBold = false;
    bool shouldSetFakeItalic = false;
    FontDescription description(font.fontDescription());
    if (family.isBold && description.weight() < FontWeightBold)
        description.setWeight(FontWeightBold);
    if (!family.isBold && description.weight() >= FontWeightBold) {
        shouldSetFakeBold = true;
        description.setWeight(FontWeightNormal);
    }
    if (family.isItalic && description.italic() == FontItalicOff)
        description.setItalic(FontItalicOn);
    if (!family.isItalic && description.italic() == FontItalicOn) {
        shouldSetFakeItalic = true;
        description.setItalic(FontItalicOff);
    }

    FontPlatformData* substitutePlatformData = getCachedFontPlatformData(description, atomicFamily, DoNotRetain);
    if (!substitutePlatformData)
        return 0;
    FontPlatformData platformData = FontPlatformData(*substitutePlatformData);
    platformData.setFakeBold(shouldSetFakeBold);
    platformData.setFakeItalic(shouldSetFakeItalic);
    return getCachedFontData(&platformData, DoNotRetain);
}

SimpleFontData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

SimpleFontData* FontCache::getLastResortFallbackFont(const FontDescription& description, ShouldRetain)
{
    DEFINE_STATIC_LOCAL(const AtomicString, sansStr, ("Sans"));
    DEFINE_STATIC_LOCAL(const AtomicString, serifStr, ("Serif"));
    DEFINE_STATIC_LOCAL(const AtomicString, monospaceStr, ("Monospace"));

    FontPlatformData* fontPlatformData = 0;
    switch (description.genericFamily()) {
    case FontDescription::SerifFamily:
        fontPlatformData = getCachedFontPlatformData(description, serifStr);
        break;
    case FontDescription::MonospaceFamily:
        fontPlatformData = getCachedFontPlatformData(description, monospaceStr);
        break;
    case FontDescription::SansSerifFamily:
    default:
        fontPlatformData = getCachedFontPlatformData(description, sansStr);
        break;
    }

    if (!fontPlatformData) {
        DEFINE_STATIC_LOCAL(const AtomicString, arialStr, ("Arial"));
        fontPlatformData = getCachedFontPlatformData(description, arialStr);
    }

    ASSERT(fontPlatformData);
    return getCachedFontData(fontPlatformData);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    notImplemented();
}

static String getFamilyNameStringFromFontDescriptionAndFamily(const FontDescription& fontDescription, const AtomicString& family)
{
    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (family.length() && !family.startsWith("-webkit-"))
        return family.string();

    switch (fontDescription.genericFamily()) {
    case FontDescription::StandardFamily:
    case FontDescription::SerifFamily:
        return "serif";
    case FontDescription::SansSerifFamily:
        return "sans-serif";
    case FontDescription::MonospaceFamily:
        return "monospace";
    case FontDescription::CursiveFamily:
        return "cursive";
    case FontDescription::FantasyFamily:
        return "fantasy";
    case FontDescription::NoFamily:
    default:
        return "";
    }
}

int fontWeightToFontconfigWeight(FontWeight weight)
{
    switch (weight) {
    case FontWeight100:
        return FC_WEIGHT_THIN;
    case FontWeight200:
        return FC_WEIGHT_ULTRALIGHT;
    case FontWeight300:
        return FC_WEIGHT_LIGHT;
    case FontWeight400:
        return FC_WEIGHT_REGULAR;
    case FontWeight500:
        return FC_WEIGHT_MEDIUM;
    case FontWeight600:
        return FC_WEIGHT_SEMIBOLD;
    case FontWeight700:
        return FC_WEIGHT_BOLD;
    case FontWeight800:
        return FC_WEIGHT_EXTRABOLD;
    case FontWeight900:
        return FC_WEIGHT_ULTRABLACK;
    default:
        ASSERT_NOT_REACHED();
        return FC_WEIGHT_REGULAR;
    }
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    // The CSS font matching algorithm (http://www.w3.org/TR/css3-fonts/#font-matching-algorithm)
    // says that we must find an exact match for font family, slant (italic or oblique can be used)
    // and font weight (we only match bold/non-bold here).
    FcPattern* pattern = FcPatternCreate();
    String familyNameString(getFamilyNameStringFromFontDescriptionAndFamily(fontDescription, family));
    if (!FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8*>(familyNameString.utf8().data())))
        return 0;

    bool italic = fontDescription.italic();
    if (!FcPatternAddInteger(pattern, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN))
        return 0;
    if (!FcPatternAddInteger(pattern, FC_WEIGHT, fontWeightToFontconfigWeight(fontDescription.weight())))
        return 0;
    if (!FcPatternAddDouble(pattern, FC_PIXEL_SIZE, fontDescription.computedPixelSize()))
        return 0;

    // The strategy is originally from Skia (src/ports/SkFontHost_fontconfig.cpp):

    // Allow Fontconfig to do pre-match substitution. Unless we are accessing a "fallback"
    // family like "sans," this is the only time we allow Fontconfig to substitute one
    // family name for another (i.e. if the fonts are aliased to each other).
    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcChar8* fontConfigFamilyNameAfterConfiguration;
    FcPatternGetString(pattern, FC_FAMILY, 0, &fontConfigFamilyNameAfterConfiguration);
    String familyNameAfterConfiguration = String::fromUTF8(reinterpret_cast<char*>(fontConfigFamilyNameAfterConfiguration));

    FcResult fontConfigResult;
    FcPattern* resultPattern = FcFontMatch(0, pattern, &fontConfigResult);
    FcPatternDestroy(pattern);
    if (!resultPattern) // No match.
        return 0;

    FcChar8* fontConfigFamilyNameAfterMatching;
    FcPatternGetString(resultPattern, FC_FAMILY, 0, &fontConfigFamilyNameAfterMatching);
    String familyNameAfterMatching = String::fromUTF8(reinterpret_cast<char*>(fontConfigFamilyNameAfterMatching));

    // If Fontconfig gave use a different font family than the one we requested, we should ignore it
    // and allow WebCore to give us the next font on the CSS fallback list. The only exception is if
    // this family name is a commonly used generic family.
    if (!equalIgnoringCase(familyNameAfterConfiguration, familyNameAfterMatching)
        && !(equalIgnoringCase(familyNameString, "sans") || equalIgnoringCase(familyNameString, "sans-serif")
            || equalIgnoringCase(familyNameString, "serif") || equalIgnoringCase(familyNameString, "monospace")
            || equalIgnoringCase(familyNameString, "fantasy") || equalIgnoringCase(familyNameString, "cursive")))
        return 0;

    int fontWeight;
    FcPatternGetInteger(resultPattern, FC_WEIGHT, 0, &fontWeight);
    bool shouldFakeBold = fontWeight < FC_WEIGHT_BOLD && fontDescription.weight() >= FontWeightBold;

    int fontSlant;
    FcPatternGetInteger(resultPattern, FC_SLANT, 0, &fontSlant);
    bool shouldFakeItalic = fontSlant == FC_SLANT_ROMAN && fontDescription.italic() == FontItalicOn;

    FcChar8* fontFileName;
    FcPatternGetString(resultPattern, FC_FILE, 0, &fontFileName);
    // fprintf(stderr, "Looking for %s found %s: i:%d b:%d\n", family.string().latin1().data(), fontFileName, shouldFakeItalic, shouldFakeBold);
    FcPatternDestroy(resultPattern);

    FILECHAR name[MAX_FONT_NAME_LEN+1];
    memset(name, 0, MAX_FONT_NAME_LEN+1);
    // fprintf(stderr, "FS_load_font %s: ", fontFileName);
    if (FS_load_font(BlackBerry::Platform::Graphics::getIType(), reinterpret_cast<FILECHAR*>(fontFileName), 0, 0, MAX_FONT_NAME_LEN, name) != SUCCESS)
        return 0;
    // fprintf(stderr, " %s\n", name);

    return new FontPlatformData(name, fontDescription.computedSize(), shouldFakeBold, shouldFakeItalic, fontDescription.orientation(), fontDescription.textOrientation());
}

} // namespace WebCore
