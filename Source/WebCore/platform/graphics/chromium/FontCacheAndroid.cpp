/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontCache.h"

#include "Font.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "NotImplemented.h"
#include "PlatformSupport.h"
#include "SimpleFontData.h"

#include "SkPaint.h"
#include "SkTypeface_android.h"
#include "SkUtils.h"

#include <unicode/locid.h>
#include <wtf/Assertions.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/CString.h>

namespace WebCore {

static const char* getFallbackFontName(const FontDescription& fontDescription)
{
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

static bool isFallbackFamily(String family)
{
    return family.startsWith("-webkit-")
        || equalIgnoringCase(family, "serif")
        || equalIgnoringCase(family, "sans-serif")
        || equalIgnoringCase(family, "sans")
        || equalIgnoringCase(family, "monospace")
        || equalIgnoringCase(family, "cursive")
        || equalIgnoringCase(family, "fantasy")
        || equalIgnoringCase(family, "times") // Skia aliases for serif
        || equalIgnoringCase(family, "times new roman")
        || equalIgnoringCase(family, "palatino")
        || equalIgnoringCase(family, "georgia")
        || equalIgnoringCase(family, "baskerville")
        || equalIgnoringCase(family, "goudy")
        || equalIgnoringCase(family, "ITC Stone Serif")
        || equalIgnoringCase(family, "arial") // Skia aliases for sans-serif
        || equalIgnoringCase(family, "helvetica")
        || equalIgnoringCase(family, "tahoma")
        || equalIgnoringCase(family, "verdana")
        || equalIgnoringCase(family, "courier") // Skia aliases for monospace
        || equalIgnoringCase(family, "courier new")
        || equalIgnoringCase(family, "monaco");
}

void FontCache::platformInit()
{
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font, const UChar* characters, int length)
{
    icu::Locale locale = icu::Locale::getDefault();
    PlatformSupport::FontFamily family;
    PlatformSupport::getFontFamilyForCharacters(characters, length, locale.getLanguage(), &family);
    if (family.name.isEmpty())
        return 0;

    AtomicString atomicFamily(family.name);
    return getCachedFontData(getCachedFontPlatformData(font.fontDescription(), atomicFamily, DoNotRetain), DoNotRetain);
}

SimpleFontData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

SimpleFontData* FontCache::getLastResortFallbackFont(const FontDescription& description, ShouldRetain shouldRetain)
{
    DEFINE_STATIC_LOCAL(const AtomicString, serif, ("Serif"));
    DEFINE_STATIC_LOCAL(const AtomicString, monospace, ("Monospace"));
    DEFINE_STATIC_LOCAL(const AtomicString, sans, ("Sans"));

    FontPlatformData* fontPlatformData = 0;
    switch (description.genericFamily()) {
    case FontDescription::SerifFamily:
        fontPlatformData = getCachedFontPlatformData(description, serif);
        break;
    case FontDescription::MonospaceFamily:
        fontPlatformData = getCachedFontPlatformData(description, monospace);
        break;
    case FontDescription::SansSerifFamily:
    default:
        fontPlatformData = getCachedFontPlatformData(description, sans);
        break;
    }

    ASSERT(fontPlatformData);
    return getCachedFontData(fontPlatformData, shouldRetain);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName, Vector<unsigned>& traitsMasks)
{
    notImplemented();
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomicString& family)
{
    const char* name = 0;
    CString nameString; // Keeps name valid within scope of this function in case that name is from a family.

    // If a fallback font is being created (e.g. "-webkit-monospace"), convert
    // it in to the fallback name (e.g. "monospace").
    if (!family.length() || family.startsWith("-webkit-"))
        name = getFallbackFontName(fontDescription);
    else {
        nameString = family.string().utf8();
        name = nameString.data();
    }

    int style = SkTypeface::kNormal;
    if (fontDescription.weight() >= FontWeightBold)
        style |= SkTypeface::kBold;
    if (fontDescription.italic())
        style |= SkTypeface::kItalic;

    SkTypeface* typeface = 0;
    FontPlatformData* result = 0;
    FallbackScripts fallbackScript = SkGetFallbackScriptFromID(name);
    if (SkTypeface_ValidScript(fallbackScript)) {
        typeface = SkCreateTypefaceForScript(fallbackScript);
        if (typeface)
            result = new FontPlatformData(typeface, name, fontDescription.computedSize(),
                                          (style & SkTypeface::kBold) && !typeface->isBold(),
                                          (style & SkTypeface::kItalic) && !typeface->isItalic(),
                                          fontDescription.orientation(),
                                          fontDescription.textOrientation());
    } else {
        typeface = SkTypeface::CreateFromName(name, SkTypeface::kNormal);

        // CreateFromName always returns a typeface, falling back to a default font
        // if the one requested could not be found. Calling Equal() with a null
        // pointer will compare the returned font against the default, with the
        // caveat that the default is always of normal style. When that happens,
        // ignore the default font and allow WebCore to provide the next font on the
        // CSS fallback list. The only exception to this occurs when the family name
        // is a commonly used generic family, which is the case when called by
        // getSimilarFontPlatformData() or getLastResortFallbackFont(). In that case
        // the default font is an acceptable result.

        if (!SkTypeface::Equal(typeface, 0) || isFallbackFamily(family.string())) {
            // We had to use normal styling to see if this was a default font. If
            // we need bold or italic, replace with the corrected typeface.
            if (style != SkTypeface::kNormal) {
                typeface->unref();
                typeface = SkTypeface::CreateFromName(name, static_cast<SkTypeface::Style>(style));
            }
            result = new FontPlatformData(typeface, name, fontDescription.computedSize(),
                                          (style & SkTypeface::kBold) && !typeface->isBold(),
                                          (style & SkTypeface::kItalic) && !typeface->isItalic(),
                                          fontDescription.orientation(),
                                          fontDescription.textOrientation());
        }
    }

    SkSafeUnref(typeface);
    return result;
}

} // namespace WebCore
