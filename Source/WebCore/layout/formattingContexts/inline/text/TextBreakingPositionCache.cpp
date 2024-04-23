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

#include "RenderStyle.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

static constexpr size_t evictionSoftThreshold = 500000; // At this amount of content (string + breaking position list) we should start evicting
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

void TextBreakingPositionCache::evict()
{
    while (m_cachedContentSize > evictionSoftThreshold && !m_breakingPositionMap.isEmpty()) {
        auto evictedEntry = m_breakingPositionMap.random();
        m_cachedContentSize -= (std::get<0>(evictedEntry->key).length() + 4 * evictedEntry->value.size());
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

    m_cachedContentSize += (std::get<0>(key).length() + 4 * breakingPositionList.size());
    m_breakingPositionMap.set(key, WTFMove(breakingPositionList));
}

const TextBreakingPositionCache::List* TextBreakingPositionCache::get(const Key& key) const
{
    auto iterator = m_breakingPositionMap.find(key);
    if (iterator == m_breakingPositionMap.end())
        return { };
    return &iterator->value;
}

void TextBreakingPositionCache::clear()
{
    m_breakingPositionMap.clear();
    m_cachedContentSize = 0;
}

void add(Hasher& hasher, const TextBreakingPositionContext& context)
{
    add(hasher, context.whitespace, context.overflowWrap, context.lineBreak, context.wordBreak, context.nbspMode, context.locale);
}

}
}
