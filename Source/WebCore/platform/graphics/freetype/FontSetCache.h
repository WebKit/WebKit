/*
 * Copyright (C) 2022 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "FcUniquePtr.h"
#include "FontCascadeCache.h"
#include "FontDescription.h"
#include "RefPtrFontconfig.h"
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/Hasher.h>

namespace WebCore {

struct FontSetCacheKey {
    FontSetCacheKey() = default;

    FontSetCacheKey(const FontDescription& description, bool coloredFont)
        : descriptionKey(description)
        , preferColoredFont(coloredFont)
    {
    }

    explicit FontSetCacheKey(WTF::HashTableDeletedValueType deletedValue)
        : descriptionKey(deletedValue)
    {
    }

    bool operator==(const FontSetCacheKey& other) const
    {
        return descriptionKey == other.descriptionKey && preferColoredFont == other.preferColoredFont;
    }

    bool isHashTableDeletedValue() const { return descriptionKey.isHashTableDeletedValue(); }

    FontDescriptionKey descriptionKey;
    bool preferColoredFont { false };
};

inline void add(Hasher& hasher, const FontSetCacheKey& key)
{
    add(hasher, key.descriptionKey, key.preferColoredFont);
}

struct FontSetCacheKeyHash {
    static unsigned hash(const FontSetCacheKey& key) { return computeHash(key); }
    static bool equal(const FontSetCacheKey& a, const FontSetCacheKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

class FontSetCache {
    WTF_MAKE_NONCOPYABLE(FontSetCache);
public:
    FontSetCache() = default;

    RefPtr<FcPattern> bestForCharacters(const FontDescription&, bool, StringView);
    void clear();

private:
    struct FontSet {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        explicit FontSet(RefPtr<FcPattern>&&);

        RefPtr<FcPattern> pattern;
        FcUniquePtr<FcFontSet> fontSet;
        Vector<std::pair<FcPattern*, FcCharSet*>> patterns;
    };

    HashMap<FontSetCacheKey, std::unique_ptr<FontSet>, FontSetCacheKeyHash, SimpleClassHashTraits<FontSetCacheKey>> m_cache;
};

} // namespace WebCore
