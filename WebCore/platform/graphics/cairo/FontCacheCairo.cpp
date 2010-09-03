/*
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
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
#include "SimpleFontData.h"
#include <wtf/Assertions.h>

namespace WebCore {

void FontCache::platformInit()
{
    if (!FontPlatformData::init())
        ASSERT_NOT_REACHED();
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
#if defined(USE_FREETYPE)
    FcResult fresult;
    FontPlatformData* prim = const_cast<FontPlatformData*>(&font.primaryFont()->platformData());

    // FIXME: This should not happen, apparently. We are null-checking
    // for now just to avoid crashing.
    if (!prim || !prim->m_pattern)
        return 0;

    if (!prim->m_fallbacks)
        prim->m_fallbacks = FcFontSort(NULL, prim->m_pattern, FcTrue, NULL, &fresult);

    FcFontSet* fs = prim->m_fallbacks;

    for (int i = 0; i < fs->nfont; i++) {
        FcPattern* fin = FcFontRenderPrepare(NULL, prim->m_pattern, fs->fonts[i]);
        cairo_font_face_t* fontFace = cairo_ft_font_face_create_for_pattern(fin);
        FontPlatformData alternateFont(fontFace, font.fontDescription().computedPixelSize(), false, false);
        cairo_font_face_destroy(fontFace);
        alternateFont.m_pattern = fin;
        SimpleFontData* sfd = getCachedFontData(&alternateFont);
        if (sfd->containsCharacters(characters, length))
            return sfd;
    }
#endif

    return 0;
}

SimpleFontData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

SimpleFontData* FontCache::getLastResortFallbackFont(const FontDescription& fontDescription)
{
    // FIXME: Would be even better to somehow get the user's default font here.
    // For now we'll pick the default that the user would get without changing any prefs.
    static AtomicString timesStr("Times New Roman");
    return getCachedFontData(fontDescription, timesStr);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
}

static bool isWellKnownFontName(const AtomicString family)
{
    // Fonts that are used by layout tests included. The fact that
    // they are used in Layout Tests indicate web compatibility issues
    // if we do not handle them correctly.
    if (equalIgnoringCase(family, "sans-serif") || equalIgnoringCase(family, "sans")
        || equalIgnoringCase(family, "serif") || equalIgnoringCase(family, "mono")
        || equalIgnoringCase(family, "monospace") || equalIgnoringCase(family, "cursive")
        || equalIgnoringCase(family, "fantasy") || equalIgnoringCase(family, "Times")
        || equalIgnoringCase(family, "Courier") || equalIgnoringCase(family, "Helvetica")
        || equalIgnoringCase(family, "Arial") || equalIgnoringCase(family, "Lucida Grande")
        || equalIgnoringCase(family, "Ahem") || equalIgnoringCase(family, "Georgia")
        || equalIgnoringCase(family, "Times New Roman"))
        return true;

    return false;
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
#if defined(USE_FREETYPE)
    // Handle generic family types specially, because fontconfig does not know them, but we have
    // code to fallback correctly in our platform data implementation.
    if (!family.length() || family.startsWith("-webkit-")
        || (fontDescription.genericFamily() != FontDescription::NoFamily)
        || isWellKnownFontName(family))
        return new FontPlatformData(fontDescription, family);

    // First check the font exists.
    CString familyNameString = family.string().utf8();
    const char* fcfamily = familyNameString.data();

    OwnPtr<FcPattern> pattern(FcPatternCreate());
    if (!FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(fcfamily)))
        return 0;

    OwnPtr<FcObjectSet> objectSet(FcObjectSetCreate());
    if (!FcObjectSetAdd(objectSet.get(), FC_FAMILY))
        return 0;

    OwnPtr<FcFontSet> fontSet(FcFontList(0, pattern.get(), objectSet.get()));

    if (!fontSet)
        return 0;

    if (!fontSet->fonts)
        return 0;
#endif

    return new FontPlatformData(fontDescription, family);
}

}
