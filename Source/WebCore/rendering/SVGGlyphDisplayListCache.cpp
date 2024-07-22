/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "SVGGlyphDisplayListCache.h"

#include "DisplayListItems.h"
#include "InlineDisplayBox.h"
#include "LegacyInlineTextBox.h"
#include "PaintInfo.h"
#include "RenderLayer.h"
#include "RenderStyleInlines.h"
#include "SVGTextFragment.h"
#include "svg/SVGTextFragment.h"

namespace WebCore {

SVGGlyphDisplayListCache& SVGGlyphDisplayListCache::singleton()
{
    static NeverDestroyed<SVGGlyphDisplayListCache> cache;
    return cache;
}

void SVGGlyphDisplayListCache::clear()
{
    m_entriesForLayoutRun.clear();
}

unsigned SVGGlyphDisplayListCache::size() const
{
    return m_entriesForLayoutRun.size();
}

DisplayList::DisplayList* SVGGlyphDisplayListCache::getDisplayList(SVGTextFragment& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun, const PaintInfo& paintInfo)
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_entriesForLayoutRun.isEmpty()) {
            LOG(MemoryPressure, "SVGGlyphDisplayListCache::%s - Under memory pressure - size: %d", __FUNCTION__, size());
            clear();
        }
        return nullptr;
    }

    if (font.isLoadingCustomFonts() || !font.fonts())
        return nullptr;

    if (auto* result = getIfExists(run))
        return result;

    bool isFrequentlyPainted = paintInfo.enclosingSelfPaintingLayer()->paintingFrequently();
    if (!isFrequentlyPainted && !m_forceUseSVGGlyphDisplayListForTesting) {
        // Now, all cache entries are actively used.
        constexpr size_t maximumCacheSize = 2048;
        if (m_entriesForLayoutRun.size() >= maximumCacheSize)
            return nullptr;
    }

    if (auto displayList = font.displayListForTextRun(context, textRun)) {
        run.setIsInSVGGlyphDisplayListCache();
        auto result = m_entriesForLayoutRun.add(&run, WTFMove(displayList));
        return result.iterator->value.get();
    }

    return nullptr;
}

DisplayList::DisplayList* SVGGlyphDisplayListCache::getIfExists(const SVGTextFragment& run)
{
    if (!run.isInSVGGlyphDisplayListCache())
        return nullptr;
    if (auto entry = m_entriesForLayoutRun.get(&run))
        return entry;
    return nullptr;
}

void SVGGlyphDisplayListCache::remove(const SVGTextFragment& run)
{
    m_entriesForLayoutRun.remove(&run);
}
} // namespace WebCore
