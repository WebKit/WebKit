/*
 * Copyright (c) 2009-2010, Google Inc. All rights reserved.
 * Copyright (c) 2011, Research In Motion. All rights reserved.
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
#include "PlatformSupport.h"

#include "FontRenderStyle.h"

#include <fontconfig/fontconfig.h>
#include <string.h>
#include <unicode/utf16.h>
#include <wtf/text/CString.h>

namespace WebCore {

static void setFontRenderStyleDefaults(FontRenderStyle* style)
{
    style->useBitmaps = 2;
    style->useAutoHint = 2;
    style->useHinting = 2;
    style->hintStyle = 0;
    style->useAntiAlias = 2;
    style->useSubpixelRendering = 2;
}

void PlatformSupport::getRenderStyleForStrike(const char* family, int sizeAndStyle, FontRenderStyle* out)
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

    setFontRenderStyleDefaults(out);

    if (!match)
        return;

    FcBool boolValue;
    int intValue;

    if (FcPatternGetBool(match, FC_ANTIALIAS, 0, &boolValue) == FcResultMatch)
        out->useAntiAlias = boolValue;
    if (FcPatternGetBool(match, FC_EMBEDDED_BITMAP, 0, &boolValue) == FcResultMatch)
        out->useBitmaps = boolValue;
    if (FcPatternGetBool(match, FC_AUTOHINT, 0, &boolValue) == FcResultMatch)
        out->useAutoHint = boolValue;
    if (FcPatternGetBool(match, FC_HINTING, 0, &boolValue) == FcResultMatch)
        out->useHinting = boolValue;

    if (FcPatternGetInteger(match, FC_RGBA, 0, &intValue) == FcResultMatch) {
        switch (intValue) {
        case FC_RGBA_NONE:
            out->useSubpixelRendering = 0;
            break;
        case FC_RGBA_RGB:
        case FC_RGBA_BGR:
        case FC_RGBA_VRGB:
        case FC_RGBA_VBGR:
            out->useSubpixelRendering = 1;
            break;
        default:
            // This includes FC_RGBA_UNKNOWN.
            out->useSubpixelRendering = 2;
            break;
        }
    }

    FcPatternDestroy(match);
}

}; // namespace WebCore
