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
#include "GlyphDisplayListCache.h"

#include "DisplayListItems.h"
#include "InlineDisplayBox.h"
#include "PaintInfo.h"
#include "RenderLayer.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GlyphDisplayListCacheEntry);
WTF_MAKE_TZONE_ALLOCATED_IMPL(GlyphDisplayListCache);

struct GlyphDisplayListCacheKey {
    GlyphDisplayListCacheKey(const TextRun& textRun, const FontCascade& font, const GraphicsContext& context)
        : textRun(textRun)
        , scaleFactor(context.scaleFactor())
        , fontCascadeGeneration(font.generation())
        , shouldSubpixelQuantizeFonts(context.shouldSubpixelQuantizeFonts())
    {
    }

    const TextRun& textRun;
    const FloatSize scaleFactor;
    const unsigned fontCascadeGeneration;
    const bool shouldSubpixelQuantizeFonts;
};

static void LEGACY_NODELETE add(Hasher& hasher, const GlyphDisplayListCacheKey& key)
{
    add(hasher, key.textRun, key.scaleFactor.width(), key.scaleFactor.height(), key.fontCascadeGeneration, key.shouldSubpixelQuantizeFonts);
}

struct GlyphDisplayListCacheKeyTranslator {
    static unsigned LEGACY_NODELETE hash(const GlyphDisplayListCacheKey& key)
    {
        return computeHash(key);
    }

    static bool LEGACY_NODELETE equal(const WeakPtr<GlyphDisplayListCacheEntry, SingleThreadWeakPtrImpl>& entryRef, const GlyphDisplayListCacheKey& key)
    {
        if (!entryRef)
            return false;
        auto& entry = *entryRef;
        return entry.m_textRun == key.textRun
            && entry.m_scaleFactor == key.scaleFactor
            && entry.m_fontCascadeGeneration == key.fontCascadeGeneration
            && entry.m_shouldSubpixelQuantizeFont == key.shouldSubpixelQuantizeFonts;
    }
};

GlyphDisplayListCache& GlyphDisplayListCache::singleton()
{
    static NeverDestroyed<GlyphDisplayListCache> cache;
    return cache;
}

void GlyphDisplayListCache::clear()
{
    m_entriesForBox.clear();
    m_entries.clear();
}

unsigned GlyphDisplayListCache::size() const
{
    return m_entries.size();
}

RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::get(const InlineDisplay::Box& box, const FontCascade& font, GraphicsContext& context, const TextRun& textRun, const PaintInfo& paintInfo)
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_entries.isEmpty()) {
            LOG(MemoryPressure, "GlyphDisplayListCache::%s - Under memory pressure - size: %d", __FUNCTION__, size());
            clear();
        }
        return nullptr;
    }

    if (font.isLoadingCustomFonts() || !font.fonts())
        return nullptr;

    if (RefPtr result = getIfExists(box))
        return result;

    bool isFrequentlyPainted = paintInfo.enclosingSelfPaintingLayer()->paintingFrequently();
    if (!isFrequentlyPainted && !m_forceUseGlyphDisplayListForTesting) {
        // Now, all cache entries are actively used.
        constexpr size_t maximumCacheSize = 2048;
        if (m_entriesForBox.size() >= maximumCacheSize)
            return nullptr;
    }

    if (auto iterator = m_entries.findIf([&](auto& weakEntry) {
        return GlyphDisplayListCacheKeyTranslator::equal(weakEntry, GlyphDisplayListCacheKey { textRun, font, context });
    }); iterator != m_entries.end()) {
        RefPtr<GlyphDisplayListCacheEntry> entry = iterator->get();
        if (!entry)
            m_entries.remove(iterator);
        else {
            const_cast<InlineDisplay::Box&>(box).setIsInGlyphDisplayListCache();
            RefPtr result = &entry->displayList();
            m_entriesForBox.add(&box, entry.releaseNonNull());
            return result;
        }
    }

    if (RefPtr displayList = font.displayListForTextRun(context, textRun)) {
        Ref entry = GlyphDisplayListCacheEntry::create(displayList.releaseNonNull(), textRun, font, context);
        Ref result = entry->displayList();
        if (canShareDisplayList(result)) {
            m_entries.append(entry.get());
            if (m_entries.size() > s_maxDeduplicationCacheSize)
                m_entries.removeFirst();
        }
        const_cast<InlineDisplay::Box&>(box).setIsInGlyphDisplayListCache();
        m_entriesForBox.add(&box, WTF::move(entry));
        return result;
    }

    return nullptr;
}

RefPtr<const DisplayList::DisplayList> GlyphDisplayListCache::getIfExists(const InlineDisplay::Box& box)
{
    if (!box.isInGlyphDisplayListCache())
        return nullptr;
    if (auto entry = m_entriesForBox.get(&box))
        return &entry->displayList();
    return nullptr;
}

bool GlyphDisplayListCache::canShareDisplayList(const DisplayList::DisplayList& displayList)
{
    for (auto& item : displayList.items()) {
        if (!(std::holds_alternative<DisplayList::Translate>(item)
            || std::holds_alternative<DisplayList::Scale>(item)
            || std::holds_alternative<DisplayList::ConcatenateCTM>(item)
            || std::holds_alternative<DisplayList::DrawGlyphs>(item)
            || std::holds_alternative<DisplayList::DrawImageBuffer>(item)
            || std::holds_alternative<DisplayList::DrawNativeImage>(item)
#if USE(SKIA)
            || std::holds_alternative<DisplayList::DrawTextBlob>(item)
#endif
            || std::holds_alternative<DisplayList::BeginTransparencyLayer>(item)
            || std::holds_alternative<DisplayList::BeginTransparencyLayerWithCompositeMode>(item)
            || std::holds_alternative<DisplayList::EndTransparencyLayer>(item)))
            return false;
    }
    return true;
}


} // namespace WebCore
