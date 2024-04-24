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
#include "LegacyInlineTextBox.h"

namespace WebCore {

struct GlyphDisplayListCacheKey {
    const TextRun& textRun;
    const FontCascade& font;
    GraphicsContext& context;
};

static void add(Hasher& hasher, const GlyphDisplayListCacheKey& key)
{
    add(hasher, key.textRun, key.context.scaleFactor().width(), key.context.scaleFactor().height(), key.font.generation(), key.context.shouldSubpixelQuantizeFonts());
}

struct GlyphDisplayListCacheKeyTranslator {
    static unsigned hash(const GlyphDisplayListCacheKey& key)
    {
        return computeHash(key);
    }

    static bool equal(const SingleThreadWeakRef<GlyphDisplayListCacheEntry>& entryRef, const GlyphDisplayListCacheKey& key)
    {
        auto& entry = entryRef.get();
        return entry.m_textRun == key.textRun
            && entry.m_scaleFactor == key.context.scaleFactor()
            && entry.m_fontCascadeGeneration == key.font.generation()
            && entry.m_shouldSubpixelQuantizeFont == key.context.shouldSubpixelQuantizeFonts();
    }
};

GlyphDisplayListCache& GlyphDisplayListCache::singleton()
{
    static NeverDestroyed<GlyphDisplayListCache> cache;
    return cache;
}

void GlyphDisplayListCache::clear()
{
    m_entriesForLayoutRun.clear();
    m_entries.clear();
}

unsigned GlyphDisplayListCache::size() const
{
    return m_entries.size();
}

template <typename LayoutRun>
DisplayList::DisplayList* GlyphDisplayListCache::getDisplayList(const LayoutRun* run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun)
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

    if (auto entry = m_entriesForLayoutRun.get(run))
        return &entry->displayList();

    if (auto entry = m_entries.find<GlyphDisplayListCacheKeyTranslator>(GlyphDisplayListCacheKey { textRun, font, context }); entry != m_entries.end()) {
        const_cast<LayoutRun*>(run)->setIsInGlyphDisplayListCache();
        return &m_entriesForLayoutRun.add(run, Ref { entry->get() }).iterator->value->displayList();
    }

    if (auto displayList = font.displayListForTextRun(context, textRun)) {
        Ref entry = GlyphDisplayListCacheEntry::create(WTFMove(displayList), textRun, font, context);
        if (canShareDisplayList(entry->displayList()))
            m_entries.add(entry.get());
        const_cast<LayoutRun*>(run)->setIsInGlyphDisplayListCache();
        return &m_entriesForLayoutRun.add(run, WTFMove(entry)).iterator->value->displayList();
    }

    return nullptr;
}

DisplayList::DisplayList* GlyphDisplayListCache::get(const LegacyInlineTextBox& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun)
{
    return getDisplayList(&run, font, context, textRun);
}

DisplayList::DisplayList* GlyphDisplayListCache::get(const InlineDisplay::Box& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun)
{
    return getDisplayList(&run, font, context, textRun);
}

DisplayList::DisplayList* GlyphDisplayListCache::getIfExists(const void* run)
{
    if (auto entry = m_entriesForLayoutRun.get(run))
        return &entry->displayList();
    return nullptr;
}

void GlyphDisplayListCache::remove(const void* run)
{
    m_entriesForLayoutRun.remove(run);
}

bool GlyphDisplayListCache::canShareDisplayList(const DisplayList::DisplayList& displayList)
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

GlyphDisplayListCacheEntry::~GlyphDisplayListCacheEntry()
{
    GlyphDisplayListCache::singleton().m_entries.remove(this);
}

} // namespace WebCore
