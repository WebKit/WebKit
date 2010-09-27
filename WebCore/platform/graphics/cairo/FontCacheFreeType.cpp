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

#include "CString.h"
#include "Font.h"
#include "OwnPtrCairo.h"
#include "PlatformRefPtrCairo.h"
#include "SimpleFontData.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <wtf/Assertions.h>

namespace WebCore {

void FontCache::platformInit()
{
    // It's fine to call FcInit multiple times per the documentation.
    if (!FcInit())
        ASSERT_NOT_REACHED();
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    FcResult fresult;
    FontPlatformData* prim = const_cast<FontPlatformData*>(&font.primaryFont()->platformData());

    // FIXME: This should not happen, apparently. We are null-checking
    // for now just to avoid crashing.
    if (!prim || !prim->m_pattern)
        return 0;

    if (!prim->m_fallbacks)
        prim->m_fallbacks = FcFontSort(0, prim->m_pattern.get(), FcTrue, 0, &fresult);

    FcFontSet* fs = prim->m_fallbacks;

    for (int i = 0; i < fs->nfont; i++) {
        PlatformRefPtr<FcPattern> fin = adoptPlatformRef(FcFontRenderPrepare(0, prim->m_pattern.get(), fs->fonts[i]));
        cairo_font_face_t* fontFace = cairo_ft_font_face_create_for_pattern(fin.get());
        FontPlatformData alternateFont(fontFace, font.fontDescription().computedPixelSize(), false, false);
        cairo_font_face_destroy(fontFace);
        alternateFont.m_pattern = fin;
        SimpleFontData* sfd = getCachedFontData(&alternateFont);
        if (sfd->containsCharacters(characters, length))
            return sfd;
    }

    return 0;
}

SimpleFontData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

SimpleFontData* FontCache::getLastResortFallbackFont(const FontDescription& fontDescription)
{
    // We want to return a fallback font here, otherwise the logic preventing FontConfig
    // matches for non-fallback fonts might return 0. See isFallbackFontAllowed.
    static AtomicString timesStr("serif");
    return getCachedFontData(fontDescription, timesStr);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
}

static CString getFamilyNameStringFromFontDescriptionAndFamily(const FontDescription& fontDescription, const AtomicString& family)
{
    // If we're creating a fallback font (e.g. "-webkit-monospace"), convert the name into
    // the fallback name (like "monospace") that fontconfig understands.
    if (family.length() && !family.startsWith("-webkit-"))
        return family.string().utf8();

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


static bool isFallbackFontAllowed(const CString& familyName)
{
    return !strcasecmp(familyName.data(), "sans")
           || !strcasecmp(familyName.data(), "sans-serif")
           || !strcasecmp(familyName.data(), "serif")
           || !strcasecmp(familyName.data(), "monospace");
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    // The CSS font matching algorithm (http://www.w3.org/TR/css3-fonts/#font-matching-algorithm)
    // says that we must find an exact match for font family, slant (italic or oblique can be used)
    // and font weight (we only match bold/non-bold here).
    PlatformRefPtr<FcPattern> pattern = adoptPlatformRef(FcPatternCreate());
    CString familyNameString = getFamilyNameStringFromFontDescriptionAndFamily(fontDescription, family);
    if (!FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(familyNameString.data())))
        return 0;

    bool italic = fontDescription.italic();
    if (!FcPatternAddInteger(pattern.get(), FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN))
        return 0;
    bool bold = fontDescription.weight() >= FontWeightBold;
    if (!FcPatternAddInteger(pattern.get(), FC_WEIGHT, bold ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL))
        return 0;
    if (!FcPatternAddDouble(pattern.get(), FC_PIXEL_SIZE, fontDescription.computedPixelSize()))
        return 0;

    // The following comment and strategy are originally from Skia (src/ports/SkFontHost_fontconfig.cpp):
    // Font matching:
    // CSS often specifies a fallback list of families:
    //    font-family: a, b, c, serif;
    // However, fontconfig will always do its best to find *a* font when asked
    // for something so we need a way to tell if the match which it has found is
    // "good enough" for us. Otherwise, we can return null which gets piped up
    // and lets WebKit know to try the next CSS family name. However, fontconfig
    // configs allow substitutions (mapping "Arial -> Helvetica" etc) and we
    // wish to support that.
    //
    // Thus, if a specific family is requested we set @family_requested. Then we
    // record two strings: the family name after config processing and the
    // family name after resolving. If the two are equal, it's a good match.
    //
    // So consider the case where a user has mapped Arial to Helvetica in their
    // config.
    //    requested family: "Arial"
    //    post_config_family: "Helvetica"
    //    post_match_family: "Helvetica"
    //      -> good match
    //
    // and for a missing font:
    //    requested family: "Monaco"
    //    post_config_family: "Monaco"
    //    post_match_family: "Times New Roman"
    //      -> BAD match
    //
    FcConfigSubstitute(0, pattern.get(), FcMatchPattern);
    FcDefaultSubstitute(pattern.get());

    FcChar8* familyNameAfterConfiguration;
    FcPatternGetString(pattern.get(), FC_FAMILY, 0, &familyNameAfterConfiguration);

    FcResult fontConfigResult;
    PlatformRefPtr<FcPattern> resultPattern = adoptPlatformRef(FcFontMatch(0, pattern.get(), &fontConfigResult));
    if (!resultPattern) // No match.
        return 0;

    // Properly handle the situation where Fontconfig gives us a font that has a different family than we requested.
    FcChar8* familyNameAfterMatching;
    FcPatternGetString(resultPattern.get(), FC_FAMILY, 0, &familyNameAfterMatching);
    if (strcasecmp(reinterpret_cast<char*>(familyNameAfterConfiguration),
            reinterpret_cast<char*>(familyNameAfterMatching)) && !isFallbackFontAllowed(familyNameString))
        return 0;

    return new FontPlatformData(resultPattern.get(), fontDescription);
}

}
