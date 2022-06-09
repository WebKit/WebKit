/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "FontCascade.h"
#include "InMemoryDisplayList.h"
#include "Logging.h"
#include "TextRun.h"
#include <wtf/HashMap.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

template<typename LayoutRun>
class GlyphDisplayListCache {
public:
    GlyphDisplayListCache() = default;

    static GlyphDisplayListCache& singleton()
    {
        static NeverDestroyed<GlyphDisplayListCache> cache;
        return cache;
    }

    DisplayList::DisplayList* get(const LayoutRun& run, const FontCascade& font, GraphicsContext& context, const TextRun& textRun)
    {
        if (MemoryPressureHandler::singleton().isUnderMemoryPressure()) {
            if (!m_glyphRunMap.isEmpty()) {
                LOG(MemoryPressure, "GlyphDisplayListCache::%s - Under memory pressure - size: %d - sizeInBytes: %ld", __FUNCTION__, size(), sizeInBytes());
                clear();
            }
            return nullptr;
        }

        if (auto displayList = m_glyphRunMap.get(&run))
            return displayList;

        if (auto displayList = font.displayListForTextRun(context, textRun))
            return m_glyphRunMap.add(&run, WTFMove(displayList)).iterator->value.get();

        return nullptr;
    }

    void remove(const LayoutRun& run)
    {
        m_glyphRunMap.remove(&run);
    }

    void clear()
    {
        m_glyphRunMap.clear();
    }

    unsigned size() const
    {
        return m_glyphRunMap.size();
    }
    
    size_t sizeInBytes() const
    {
        size_t sizeInBytes = 0;
        for (const auto& entry : m_glyphRunMap)
            sizeInBytes += entry.value->sizeInBytes();
        return sizeInBytes;
    }
    
private:
    using GlyphRunMap = HashMap<const LayoutRun*, std::unique_ptr<DisplayList::InMemoryDisplayList>>;
    GlyphRunMap m_glyphRunMap;
};
    
}
