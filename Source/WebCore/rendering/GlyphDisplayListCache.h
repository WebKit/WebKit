/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "FontCascade.h"
#include "InMemoryDisplayList.h"
#include "Logging.h"
#include "TextRun.h"
#include "TextRunHash.h"
#include <wtf/HashMap.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class LegacyInlineTextBox;

namespace InlineDisplay {
struct Box;
}

class GlyphDisplayListCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GlyphDisplayListCache() = default;

    DisplayList::DisplayList* get(const LegacyInlineTextBox& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun) { return get(&run, font, context, textRun); }
    DisplayList::DisplayList* get(const InlineDisplay::Box& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun) { return get(&run, font, context, textRun); }

    DisplayList::DisplayList* getIfExists(const LegacyInlineTextBox& run) { return getIfExists(&run); }
    DisplayList::DisplayList* getIfExists(const InlineDisplay::Box& run) { return getIfExists(&run); }

    void remove(const LegacyInlineTextBox& run) { remove(&run); }
    void remove(const InlineDisplay::Box& run) { remove(&run); }

    void clear()
    {
        m_entriesForLayoutRun.clear();
        m_entriesForTextRun.clear();
    }

    unsigned size() const
    {
        return m_entriesForTextRun.size();
    }

    size_t sizeInBytes() const
    {
        size_t sizeInBytes = 0;
        for (auto& entry : m_entriesForTextRun)
            sizeInBytes += entry.value->displayList().sizeInBytes();
        return sizeInBytes;
    }

private:
    class Entry : public RefCounted<Entry>, public CanMakeWeakPtr<Entry> {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        static Ref<Entry> create(GraphicsContext& context, const FontCascade& font, std::unique_ptr<DisplayList::InMemoryDisplayList>&& displayList)
        {
            return adoptRef(*new Entry(context, font, WTFMove(displayList)));
        }

        DisplayList::InMemoryDisplayList& displayList() { return *m_displayList.get(); }
        bool canUseSharedDisplayList(GraphicsContext& context, const FontCascade& font) const { return font == m_font && context.scaleFactor() == m_scaleFactor && m_shouldSubpixelQuantizeFont == context.shouldSubpixelQuantizeFonts(); }

    private:
        Entry(GraphicsContext& context, const FontCascade& font, std::unique_ptr<DisplayList::InMemoryDisplayList>&& displayList)
            : m_displayList(WTFMove(displayList))
            , m_font(font)
            , m_scaleFactor(context.scaleFactor())
            , m_shouldSubpixelQuantizeFont(context.shouldSubpixelQuantizeFonts())
        {
            ASSERT(m_displayList.get());
        }

        std::unique_ptr<DisplayList::InMemoryDisplayList> m_displayList;
        FontCascade m_font;
        FloatSize m_scaleFactor;
        bool m_shouldSubpixelQuantizeFont;
    };

    DisplayList::DisplayList* get(const void* run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun)
    {
        if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
            if (!m_entriesForTextRun.isEmpty()) {
                LOG(MemoryPressure, "GlyphDisplayListCache::%s - Under memory pressure - size: %d - sizeInBytes: %ld", __FUNCTION__, size(), sizeInBytes());
                clear();
            }
            return nullptr;
        }

        if (auto entry = m_entriesForLayoutRun.get(run))
            return &entry->displayList();

        if (auto entry = m_entriesForTextRun.get(textRun)) {
            if (entry->canUseSharedDisplayList(context, font))
                return &m_entriesForLayoutRun.add(run, Ref { *entry }).iterator->value->displayList();
        }

        if (auto displayList = font.displayListForTextRun(context, textRun)) {
            auto entry = Entry::create(context, font, WTFMove(displayList));
            if (canShareDisplayList(entry->displayList()))
                m_entriesForTextRun.add(textRun.isolatedCopy(), entry);
            return &m_entriesForLayoutRun.add(run, WTFMove(entry)).iterator->value->displayList();
        }

        return nullptr;
    }

    DisplayList::DisplayList* getIfExists(const void* run)
    {
        if (auto entry = m_entriesForLayoutRun.get(run))
            return &entry->displayList();
        return nullptr;
    }

    void remove(const void* run)
    {
        m_entriesForLayoutRun.remove(&run);
    }

    static bool canShareDisplayList(const DisplayList::InMemoryDisplayList&);

    HashMap<TextRun, WeakPtr<Entry>> m_entriesForTextRun;
    HashMap<const void*, Ref<Entry>> m_entriesForLayoutRun;
};

}
