/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "WebFontInfo.h"

#include "WebFontRenderStyle.h"
#include "platform/linux/WebFontFamily.h"
#include <fontconfig/fontconfig.h>
#include <string.h>
#include <unicode/utf16.h>

namespace WebKit {

void WebFontInfo::familyForChars(const WebUChar* characters, size_t numCharacters, const char* preferredLocale, WebFontFamily* family)
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

    if (preferredLocale) {
        FcLangSet* langset = FcLangSetCreate();
        FcLangSetAdd(langset, reinterpret_cast<const FcChar8 *>(preferredLocale));
        FcPatternAddLangSet(pattern, FC_LANG, langset);
        FcLangSetDestroy(langset);
    }

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcFontSet* fontSet = FcFontSort(0, pattern, 0, 0, &result);
    FcPatternDestroy(pattern);
    FcCharSetDestroy(cset);

    if (!fontSet) {
        family->name = WebCString();
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
            family->name = WebCString(charFamily, strlen(charFamily));
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

void WebFontInfo::renderStyleForStrike(const char* family, int sizeAndStyle, WebFontRenderStyle* out)
{
    bool isBold = sizeAndStyle & 1;
    bool isItalic = sizeAndStyle & 2;
    int pixelSize = sizeAndStyle >> 2;

    FcPattern* pattern = FcPatternCreate();
    FcValue fcvalue;

    fcvalue.type = FcTypeString;
    fcvalue.u.s = reinterpret_cast<const FcChar8 *>(family);
    FcPatternAdd(pattern, FC_FAMILY, fcvalue, FcFalse);

    fcvalue.type = FcTypeInteger;
    fcvalue.u.i = isBold ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL;
    FcPatternAdd(pattern, FC_WEIGHT, fcvalue, FcFalse);

    fcvalue.type = FcTypeInteger;
    fcvalue.u.i = isItalic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
    FcPatternAdd(pattern, FC_SLANT, fcvalue, FcFalse);

    fcvalue.type = FcTypeBool;
    fcvalue.u.b = FcTrue;
    FcPatternAdd(pattern, FC_SCALABLE, fcvalue, FcFalse);

    fcvalue.type = FcTypeDouble;
    fcvalue.u.d = pixelSize;
    FcPatternAdd(pattern, FC_SIZE, fcvalue, FcFalse);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    // Some versions of fontconfig don't actually write a value into result.
    // However, it's not clear from the documentation if result should be a
    // non-0 pointer: future versions might expect to be able to write to
    // it. So we pass in a valid pointer and ignore it.
    FcPattern* match = FcFontMatch(0, pattern, &result);
    FcPatternDestroy(pattern);

    out->setDefaults();

    if (!match)
        return;

    FcBool b;
    int i;

    if (FcPatternGetBool(match, FC_ANTIALIAS, 0, &b) == FcResultMatch)
        out->useAntiAlias = b;
    if (FcPatternGetBool(match, FC_EMBEDDED_BITMAP, 0, &b) == FcResultMatch)
        out->useBitmaps = b;
    if (FcPatternGetBool(match, FC_AUTOHINT, 0, &b) == FcResultMatch)
        out->useAutoHint = b;
    if (FcPatternGetBool(match, FC_HINTING, 0, &b) == FcResultMatch)
        out->useHinting = b;
    if (FcPatternGetInteger(match, FC_HINT_STYLE, 0, &i) == FcResultMatch)
        out->hintStyle = i;
    if (FcPatternGetInteger(match, FC_RGBA, 0, &i) == FcResultMatch) {
        switch (i) {
        case FC_RGBA_NONE:
            out->useSubpixel = 0;
            break;
        case FC_RGBA_RGB:
        case FC_RGBA_BGR:
        case FC_RGBA_VRGB:
        case FC_RGBA_VBGR:
            out->useSubpixel = 1;
            break;
        default:
            // This includes FC_RGBA_UNKNOWN.
            out->useSubpixel = 2;
            break;
        }
    }

    FcPatternDestroy(match);
}

} // namespace WebKit
