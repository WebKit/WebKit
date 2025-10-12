/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontCache.h"

#if PLATFORM(WIN) && USE(CAIRO)

namespace WebCore {

LONG adjustedGDIFontWeight(LONG, const String&);
GDIObject<HFONT> createGDIFont(const AtomString&, LONG, bool, int);
LONG toGDIFontWeight(FontSelectionValue);
bool isGDIFontWeightBold(LONG);

std::unique_ptr<FontPlatformData> FontCache::createFontPlatformData(const FontDescription& fontDescription, const AtomString& family, const FontCreationContext&, OptionSet<FontLookupOptions> options)
{
    LONG weight = adjustedGDIFontWeight(toGDIFontWeight(fontDescription.weight()), family);
    auto hfont = createGDIFont(family, weight, isItalic(fontDescription.fontStyleSlope()),
        fontDescription.computedSize() * cWindowsFontScaleFactor);

    if (!hfont)
        return nullptr;

    LOGFONT logFont;
    GetObject(hfont.get(), sizeof(LOGFONT), &logFont);

    bool synthesizeBold = !options.contains(FontLookupOptions::DisallowBoldSynthesis)
        && isGDIFontWeightBold(weight) && !isGDIFontWeightBold(logFont.lfWeight);
    bool synthesizeItalic = !options.contains(FontLookupOptions::DisallowObliqueSynthesis)
        && isItalic(fontDescription.fontStyleSlope()) && !logFont.lfItalic;

    auto result = makeUnique<FontPlatformData>(WTFMove(hfont), fontDescription.computedSize(), synthesizeBold, synthesizeItalic);

    bool fontCreationFailed = !result->scaledFont();

    if (fontCreationFailed) {
        // The creation of the cairo scaled font failed for some reason. We already asserted in debug builds, but to make
        // absolutely sure that we don't use this font, go ahead and return 0 so that we can fall back to the next
        // font.
        return nullptr;
    }

    return result;
}

} // namespace WebCore

#endif // PLATFORM(WIN) && USE(CAIRO)
