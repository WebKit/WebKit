/*
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov
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
#include "SystemFallbackFontCache.h"

#include "FontCache.h"
#include "FontCascade.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

SystemFallbackFontCache& SystemFallbackFontCache::forCurrentThread()
{
    return FontCache::forCurrentThread().systemFallbackFontCache();
}

RefPtr<Font> SystemFallbackFontCache::systemFallbackFontForCharacterCluster(const Font* font, StringView characterCluster, const FontDescription& description, ResolvedEmojiPolicy resolvedEmojiPolicy, IsForPlatformFont isForPlatformFont)
{
    auto fontAddResult = m_characterFallbackMaps.add(font, CharacterFallbackMap());

    auto key = CharacterFallbackMapKey { description.computedLocale(), characterCluster.toString(), isForPlatformFont != IsForPlatformFont::No, resolvedEmojiPolicy };
    return fontAddResult.iterator->value.ensure(WTFMove(key), [&] {
        StringBuilder stringBuilder;
        stringBuilder.append(FontCascade::normalizeSpaces(characterCluster));

        // FIXME: Is this the right place to add the variation selectors?
        // Should this be done in platform-specific code instead?
        // The fact that Core Text accepts this information in the form of variation selectors
        // seems like a platform-specific quirk.
        // However, if we do this later in platform-specific code, we'd have to reallocate
        // the array and copy its contents, which seems wasteful.
        switch (resolvedEmojiPolicy) {
        case ResolvedEmojiPolicy::NoPreference:
            break;
        case ResolvedEmojiPolicy::RequireText:
            stringBuilder.append(textVariationSelector);
            break;
        case ResolvedEmojiPolicy::RequireEmoji:
            stringBuilder.append(emojiVariationSelector);
            break;
        }

        auto fallbackFont = FontCache::forCurrentThread().systemFallbackForCharacterCluster(description, *font, isForPlatformFont, FontCache::PreferColoredFont::No, stringBuilder).get();
        if (fallbackFont)
            fallbackFont->setIsUsedInSystemFallbackFontCache();
        return fallbackFont;
    }).iterator->value;
}

void SystemFallbackFontCache::remove(Font* font)
{
    m_characterFallbackMaps.remove(font);

    if (!font->isUsedInSystemFallbackFontCache())
        return;

    for (auto& characterMap : m_characterFallbackMaps.values()) {
        Vector<CharacterFallbackMapKey, 512> toRemove;
        for (auto& entry : characterMap) {
            if (entry.value == font)
                toRemove.append(entry.key);
        }
        for (auto& key : toRemove)
            characterMap.remove(key);
    }
}

} // namespace WebCore
