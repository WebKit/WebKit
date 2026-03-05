/*
 * Copyright (C) 2018-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "DisplayList.h"
#include "FloatSizeHash.h"
#include "FontCascade.h"
#include "Logging.h"
#include "TextRun.h"
#include "TextRunHash.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class LegacyInlineTextBox;
struct PaintInfo;

namespace InlineDisplay {
struct Box;
}

class GlyphDisplayListCacheEntry : public RefCounted<GlyphDisplayListCacheEntry>, public CanMakeSingleThreadWeakPtr<GlyphDisplayListCacheEntry> {
    WTF_MAKE_TZONE_ALLOCATED(GlyphDisplayListCacheEntry);
    friend struct GlyphDisplayListCacheKeyTranslator;
    friend void add(Hasher&, const GlyphDisplayListCacheEntry&);
public:
    static Ref<GlyphDisplayListCacheEntry> create(Ref<const DisplayList::DisplayList>&& displayList, const TextRun& textRun, const FontCascade& font, GraphicsContext& context)
    {
        return adoptRef(*new GlyphDisplayListCacheEntry(WTF::move(displayList), textRun, font, context));
    }

    bool operator==(const GlyphDisplayListCacheEntry& other) const
    {
        return m_textRun == other.m_textRun
            && m_scaleFactor == other.m_scaleFactor
            && m_fontCascadeGeneration == other.m_fontCascadeGeneration
            && m_shouldSubpixelQuantizeFont == other.m_shouldSubpixelQuantizeFont;
    }

    const DisplayList::DisplayList& displayList() const { return m_displayList.get(); }

private:
    GlyphDisplayListCacheEntry(Ref<const DisplayList::DisplayList>&& displayList, const TextRun& textRun, const FontCascade& font, GraphicsContext& context)
        : m_displayList(WTF::move(displayList))
        , m_textRun(textRun.isolatedCopy())
        , m_scaleFactor(context.scaleFactor())
        , m_fontCascadeGeneration(font.generation())
        , m_shouldSubpixelQuantizeFont(context.shouldSubpixelQuantizeFonts())
    {
    }

    const Ref<const DisplayList::DisplayList> m_displayList;

    TextRun m_textRun;
    FloatSize m_scaleFactor;
    unsigned m_fontCascadeGeneration;
    bool m_shouldSubpixelQuantizeFont;
};

inline void add(Hasher& hasher, const GlyphDisplayListCacheEntry& entry)
{
    add(hasher, entry.m_textRun, entry.m_scaleFactor.width(), entry.m_scaleFactor.height(), entry.m_fontCascadeGeneration, entry.m_shouldSubpixelQuantizeFont);
}

class GlyphDisplayListCache {
    WTF_MAKE_TZONE_ALLOCATED(GlyphDisplayListCache);
    friend class GlyphDisplayListCacheEntry;
public:
    GlyphDisplayListCache() = default;

    static GlyphDisplayListCache& singleton();

    RefPtr<const DisplayList::DisplayList> get(const LegacyInlineTextBox&, const FontCascade&, GraphicsContext&, const TextRun&, const PaintInfo&);
    RefPtr<const DisplayList::DisplayList> get(const InlineDisplay::Box&, const FontCascade&, GraphicsContext&, const TextRun&, const PaintInfo&);

    RefPtr<const DisplayList::DisplayList> getIfExists(const LegacyInlineTextBox&);
    RefPtr<const DisplayList::DisplayList> getIfExists(const InlineDisplay::Box&);

    void remove(const LegacyInlineTextBox& run) { remove(&run); }
    void remove(const InlineDisplay::Box& run) { remove(&run); }

    void clear();
    unsigned NODELETE size() const;

    void setForceUseGlyphDisplayListForTesting(bool flag)
    {
        m_forceUseGlyphDisplayListForTesting = flag;
    }

private:
    static bool canShareDisplayList(const DisplayList::DisplayList&);

    template<typename LayoutRun>
    RefPtr<const DisplayList::DisplayList> getDisplayList(const LayoutRun&, const FontCascade&, GraphicsContext&, const TextRun&, const PaintInfo&);
    template<typename LayoutRun>
    RefPtr<const DisplayList::DisplayList> getIfExistsImpl(const LayoutRun&);
    void remove(const void* run);

    HashMap<const void*, Ref<GlyphDisplayListCacheEntry>> m_entriesForLayoutRun;
    Deque<WeakPtr<GlyphDisplayListCacheEntry, SingleThreadWeakPtrImpl>> m_entries;
    bool m_forceUseGlyphDisplayListForTesting { false };
    static constexpr unsigned s_maxDeduplicationCacheSize { 20 };
};

} // namespace WebCore
