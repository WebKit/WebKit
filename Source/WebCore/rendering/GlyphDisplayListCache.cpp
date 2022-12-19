/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "DisplayListIterator.h"

namespace WebCore {

static bool canShareDisplayListWithItem(DisplayList::ItemType itemType)
{
    using DisplayList::ItemType;

    switch (itemType) {
    case ItemType::Translate:
    case ItemType::Scale:
    case ItemType::ConcatenateCTM:
    case ItemType::DrawDecomposedGlyphs:
    case ItemType::DrawImageBuffer:
    case ItemType::DrawNativeImage:
    case ItemType::BeginTransparencyLayer:
    case ItemType::EndTransparencyLayer:
        return true;
    case ItemType::Save:
    case ItemType::Restore:
    case ItemType::Rotate:
    case ItemType::SetCTM:
    case ItemType::SetInlineFillColor:
    case ItemType::SetInlineStrokeColor:
    case ItemType::SetStrokeThickness:
    case ItemType::SetState:
    case ItemType::SetLineCap:
    case ItemType::SetLineDash:
    case ItemType::SetLineJoin:
    case ItemType::SetMiterLimit:
    case ItemType::ClearShadow:
    case ItemType::Clip:
    case ItemType::ClipOut:
    case ItemType::ClipToImageBuffer:
    case ItemType::ClipOutToPath:
    case ItemType::ClipPath:
    case ItemType::DrawControlPart:
    case ItemType::DrawFilteredImageBuffer:
    case ItemType::DrawSystemImage:
    case ItemType::DrawGlyphs:
    case ItemType::DrawPattern:
    case ItemType::DrawRect:
    case ItemType::DrawLine:
    case ItemType::DrawLinesForText:
    case ItemType::DrawDotsForDocumentMarker:
    case ItemType::DrawEllipse:
    case ItemType::DrawPath:
    case ItemType::DrawFocusRingPath:
    case ItemType::DrawFocusRingRects:
    case ItemType::FillRect:
    case ItemType::FillRectWithColor:
    case ItemType::FillRectWithGradient:
    case ItemType::FillCompositedRect:
    case ItemType::FillRoundedRect:
    case ItemType::FillRectWithRoundedHole:
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::FillLine:
    case ItemType::FillArc:
    case ItemType::FillQuadCurve:
    case ItemType::FillBezierCurve:
#endif
    case ItemType::FillPath:
    case ItemType::FillEllipse:
#if ENABLE(VIDEO)
    case ItemType::PaintFrameForMedia:
#endif
    case ItemType::StrokeRect:
    case ItemType::StrokeLine:
#if ENABLE(INLINE_PATH_DATA)
    case ItemType::StrokeArc:
    case ItemType::StrokeQuadCurve:
    case ItemType::StrokeBezierCurve:
#endif
    case ItemType::StrokePath:
    case ItemType::StrokeEllipse:
    case ItemType::ClearRect:
#if USE(CG)
    case ItemType::ApplyStrokePattern:
    case ItemType::ApplyFillPattern:
#endif
    case ItemType::ApplyDeviceScaleFactor:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

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

    static bool equal(GlyphDisplayListCacheEntry* entry, const GlyphDisplayListCacheKey& key)
    {
        return entry->m_textRun == key.textRun
            && entry->m_scaleFactor == key.context.scaleFactor()
            && entry->m_fontCascadeGeneration == key.font.generation()
            && entry->m_shouldSubpixelQuantizeFont == key.context.shouldSubpixelQuantizeFonts();
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

size_t GlyphDisplayListCache::sizeInBytes() const
{
    size_t sizeInBytes = 0;
    for (auto entry : m_entries)
        sizeInBytes += entry->displayList().sizeInBytes();
    return sizeInBytes;
}

DisplayList::DisplayList* GlyphDisplayListCache::get(const void* run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun)
{
    if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
        if (!m_entries.isEmpty()) {
            LOG(MemoryPressure, "GlyphDisplayListCache::%s - Under memory pressure - size: %d - sizeInBytes: %ld", __FUNCTION__, size(), sizeInBytes());
            clear();
        }
        return nullptr;
    }

    if (font.isLoadingCustomFonts() || !font.fonts())
        return nullptr;

    if (auto entry = m_entriesForLayoutRun.get(run))
        return &entry->displayList();

    if (auto entry = m_entries.find<GlyphDisplayListCacheKeyTranslator>(GlyphDisplayListCacheKey { textRun, font, context }); entry != m_entries.end())
        return &m_entriesForLayoutRun.add(run, Ref { **entry }).iterator->value->displayList();

    if (auto displayList = font.displayListForTextRun(context, textRun)) {
        auto entry = GlyphDisplayListCacheEntry::create(WTFMove(displayList), textRun, font, context);
        if (canShareDisplayList(entry->displayList()))
            m_entries.add(entry.ptr());
        return &m_entriesForLayoutRun.add(run, WTFMove(entry)).iterator->value->displayList();
    }

    return nullptr;
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

bool GlyphDisplayListCache::canShareDisplayList(const DisplayList::InMemoryDisplayList& displayList)
{
    for (auto displayListItem : displayList) {
        if (!canShareDisplayListWithItem(displayListItem.value().item.type()))
            return false;
    }
    return true;
}

GlyphDisplayListCacheEntry::~GlyphDisplayListCacheEntry()
{
    GlyphDisplayListCache::singleton().m_entries.remove(this);
}

} // namespace WebCore
