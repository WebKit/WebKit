/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include <memory>
#include <wtf/GetPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class SVGGlyphDisplayListCacheKey {
    WTF_MAKE_FAST_ALLOCATED;
    friend struct SVGGlyphDisplayListCacheEntryHashTraits;
    friend void add(Hasher&, const SVGGlyphDisplayListCacheKey&);
public:
    bool operator==(const SVGGlyphDisplayListCacheKey& other) const
    {
        return m_textRun == other.m_textRun
            && m_scaleFactor == other.m_scaleFactor
            && m_fontCascadeGeneration == other.m_fontCascadeGeneration
            && m_shouldSubpixelQuantizeFont == other.m_shouldSubpixelQuantizeFont;
    }

    SVGGlyphDisplayListCacheKey()
        : m_textRun(WTF::HashTableEmptyValue)
    { }

    SVGGlyphDisplayListCacheKey(WTF::HashTableDeletedValueType)
        : m_textRun(WTF::HashTableDeletedValue)
    { }

    SVGGlyphDisplayListCacheKey(const TextRun& textRun, const FontCascade& font, GraphicsContext& context)
        : m_textRun(textRun.isolatedCopy())
        , m_scaleFactor(context.scaleFactor())
        , m_fontCascadeGeneration(font.generation())
        , m_shouldSubpixelQuantizeFont(context.shouldSubpixelQuantizeFonts())
    {
    }

private:
    TextRun m_textRun;
    FloatSize m_scaleFactor;
    unsigned m_fontCascadeGeneration;
    bool m_shouldSubpixelQuantizeFont;
};

inline void add(Hasher& hasher, const SVGGlyphDisplayListCacheKey& key)
{
    add(hasher, key.m_textRun, key.m_scaleFactor.width(), key.m_scaleFactor.height(), key.m_fontCascadeGeneration, key.m_shouldSubpixelQuantizeFont);
}

struct SVGGlyphDisplayListCacheEntryHash {
    static unsigned hash(const SVGGlyphDisplayListCacheKey& key) { return computeHash(key); }
    static bool equal (const SVGGlyphDisplayListCacheKey& a, SVGGlyphDisplayListCacheKey b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct SVGGlyphDisplayListCacheEntryHashTraits : SimpleClassHashTraits<SVGGlyphDisplayListCacheKey> {
    static const bool emptyValueIsZero = false;
    static SVGGlyphDisplayListCacheKey emptyValue() { return { }; }
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const SVGGlyphDisplayListCacheKey& key) { return key.m_textRun.isHashTableEmptyValue(); }

    static void constructDeletedValue(SVGGlyphDisplayListCacheKey& slot) { new (NotNull, &slot) SVGGlyphDisplayListCacheKey(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const SVGGlyphDisplayListCacheKey& key) { return key.m_textRun.isHashTableDeletedValue(); }
};

class SVGGlyphDisplayListCache {
    WTF_MAKE_FAST_ALLOCATED;
    friend class SVGGlyphDisplayListCacheKey;
public:
    SVGGlyphDisplayListCache() = default;

    static SVGGlyphDisplayListCache& singleton();

    DisplayList::DisplayList* getDisplayList(const FontCascade&, GraphicsContext&, const TextRun&, const PaintInfo&);

    void clear();
    unsigned size() const;

private:
    static bool canShareDisplayList(const DisplayList::DisplayList&);
    HashMap<SVGGlyphDisplayListCacheKey, std::unique_ptr<DisplayList::DisplayList>, SVGGlyphDisplayListCacheEntryHash, SVGGlyphDisplayListCacheEntryHashTraits> m_entries;
};

} // namespace WebCore

namespace WTF {

template<> struct DefaultHash<WebCore::SVGGlyphDisplayListCacheKey> : WebCore::SVGGlyphDisplayListCacheEntryHash { };

} // namespace WTF
