/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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
#include "FontCascadeCache.h"

#include "FontCache.h"
#include "FontCascadeDescription.h"

namespace WebCore {

FontFamilyName::FontFamilyName() = default;

FontFamilyName::FontFamilyName(const AtomString& name)
    : m_name { name }
{
}

const AtomString& FontFamilyName::string() const
{
    return m_name;
}

void add(Hasher& hasher, const FontFamilyName& name)
{
    // FIXME: Would be better to hash the characters in the name instead of hashing a hash.
    if (!name.string().isNull())
        add(hasher, FontCascadeDescription::familyNameHash(name.string()));
}

bool operator==(const FontFamilyName& a, const FontFamilyName& b)
{
    return (a.string().isNull() || b.string().isNull()) ? a.string() == b.string() : FontCascadeDescription::familyNamesAreEqual(a.string(), b.string());
}

FontCascadeCache& FontCascadeCache::forCurrentThread()
{
    return FontCache::forCurrentThread().fontCascadeCache();
}

void FontCascadeCache::invalidate()
{
    m_entries.clear();
}

void FontCascadeCache::clearWidthCaches()
{
    for (auto& value : m_entries.values())
        value->fonts.get().widthCache().clear();
}

void FontCascadeCache::pruneUnreferencedEntries()
{
    m_entries.removeIf([](auto& entry) {
        return entry.value->fonts.get().hasOneRef();
    });
}

void FontCascadeCache::pruneSystemFallbackFonts()
{
    for (auto& entry : m_entries.values())
        entry->fonts->pruneSystemFallbacks();
}

static FontCascadeCacheKey makeFontCascadeCacheKey(const FontCascadeDescription& description, FontSelector* fontSelector)
{
    unsigned familyCount = description.familyCount();
    return FontCascadeCacheKey {
        FontDescriptionKey(description),
        Vector<FontFamilyName, 3>(familyCount, [&](size_t i) { return description.familyAt(i); }),
        fontSelector ? fontSelector->uniqueId() : 0,
        fontSelector ? fontSelector->version() : 0
    };
}

Ref<FontCascadeFonts> FontCascadeCache::retrieveOrAddCachedFonts(const FontCascadeDescription& fontDescription, RefPtr<FontSelector>&& fontSelector)
{
    auto key = makeFontCascadeCacheKey(fontDescription, fontSelector.get());
    auto addResult = m_entries.add(key, nullptr);
    if (!addResult.isNewEntry)
        return addResult.iterator->value->fonts.get();

    auto& newEntry = addResult.iterator->value;
    newEntry = makeUnique<FontCascadeCacheEntry>(FontCascadeCacheEntry { WTFMove(key), FontCascadeFonts::create(WTFMove(fontSelector)) });
    Ref<FontCascadeFonts> glyphs = newEntry->fonts.get();

    static constexpr unsigned unreferencedPruneInterval = 50;
    static constexpr int maximumEntries = 400;
    static unsigned pruneCounter;
    // Referenced FontCascadeFonts would exist anyway so pruning them saves little memory.
    if (!(++pruneCounter % unreferencedPruneInterval))
        pruneUnreferencedEntries();
    // Prevent pathological growth.
    if (m_entries.size() > maximumEntries)
        m_entries.remove(m_entries.random());
    return glyphs;
}

} // namespace WebCore
