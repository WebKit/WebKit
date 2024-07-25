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

#include "FloatSizeHash.h"
#include "FontCascade.h"
#include "GlyphBuffer.h"
#include "Logging.h"
#include "TextRun.h"
#include "TextRunHash.h"
#include <memory>
#include <wtf/GetPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class GlyphBufferCacheKey {
    WTF_MAKE_FAST_ALLOCATED;
    friend struct GlyphBufferCacheEntryHashTraits;
    friend void add(Hasher&, const GlyphBufferCacheKey&);
public:
    bool operator==(const GlyphBufferCacheKey& other) const
    {
        return m_textRun == other.m_textRun
            && m_fontCascadeGeneration == other.m_fontCascadeGeneration
            && m_codePath == other.m_codePath
            && m_forTextEmphasisOrNot == other.m_forTextEmphasisOrNot
            && from == other.from
            && to == other.to;
    }

    GlyphBufferCacheKey(WTF::HashTableEmptyValueType)
        : m_textRun(WTF::HashTableEmptyValue)
    { }

    GlyphBufferCacheKey(WTF::HashTableDeletedValueType)
        : m_textRun(WTF::HashTableDeletedValue)
    { }

    GlyphBufferCacheKey(const TextRun& textRun, const FontCascade& font, FontCascade::CodePath& codePath, FontCascade::ForTextEmphasisOrNot forTextEmphasisOrNot, unsigned from, unsigned to)
        : m_textRun(textRun)
        , m_fontCascadeGeneration(font.generation())
        , m_codePath(codePath)
        , m_forTextEmphasisOrNot(forTextEmphasisOrNot)
        , from(from)
        , to(to)
    {
    }

private:
    TextRun m_textRun;
    unsigned m_fontCascadeGeneration;
    FontCascade::CodePath m_codePath;
    FontCascade::ForTextEmphasisOrNot m_forTextEmphasisOrNot;
    unsigned from;
    unsigned to;
};

inline void add(Hasher& hasher, const GlyphBufferCacheKey& key)
{
    add(hasher, key.m_textRun, key.m_fontCascadeGeneration, key.m_codePath, key.m_forTextEmphasisOrNot, key.from, key.to);
}

struct GlyphBufferCacheEntryHash {
    static unsigned hash(const GlyphBufferCacheKey& key) { return computeHash(key); }
    static bool equal (const GlyphBufferCacheKey& a, const GlyphBufferCacheKey& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct GlyphBufferCacheEntryHashTraits : WTF::GenericHashTraits<GlyphBufferCacheKey> {
    static const bool emptyValueIsZero = false;
    static const bool hasIsEmptyValueFunction = true;
    static GlyphBufferCacheKey emptyValue() { return GlyphBufferCacheKey(WTF::HashTableEmptyValue); }
    static void constructDeletedValue(GlyphBufferCacheKey& slot) { new (NotNull, &slot) GlyphBufferCacheKey(WTF::HashTableDeletedValue); }
    static const bool hasIsDeletedValueFunction = true;
    static bool isDeletedValue(const GlyphBufferCacheKey& key) { return key.m_textRun.isHashTableDeletedValue(); }
    static bool isEmptyValue(const GlyphBufferCacheKey& key) { return key.m_textRun.isHashTableEmptyValue(); }
};

class GlyphBufferCache {
    WTF_MAKE_FAST_ALLOCATED;
    friend class GlyphBufferCacheKey;
public:
    GlyphBufferCache() = default;

    static GlyphBufferCache& singleton();

    // DisplayList::DisplayList* getDisplayList(const FontCascade&, GraphicsContext&, const TextRun&);
    GlyphBuffer glyphBuffer(const FontCascade&, FontCascade::CodePath, FontCascade::ForTextEmphasisOrNot, const TextRun&, unsigned, unsigned);

    void clear();
    unsigned size() const;

private:
    // static bool canShareDisplayList(const DisplayList::DisplayList&);
    HashMap<GlyphBufferCacheKey, WTF::UniqueRef<GlyphBuffer>, GlyphBufferCacheEntryHash, GlyphBufferCacheEntryHashTraits> m_entries;
    // HashMap<GlyphBufferCacheKey,std::unique_ptr<GlyphBuffer>, GlyphBufferCacheEntryHash, GlyphBufferCacheEntryHashTraits> m_entries;
};

} // namespace WebCore

namespace WTF {

template<> struct DefaultHash<WebCore::GlyphBufferCacheKey> : WebCore::GlyphBufferCacheEntryHash { };

} // namespace WTF
