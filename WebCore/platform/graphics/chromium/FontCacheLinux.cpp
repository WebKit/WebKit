/*
 * Copyright (c) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#include <fontconfig/fontconfig.h>

#include "AtomicString.h"
#include "CString.h"
#include "Font.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"

#include "SkPaint.h"
#include "SkTypeface.h"
#include "SkUtils.h"

namespace WebCore {

void FontCache::platformInit()
{
}

const SimpleFontData* FontCache::getFontDataForCharacters(const Font& font,
                                                          const UChar* characters,
                                                          int length)
{
    FcCharSet* cset = FcCharSetCreate();
    for (int i = 0; i < length; ++i)
        FcCharSetAddChar(cset, characters[i]);

    FcPattern* pattern = FcPatternCreate();

    FcValue fcvalue;
    fcvalue.type = FcTypeCharSet;
    fcvalue.u.c = cset;
    FcPatternAdd(pattern, FC_CHARSET, fcvalue, 0);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern* match = FcFontMatch(0, pattern, &result);
    FcPatternDestroy(pattern);

    SimpleFontData* ret = 0;

    if (match) {
        FcChar8* family;
        if (FcPatternGetString(match, FC_FAMILY, 0, &family) == FcResultMatch) {
            AtomicString fontFamily(reinterpret_cast<char*>(family));
            ret = getCachedFontData(getCachedFontPlatformData(font.fontDescription(), fontFamily, false));
        }
        FcPatternDestroy(match);
    }

    FcCharSetDestroy(cset);

    return ret;
}

FontPlatformData* FontCache::getSimilarFontPlatformData(const Font& font)
{
    return 0;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const FontDescription& description)
{
    static AtomicString arialStr("Arial");
    return getCachedFontPlatformData(description, arialStr);
}

void FontCache::getTraitsInFamily(const AtomicString& familyName,
                                  Vector<unsigned>& traitsMasks)
{
    notImplemented();
}

FontPlatformData* FontCache::createFontPlatformData(const FontDescription& fontDescription,
                                                    const AtomicString& family)
{
    const char* name = 0;
    CString s;

    if (family.length() == 0) {
        static const struct {
            FontDescription::GenericFamilyType mType;
            const char* mName;
        } fontDescriptions[] = {
            { FontDescription::SerifFamily, "serif" },
            { FontDescription::SansSerifFamily, "sans-serif" },
            { FontDescription::MonospaceFamily, "monospace" },
            { FontDescription::CursiveFamily, "cursive" },
            { FontDescription::FantasyFamily, "fantasy" }
        };

        FontDescription::GenericFamilyType type = fontDescription.genericFamily();
        for (unsigned i = 0; i < SK_ARRAY_COUNT(fontDescriptions); i++) {
            if (type == fontDescriptions[i].mType) {
                name = fontDescriptions[i].mName;
                break;
            }
        }
        // if we fall out of the loop, it's ok for name to still be 0
    }
    else {    // convert the name to utf8
        s = family.string().utf8();
        name = s.data();
    }

    int style = SkTypeface::kNormal;
    if (fontDescription.weight() >= FontWeightBold)
        style |= SkTypeface::kBold;
    if (fontDescription.italic())
        style |= SkTypeface::kItalic;

    // FIXME:  This #ifdef can go away once we're firmly using the new Skia.
    // During the transition, this makes the code compatible with both versions.
#ifdef SK_USE_OLD_255_TO_256
    SkTypeface* tf = SkTypeface::CreateFromName(name, static_cast<SkTypeface::Style>(style));
#else
    SkTypeface* tf = SkTypeface::Create(name, static_cast<SkTypeface::Style>(style));
#endif
    if (!tf)
        return 0;

    FontPlatformData* result =
        new FontPlatformData(tf,
                             fontDescription.computedSize(),
                             (style & SkTypeface::kBold) && !tf->isBold(),
                             (style & SkTypeface::kItalic) && !tf->isItalic());
    tf->unref();
    return result;
}

}  // namespace WebCore
