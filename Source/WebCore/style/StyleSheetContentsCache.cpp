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
#include "StyleSheetContentsCache.h"

#include "StyleSheetContents.h"

namespace WebCore {
namespace Style {

StyleSheetContentsCache::StyleSheetContentsCache() = default;

StyleSheetContentsCache& StyleSheetContentsCache::singleton()
{
    static NeverDestroyed<StyleSheetContentsCache> cache;
    return cache.get();
}

RefPtr<StyleSheetContents> StyleSheetContentsCache::get(const Key& key)
{
    return m_cache.get(key);
}

void StyleSheetContentsCache::add(Key&& key, Ref<StyleSheetContents> contents)
{
    ASSERT(contents->isCacheable());

    m_cache.add(WTFMove(key), contents);
    contents->addedToMemoryCache();

    static constexpr auto maximumCacheSize = 256;
    if (m_cache.size() > maximumCacheSize) {
        auto toRemove = m_cache.random();
        toRemove->value->removedFromMemoryCache();
        m_cache.remove(toRemove);
    }
}

void StyleSheetContentsCache::clear()
{
    m_cache.clear();
}

}
}
