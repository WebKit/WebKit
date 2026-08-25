/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextBreakingPositionCache.h"

#include "FontCache.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextBreakingPositionCache);
WTF_MAKE_TZONE_ALLOCATED_IMPL(TextBreakingPositionCache::Entry);

static constexpr size_t evictionSoftThreshold = 500000; // At this amount of content (string + breaking position list + width lists) we should start evicting
static constexpr size_t evictionHardCapMultiplier = 5; // Do not let the cache grow beyond this
static constexpr Seconds idleIntervalForEviction { 10_s };

TextBreakingPositionCache& TextBreakingPositionCache::singleton()
{
    static NeverDestroyed<TextBreakingPositionCache> cache;
    return cache.get();
}

TextBreakingPositionCache::TextBreakingPositionCache()
    : m_delayedEvictionTimer([this] { evict(); })
{
}

static size_t widthsBytes(const TextBreakingPositionCache::Entry& entry)
{
    size_t bytes = 0;
    for (auto& cascadeWidths : entry.widthsByFontCascade)
        bytes += sizeof(float) * cascadeWidths.widths.size() + sizeof(FontCascadeCacheKey);
    return bytes;
}

bool TextBreakingPositionCache::clearWidthsIfGenerationChanged()
{
    auto currentGeneration = FontCache::forCurrentThread().generation();
    if (m_widthsGeneration == currentGeneration)
        return false;
    m_widthsGeneration = currentGeneration;
    for (auto& entry : m_breakingPositionMap.values()) {
        m_cachedContentSize -= widthsBytes(*entry);
        entry->widthsByFontCascade.clear();
    }
    return true;
}

const TextBreakingPositionCache::WidthList* TextBreakingPositionCache::widths(const Entry& entry, const FontCascadeCacheKey& fontCascade)
{
    if (clearWidthsIfGenerationChanged())
        return nullptr;

    for (auto& cascadeWidths : entry.widthsByFontCascade) {
        if (cascadeWidths.fontCascade == fontCascade)
            return &cascadeWidths.widths;
    }
    return nullptr;
}

void TextBreakingPositionCache::addWidths(const Key& key, const FontCascadeCacheKey& fontCascade, WidthList&& widthList)
{
    auto iterator = m_breakingPositionMap.find(key);
    if (iterator == m_breakingPositionMap.end())
        return;
    auto& entry = *iterator->value;

    ASSERT(m_cachedContentSize >= widthsBytes(entry));
    m_cachedContentSize -= widthsBytes(entry);

    auto replaced = false;
    for (auto& cascadeWidths : entry.widthsByFontCascade) {
        if (cascadeWidths.fontCascade == fontCascade) {
            cascadeWidths.widths = WTF::move(widthList);
            replaced = true;
            break;
        }
    }
    if (!replaced)
        entry.widthsByFontCascade.append(Entry::FontCascadeWidths { fontCascade, WTF::move(widthList) });

    m_cachedContentSize += widthsBytes(entry);
}

size_t TextBreakingPositionCache::approximateEntrySizeBytes(const String& text, const Entry& entry)
{
    return text.length() + sizeof(unsigned) * entry.breakingPositions.size() + widthsBytes(entry);
}

void TextBreakingPositionCache::evict()
{
    while (m_cachedContentSize > evictionSoftThreshold && !m_breakingPositionMap.isEmpty()) {
        auto evictedEntry = m_breakingPositionMap.random();
        auto entrySize = approximateEntrySizeBytes(std::get<0>(evictedEntry->key), *evictedEntry->value);
        ASSERT(m_cachedContentSize >= entrySize);
        m_cachedContentSize -= entrySize;
        m_breakingPositionMap.remove(evictedEntry->key);
    }
}

void TextBreakingPositionCache::set(const Key& key, List&& breakingPositionList)
{
    ASSERT(!m_breakingPositionMap.contains(key));

    auto evictIfNeeded = [&] {
        if (m_cachedContentSize < evictionSoftThreshold)
            return;

        ASSERT(!m_breakingPositionMap.isEmpty());
        auto isBelowHardThreshold = m_cachedContentSize < evictionSoftThreshold * evictionHardCapMultiplier;
        if (isBelowHardThreshold) {
            m_delayedEvictionTimer.startOneShot(idleIntervalForEviction);
            return;
        }
        evict();
    };
    evictIfNeeded();

    auto entry = makeUnique<Entry>(WTF::move(breakingPositionList));
    m_cachedContentSize += approximateEntrySizeBytes(std::get<0>(key), *entry);
    m_breakingPositionMap.set(key, WTF::move(entry));
}

const TextBreakingPositionCache::Entry* TextBreakingPositionCache::get(const Key& key) const
{
    auto iterator = m_breakingPositionMap.find(key);
    if (iterator == m_breakingPositionMap.end())
        return nullptr;
    return iterator->value.get();
}

void TextBreakingPositionCache::clear()
{
    m_breakingPositionMap.clear();
    m_cachedContentSize = 0;
}

void NODELETE add(Hasher& hasher, const TextBreakingPositionContext& context)
{
    add(hasher, context.whitespaceCollapseBehavior, context.overflowWrap, context.lineBreak, context.wordBreak, context.nbspMode, context.locale);
}

}
}
