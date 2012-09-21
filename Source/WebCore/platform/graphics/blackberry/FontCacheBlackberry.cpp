/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "FontCache.h"

#include "FontRenderStyle.h"

#include <fontconfig/fontconfig.h>
#include <string.h>
#include <unicode/utf16.h>
#include <wtf/text/CString.h>

namespace WebCore {

void FontCache::getFontFamilyForCharacters(const UChar* characters, size_t numCharacters, const char*, FontCache::SimpleFontFamily* family)
{
    FcCharSet* cset = FcCharSetCreate();
    for (size_t i = 0; i < numCharacters; ++i) {
        if (U16_IS_SURROGATE(characters[i])
            && U16_IS_SURROGATE_LEAD(characters[i])
            && i != numCharacters - 1
            && U16_IS_TRAIL(characters[i + 1])) {
            FcCharSetAddChar(cset, U16_GET_SUPPLEMENTARY(characters[i], characters[i+1]));
            i++;
        } else
              FcCharSetAddChar(cset, characters[i]);
    }
    FcPattern* pattern = FcPatternCreate();

    FcValue fcvalue;
    fcvalue.type = FcTypeCharSet;
    fcvalue.u.c = cset;
    FcPatternAdd(pattern, FC_CHARSET, fcvalue, FcFalse);

    fcvalue.type = FcTypeBool;
    fcvalue.u.b = FcTrue;
    FcPatternAdd(pattern, FC_SCALABLE, fcvalue, FcFalse);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcFontSet* fontSet = FcFontSort(0, pattern, 0, 0, &result);
    FcPatternDestroy(pattern);
    FcCharSetDestroy(cset);

    if (!fontSet) {
        family->name = String();
        family->isBold = false;
        family->isItalic = false;
        return;
    }

    // Older versions of fontconfig have a bug where they cannot select
    // only scalable fonts so we have to manually filter the results.
    for (int i = 0; i < fontSet->nfont; ++i) {
        FcPattern* current = fontSet->fonts[i];
        FcBool isScalable;

        if (FcPatternGetBool(current, FC_SCALABLE, 0, &isScalable) != FcResultMatch
            || !isScalable)
            continue;

        // fontconfig can also return fonts which are unreadable
        FcChar8* cFilename;
        if (FcPatternGetString(current, FC_FILE, 0, &cFilename) != FcResultMatch)
            continue;

        if (access(reinterpret_cast<char*>(cFilename), R_OK))
            continue;

        FcChar8* familyName;
        if (FcPatternGetString(current, FC_FAMILY, 0, &familyName) == FcResultMatch) {
            const char* charFamily = reinterpret_cast<char*>(familyName);
            family->name = String::fromUTF8(charFamily, strlen(charFamily));
        }

        int weight;
        if (FcPatternGetInteger(current, FC_WEIGHT, 0, &weight) == FcResultMatch)
            family->isBold = weight >= FC_WEIGHT_BOLD;
        else
            family->isBold = false;

        int slant;
        if (FcPatternGetInteger(current, FC_SLANT, 0, &slant) == FcResultMatch)
            family->isItalic = slant != FC_SLANT_ROMAN;
        else
            family->isItalic = false;

        FcFontSetDestroy(fontSet);
        return;
    }

    FcFontSetDestroy(fontSet);
}

}; // namespace WebCore
