/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OutlineDisplayListCache.h"

#include "DisplayListItems.h"
#include "PaintInfo.h"
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OutlineDisplayListCacheEntry);
WTF_MAKE_TZONE_ALLOCATED_IMPL(OutlineDisplayListCache);

OutlineDisplayListCache& OutlineDisplayListCache::singleton()
{
    static NeverDestroyed<OutlineDisplayListCache> cache;
    return cache;
}

RefPtr<const DisplayList::DisplayList> OutlineDisplayListCache::getIfExists(const RenderElement& renderer)
{
    if (auto entry = m_entriesForRenderer.get(&renderer))
        return &entry->displayList();
    return nullptr;
}

void OutlineDisplayListCache::store(const RenderElement& renderer, Ref<const DisplayList::DisplayList> displayList)
{
    Ref entry = OutlineDisplayListCacheEntry::create(WTFMove(displayList));
    m_entriesForRenderer.add(&renderer, WTFMove(entry));
}

void OutlineDisplayListCache::remove(RenderElement* renderer)
{
    m_entriesForRenderer.remove(renderer);
}

} // namespace WebCore
