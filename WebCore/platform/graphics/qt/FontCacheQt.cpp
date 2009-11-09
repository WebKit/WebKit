/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther
    Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "config.h"
#include "FontCache.h"

#include "FontDescription.h"
#include "FontPlatformData.h"
#include "Font.h"
#include "PlatformString.h"
#include "StringHash.h"
#include <utility>
#include <wtf/ListHashSet.h>
#include <wtf/StdLibExtras.h>

using namespace WTF;

namespace WebCore {

FontCache* fontCache()
{
    DEFINE_STATIC_LOCAL(FontCache, globalFontCache, ());
    return &globalFontCache;
}

FontCache::FontCache()
{
}

void FontCache::getTraitsInFamily(const AtomicString&, Vector<unsigned>&)
{
}

// This type must be consistent with FontPlatformData's ctor - the one which
// gets FontDescription as it's parameter.
class FontPlatformDataCacheKey {
public:
    FontPlatformDataCacheKey(const FontDescription& description)
        : m_familyName()
        , m_size(description.computedPixelSize())
        , m_bold(false)
        , m_italic(description.italic())
        , m_smallCaps(description.smallCaps())
        , m_hash(0)
    {
        // FIXME: Map all FontWeight values to QFont weights in FontPlatformData's ctor and follow it here
        if (FontPlatformData::toQFontWeight(description.weight()) > QFont::Normal)
            m_bold = true;

        const FontFamily* family = &description.family();
        while (family) {
            m_familyName.append(family->family());
            family = family->next();
            if (family)
                m_familyName.append(',');
        }

        computeHash();
    }

    FontPlatformDataCacheKey(const FontPlatformData& fontData)
        : m_familyName(static_cast<String>(fontData.family()))
        , m_size(fontData.pixelSize())
        , m_bold(fontData.bold())
        , m_italic(fontData.italic())
        , m_smallCaps(fontData.smallCaps())
        , m_hash(0)
    {
        computeHash();
    }

    FontPlatformDataCacheKey(HashTableDeletedValueType) : m_size(hashTableDeletedSize()) { }
    bool isHashTableDeletedValue() const { return m_size == hashTableDeletedSize(); }

    enum HashTableEmptyValueType { HashTableEmptyValue };

    FontPlatformDataCacheKey(HashTableEmptyValueType)
        : m_familyName()
        , m_size(0)
        , m_bold(false)
        , m_italic(false)
        , m_smallCaps(false)
        , m_hash(0)
    {
    }

    bool operator==(const FontPlatformDataCacheKey& other) const
    {
        if (m_hash != other.m_hash)
            return false;

        return equalIgnoringCase(m_familyName, other.m_familyName) && m_size == other.m_size &&
            m_bold == other.m_bold && m_italic == other.m_italic && m_smallCaps == other.m_smallCaps;
    }

    unsigned hash() const
    {
        return m_hash;
    }

    void computeHash()
    {
        unsigned hashCodes[] = {
            CaseFoldingHash::hash(m_familyName),
            m_size | static_cast<unsigned>(m_bold << (sizeof(unsigned) * 8 - 1))
                | static_cast<unsigned>(m_italic) << (sizeof(unsigned) * 8 - 2)
                | static_cast<unsigned>(m_smallCaps) << (sizeof(unsigned) * 8 - 3)
        };
        m_hash = StringImpl::computeHash(reinterpret_cast<UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
    }

private:
    String m_familyName;
    int m_size;
    bool m_bold;
    bool m_italic;
    bool m_smallCaps;
    unsigned m_hash;

    static unsigned hashTableDeletedSize() { return 0xFFFFFFFFU; }
};

struct FontPlatformDataCacheKeyHash {
    static unsigned hash(const FontPlatformDataCacheKey& key)
    {
        return key.hash();
    }

    static bool equal(const FontPlatformDataCacheKey& a, const FontPlatformDataCacheKey& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FontPlatformDataCacheKeyTraits : WTF::GenericHashTraits<FontPlatformDataCacheKey> {
    static const bool needsDestruction = true;
    static const FontPlatformDataCacheKey& emptyValue()
    {
        DEFINE_STATIC_LOCAL(FontPlatformDataCacheKey, key, (FontPlatformDataCacheKey::HashTableEmptyValue));
        return key;
    }
    static void constructDeletedValue(FontPlatformDataCacheKey& slot)
    {
        new (&slot) FontPlatformDataCacheKey(HashTableDeletedValue);
    }
    static bool isDeletedValue(const FontPlatformDataCacheKey& value)
    {
        return value.isHashTableDeletedValue();
    }
};

typedef HashMap<FontPlatformDataCacheKey, FontPlatformData*, FontPlatformDataCacheKeyHash, FontPlatformDataCacheKeyTraits> FontPlatformDataCache;

// using Q_GLOBAL_STATIC leads to crash. TODO investigate the way to fix this.
static FontPlatformDataCache* gFontPlatformDataCache = 0;

FontPlatformData* FontCache::getCachedFontPlatformData(const FontDescription& description, const AtomicString&, bool)
{
    if (!gFontPlatformDataCache)
        gFontPlatformDataCache = new FontPlatformDataCache;

    FontPlatformDataCacheKey key(description);
    FontPlatformData* platformData = gFontPlatformDataCache->get(key);
    if (!platformData) {
        platformData = new FontPlatformData(description);
        gFontPlatformDataCache->add(key, platformData);
    }
    return platformData;
}

typedef HashMap<FontPlatformDataCacheKey, std::pair<SimpleFontData*, unsigned>, FontPlatformDataCacheKeyHash, FontPlatformDataCacheKeyTraits> FontDataCache;

static FontDataCache* gFontDataCache = 0;

static const int cMaxInactiveFontData = 40;
static const int cTargetInactiveFontData = 32;

static ListHashSet<const SimpleFontData*>* gInactiveFontDataSet = 0;

SimpleFontData* FontCache::getCachedFontData(const FontPlatformData* fontPlatformData)
{
    if (!gFontDataCache) {
        gFontDataCache = new FontDataCache;
        gInactiveFontDataSet = new ListHashSet<const SimpleFontData*>;
    }

    FontPlatformDataCacheKey key(*fontPlatformData);
    FontDataCache::iterator it = gFontDataCache->find(key);
    if (it == gFontDataCache->end()) {
        SimpleFontData* fontData = new SimpleFontData(*fontPlatformData);
        gFontDataCache->add(key, std::pair<SimpleFontData*, unsigned>(fontData, 1));
        return fontData;
    }
    if (!it->second.second++) {
        ASSERT(gInactiveFontDataSet->contains(it->second.first));
        gInactiveFontDataSet->remove(it->second.first);
    }
    return it->second.first;
}

FontPlatformData* FontCache::getLastResortFallbackFont(const FontDescription&)
{
    return 0;
}

void FontCache::releaseFontData(const WebCore::SimpleFontData* fontData)
{
    ASSERT(gFontDataCache);
    ASSERT(!fontData->isCustomFont());

    FontPlatformDataCacheKey key(fontData->platformData());
    FontDataCache::iterator it = gFontDataCache->find(key);
    ASSERT(it != gFontDataCache->end());
    if (!--it->second.second) {
        gInactiveFontDataSet->add(it->second.first);
        if (gInactiveFontDataSet->size() > cMaxInactiveFontData)
            purgeInactiveFontData(gInactiveFontDataSet->size() - cTargetInactiveFontData);
    }
}

void FontCache::purgeInactiveFontData(int count)
{
    static bool isPurging;  // Guard against reentry when e.g. a deleted FontData releases its small caps FontData.
    if (isPurging)
        return;

    isPurging = true;

    ListHashSet<const SimpleFontData*>::iterator it = gInactiveFontDataSet->begin();
    ListHashSet<const SimpleFontData*>::iterator end = gInactiveFontDataSet->end();
    for (int i = 0; i < count && it != end; ++i, ++it) {
        FontPlatformDataCacheKey key = (*it)->platformData();
        pair<SimpleFontData*, unsigned> fontDataPair = gFontDataCache->take(key);
        ASSERT(fontDataPair.first != 0);
        ASSERT(!fontDataPair.second);
        delete fontDataPair.first;

        FontPlatformData* platformData = gFontPlatformDataCache->take(key);
        if (platformData)
            delete platformData;
    }

    if (it == end) {
        // Removed everything
        gInactiveFontDataSet->clear();
    } else {
        for (int i = 0; i < count; ++i)
            gInactiveFontDataSet->remove(gInactiveFontDataSet->begin());
    }

    isPurging = false;
}

void FontCache::addClient(FontSelector*)
{
}

void FontCache::removeClient(FontSelector*)
{
}

void FontCache::invalidate()
{
    if (!gFontPlatformDataCache || !gFontDataCache)
        return;

    purgeInactiveFontData();
}

} // namespace WebCore
