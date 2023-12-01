/*
 * Copyright (C) 2022 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "FontSetCache.h"

#include "CairoUtilities.h"
#include "CharacterProperties.h"
#include "FontCache.h"

namespace WebCore {

FontSetCache::FontSet::FontSet(RefPtr<FcPattern>&& fontPattern)
    : pattern(WTFMove(fontPattern))
{
    FcResult result;
    fontSet.reset(FcFontSort(nullptr, pattern.get(), FcTrue, nullptr, &result));
    for (int i = 0; i < fontSet->nfont; ++i) {
        FcPattern* fontSetPattern = fontSet->fonts[i];
        FcCharSet* charSet;

        if (FcPatternGetCharSet(fontSetPattern, FC_CHARSET, 0, &charSet) == FcResultMatch)
            patterns.append({ fontSetPattern, charSet });
    }
}

RefPtr<FcPattern> FontSetCache::bestForCharacters(const FontDescription& fontDescription, bool preferColoredFont, StringView stringView)
{
    auto addResult = m_cache.ensure(FontSetCacheKey(fontDescription, preferColoredFont), [&fontDescription, preferColoredFont]() -> std::unique_ptr<FontSetCache::FontSet> {
        RefPtr<FcPattern> pattern = adoptRef(FcPatternCreate());
        FcPatternAddBool(pattern.get(), FC_SCALABLE, FcTrue);
#ifdef FC_COLOR
        if (preferColoredFont)
            FcPatternAddBool(pattern.get(), FC_COLOR, FcTrue);
#else
        UNUSED_VARIABLE(preferColoredFont);
#endif
        if (!FontCache::configurePatternForFontDescription(pattern.get(), fontDescription))
            return nullptr;

        FcConfigSubstitute(nullptr, pattern.get(), FcMatchPattern);
        cairo_ft_font_options_substitute(getDefaultCairoFontOptions(), pattern.get());
        FcDefaultSubstitute(pattern.get());
        return makeUnique<FontSetCache::FontSet>(WTFMove(pattern));
    });

    if (!addResult.iterator->value)
        return nullptr;

    auto& cachedFontSet = *addResult.iterator->value;
    if (cachedFontSet.patterns.isEmpty()) {
        FcResult result;
        return adoptRef(FcFontMatch(nullptr, cachedFontSet.pattern.get(), &result));
    }

    FcUniquePtr<FcCharSet> fontConfigCharSet(FcCharSetCreate());
    bool hasNonIgnorableCharacters = false;
    for (char32_t character : stringView.codePoints()) {
        if (!isDefaultIgnorableCodePoint(character)) {
            FcCharSetAddChar(fontConfigCharSet.get(), character);
            hasNonIgnorableCharacters = true;
        }
    }

    FcPattern* bestPattern = nullptr;
    if (hasNonIgnorableCharacters) {
        int minScore = std::numeric_limits<int>::max();
        for (const auto& [pattern, charSet] : cachedFontSet.patterns) {
            if (!charSet)
                continue;

            int score = FcCharSetSubtractCount(fontConfigCharSet.get(), charSet);
            if (!score) {
                bestPattern = pattern;
                break;
            }

            if (score < minScore) {
                bestPattern = pattern;
                minScore = score;
            }
        }
    }

    // If there aren't fonts with the given characters or all characters are ignorable, the first one is the best match.
    if (!bestPattern)
        bestPattern = cachedFontSet.patterns[0].first;

    return adoptRef(FcFontRenderPrepare(nullptr, cachedFontSet.pattern.get(), bestPattern));
}

void FontSetCache::clear()
{
    m_cache.clear();
}

} // namespace WebCore
