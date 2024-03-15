/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "CanvasGlyphDisplayListCache.h"

#include "DisplayListItems.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

CanvasGlyphDisplayListCache& CanvasGlyphDisplayListCache::singleton()
{
    static NeverDestroyed<CanvasGlyphDisplayListCache> cache;
    return cache;
}

void CanvasGlyphDisplayListCache::clear()
{
    m_entries.clear();
}

unsigned CanvasGlyphDisplayListCache::size() const
{
    return m_entries.size();
}

DisplayList::DisplayList* CanvasGlyphDisplayListCache::getDisplayList(const FontCascade& font, GraphicsContext& context, const TextRun& textRun)
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_entries.isEmpty()) {
            LOG(MemoryPressure, "CanvasGlyphDisplayListCache::%s - Under memory pressure - size: %d", __FUNCTION__, size());
            clear();
        }
        return nullptr;
    }

    if (font.isLoadingCustomFonts() || !font.fonts())
        return nullptr;

    if (auto entry = m_entries.find(CanvasGlyphDisplayListCacheKey { textRun, font, context }); entry != m_entries.end())
        return entry->value.get();

    if (auto displayList = font.displayListForTextRun(context, textRun)) {
        CanvasGlyphDisplayListCacheKey key { textRun, font, context };
        if (!canShareDisplayList(*displayList))
            return nullptr;

        auto addResult = m_entries.add(key, WTFMove(displayList));
        return addResult.iterator->value.get();
    }

    return nullptr;
}

bool CanvasGlyphDisplayListCache::canShareDisplayList(const DisplayList::DisplayList& displayList)
{
    for (auto& item : displayList.items()) {
        if (!(std::holds_alternative<DisplayList::Translate>(item)
            || std::holds_alternative<DisplayList::Scale>(item)
            || std::holds_alternative<DisplayList::ConcatenateCTM>(item)
            || std::holds_alternative<DisplayList::DrawDecomposedGlyphs>(item)
            || std::holds_alternative<DisplayList::DrawImageBuffer>(item)
            || std::holds_alternative<DisplayList::DrawNativeImage>(item)
            || std::holds_alternative<DisplayList::BeginTransparencyLayer>(item)
            || std::holds_alternative<DisplayList::EndTransparencyLayer>(item)))
            return false;
    }
    return true;
}
} // namespace WebCore
