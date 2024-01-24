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
#include <wtf/HashMap.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class LegacyInlineTextBox;

namespace InlineDisplay {
struct Box;
}

class GlyphDisplayListCacheEntry : public RefCounted<GlyphDisplayListCacheEntry>, public CanMakeSingleThreadWeakPtr<GlyphDisplayListCacheEntry> {
    WTF_MAKE_FAST_ALLOCATED;
    friend struct GlyphDisplayListCacheKeyTranslator;
    friend void add(Hasher&, const GlyphDisplayListCacheEntry&);
public:
    static Ref<GlyphDisplayListCacheEntry> create(std::unique_ptr<DisplayList::DisplayList>&& displayList, const TextRun& textRun, const FontCascade& font, GraphicsContext& context)
    {
        return adoptRef(*new GlyphDisplayListCacheEntry(WTFMove(displayList), textRun, font, context));
    }

    ~GlyphDisplayListCacheEntry();

    bool operator==(const GlyphDisplayListCacheEntry& other) const
    {
        return m_textRun == other.m_textRun
            && m_scaleFactor == other.m_scaleFactor
            && m_fontCascadeGeneration == other.m_fontCascadeGeneration
            && m_shouldSubpixelQuantizeFont == other.m_shouldSubpixelQuantizeFont;
    }

    DisplayList::DisplayList& displayList() { return *m_displayList.get(); }

private:
    GlyphDisplayListCacheEntry(std::unique_ptr<DisplayList::DisplayList>&& displayList, const TextRun& textRun, const FontCascade& font, GraphicsContext& context)
        : m_displayList(WTFMove(displayList))
        , m_textRun(textRun.isolatedCopy())
        , m_scaleFactor(context.scaleFactor())
        , m_fontCascadeGeneration(font.generation())
        , m_shouldSubpixelQuantizeFont(context.shouldSubpixelQuantizeFonts())
    {
        ASSERT(m_displayList.get());
    }

    std::unique_ptr<DisplayList::DisplayList> m_displayList;

    TextRun m_textRun;
    FloatSize m_scaleFactor;
    unsigned m_fontCascadeGeneration;
    bool m_shouldSubpixelQuantizeFont;
};

inline void add(Hasher& hasher, const GlyphDisplayListCacheEntry& entry)
{
    add(hasher, entry.m_textRun, entry.m_scaleFactor.width(), entry.m_scaleFactor.height(), entry.m_fontCascadeGeneration, entry.m_shouldSubpixelQuantizeFont);
}

struct GlyphDisplayListCacheEntryHash {
    static unsigned hash(const GlyphDisplayListCacheEntry* entry) { return computeHash(*entry); }
    static unsigned hash(const SingleThreadWeakRef<GlyphDisplayListCacheEntry>& entry) { return computeHash(entry.get()); }
    static bool equal(const SingleThreadWeakRef<GlyphDisplayListCacheEntry>& a, const SingleThreadWeakRef<GlyphDisplayListCacheEntry>& b) { return a.ptr() == b.ptr(); }
    static bool equal(const SingleThreadWeakRef<GlyphDisplayListCacheEntry>& a, const GlyphDisplayListCacheEntry* b) { return a.ptr() == b; }
    static bool equal(const GlyphDisplayListCacheEntry* a, const SingleThreadWeakRef<GlyphDisplayListCacheEntry>& b) { return a == b.ptr(); }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

class GlyphDisplayListCache {
    WTF_MAKE_FAST_ALLOCATED;
    friend class GlyphDisplayListCacheEntry;
public:
    GlyphDisplayListCache() = default;

    static GlyphDisplayListCache& singleton();

    DisplayList::DisplayList* get(const LegacyInlineTextBox& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun);
    DisplayList::DisplayList* get(const InlineDisplay::Box& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun);

    DisplayList::DisplayList* getIfExists(const LegacyInlineTextBox& run) { return getIfExists(&run); }
    DisplayList::DisplayList* getIfExists(const InlineDisplay::Box& run) { return getIfExists(&run); }

    void remove(const LegacyInlineTextBox& run) { remove(&run); }
    void remove(const InlineDisplay::Box& run) { remove(&run); }

    void clear();
    unsigned size() const;

private:
    static bool canShareDisplayList(const DisplayList::DisplayList&);

    template <typename LayoutRun> DisplayList::DisplayList* getDisplayList(const LayoutRun*, const FontCascade&, GraphicsContext&, const TextRun&);
    DisplayList::DisplayList* getIfExists(const void* run);
    void remove(const void* run);

    HashMap<const void*, Ref<GlyphDisplayListCacheEntry>> m_entriesForLayoutRun;
    HashSet<SingleThreadWeakRef<GlyphDisplayListCacheEntry>> m_entries;
};

} // namespace WebCore

namespace WTF {

template<> struct DefaultHash<SingleThreadWeakRef<WebCore::GlyphDisplayListCacheEntry>> : WebCore::GlyphDisplayListCacheEntryHash { };

} // namespace WTF
