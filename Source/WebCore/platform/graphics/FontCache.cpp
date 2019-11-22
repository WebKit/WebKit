/*
 * Copyright (C) 2006, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontCache.h"

#include "FontCascade.h"
#include "FontPlatformData.h"
#include "FontSelector.h"
#include "Logging.h"
#include "WebKitFontFamilyNames.h"
#include <wtf/HashMap.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringHash.h>

#if ENABLE(OPENTYPE_VERTICAL)
#include "OpenTypeVerticalData.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include <wtf/Lock.h>
#include <wtf/RecursiveLockAdapter.h>

static RecursiveLock fontLock;

#endif // PLATFORM(IOS_FAMILY)


namespace WebCore {
using namespace WTF;

FontCache& FontCache::singleton()
{
    static NeverDestroyed<FontCache> globalFontCache;
    return globalFontCache;
}

FontCache::FontCache()
    : m_purgeTimer(*this, &FontCache::purgeInactiveFontDataIfNeeded)
{
}

struct FontPlatformDataCacheKey {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FontPlatformDataCacheKey() { }
    FontPlatformDataCacheKey(const AtomString& family, const FontDescription& description, const FontFeatureSettings* fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities)
        : m_fontDescriptionKey(description)
        , m_family(family)
        , m_fontFaceFeatures(fontFaceFeatures ? *fontFaceFeatures : FontFeatureSettings())
        , m_fontFaceCapabilities(fontFaceCapabilities)
    { }

    explicit FontPlatformDataCacheKey(HashTableDeletedValueType t)
        : m_fontDescriptionKey(t)
    { }

    bool isHashTableDeletedValue() const { return m_fontDescriptionKey.isHashTableDeletedValue(); }

    bool operator==(const FontPlatformDataCacheKey& other) const
    {
        if (m_fontDescriptionKey != other.m_fontDescriptionKey
            || m_fontFaceFeatures != other.m_fontFaceFeatures
            || m_fontFaceCapabilities != other.m_fontFaceCapabilities)
            return false;
        if (m_family.impl() == other.m_family.impl())
            return true;
        if (m_family.isNull() || other.m_family.isNull())
            return false;
        return FontCascadeDescription::familyNamesAreEqual(m_family, other.m_family);
    }

    FontDescriptionKey m_fontDescriptionKey;
    AtomString m_family;
    FontFeatureSettings m_fontFaceFeatures;
    FontSelectionSpecifiedCapabilities m_fontFaceCapabilities;
};

struct FontPlatformDataCacheKeyHash {
    static unsigned hash(const FontPlatformDataCacheKey& fontKey)
    {
        IntegerHasher hasher;
        hasher.add(FontCascadeDescription::familyNameHash(fontKey.m_family));
        hasher.add(fontKey.m_fontDescriptionKey.computeHash());
        hasher.add(fontKey.m_fontFaceFeatures.hash());
        if (auto weight = fontKey.m_fontFaceCapabilities.weight)
            hasher.add(weight->uniqueValue());
        else
            hasher.add(std::numeric_limits<unsigned>::max());
        if (auto width = fontKey.m_fontFaceCapabilities.weight)
            hasher.add(width->uniqueValue());
        else
            hasher.add(std::numeric_limits<unsigned>::max());
        if (auto slope = fontKey.m_fontFaceCapabilities.weight)
            hasher.add(slope->uniqueValue());
        else
            hasher.add(std::numeric_limits<unsigned>::max());
        return hasher.hash();
    }
         
    static bool equal(const FontPlatformDataCacheKey& a, const FontPlatformDataCacheKey& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FontPlatformDataCacheKeyHashTraits : public SimpleClassHashTraits<FontPlatformDataCacheKey> {
    static const bool emptyValueIsZero = false;
};

typedef HashMap<FontPlatformDataCacheKey, std::unique_ptr<FontPlatformData>, FontPlatformDataCacheKeyHash, FontPlatformDataCacheKeyHashTraits> FontPlatformDataCache;

static FontPlatformDataCache& fontPlatformDataCache()
{
    static NeverDestroyed<FontPlatformDataCache> cache;
    return cache;
}

const AtomString& FontCache::alternateFamilyName(const AtomString& familyName)
{
    static NeverDestroyed<AtomString> arial("Arial", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> courier("Courier", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> courierNew("Courier New", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> helvetica("Helvetica", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> times("Times", AtomString::ConstructFromLiteral);
    static NeverDestroyed<AtomString> timesNewRoman("Times New Roman", AtomString::ConstructFromLiteral);

    const AtomString& platformSpecificAlternate = platformAlternateFamilyName(familyName);
    if (!platformSpecificAlternate.isNull())
        return platformSpecificAlternate;

    switch (familyName.length()) {
    case 5:
        if (equalLettersIgnoringASCIICase(familyName, "arial"))
            return helvetica;
        if (equalLettersIgnoringASCIICase(familyName, "times"))
            return timesNewRoman;
        break;
    case 7:
        if (equalLettersIgnoringASCIICase(familyName, "courier"))
            return courierNew;
        break;
    case 9:
        if (equalLettersIgnoringASCIICase(familyName, "helvetica"))
            return arial;
        break;
#if !OS(WINDOWS)
    // On Windows, Courier New is a TrueType font that is always present and
    // Courier is a bitmap font that we do not support. So, we don't want to map
    // Courier New to Courier.
    // FIXME: Not sure why this is harmful on Windows, since the alternative will
    // only be tried if Courier New is not found.
    case 11:
        if (equalLettersIgnoringASCIICase(familyName, "courier new"))
            return courier;
        break;
#endif
    case 15:
        if (equalLettersIgnoringASCIICase(familyName, "times new roman"))
            return times;
        break;
    }

    return nullAtom();
}

FontPlatformData* FontCache::getCachedFontPlatformData(const FontDescription& fontDescription, const AtomString& passedFamilyName,
    const FontFeatureSettings* fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities, bool checkingAlternateName)
{
#if PLATFORM(IOS_FAMILY)
    auto locker = holdLock(fontLock);
#endif
    
#if OS(WINDOWS) && ENABLE(OPENTYPE_VERTICAL)
    // Leading "@" in the font name enables Windows vertical flow flag for the font.
    // Because we do vertical flow by ourselves, we don't want to use the Windows feature.
    // IE disregards "@" regardless of the orientatoin, so we follow the behavior.
    const AtomString& familyName = (passedFamilyName.isEmpty() || passedFamilyName[0] != '@') ?
        passedFamilyName : AtomString(passedFamilyName.impl()->substring(1));
#else
    const AtomString& familyName = passedFamilyName;
#endif

    static bool initialized;
    if (!initialized) {
        platformInit();
        initialized = true;
    }

    FontPlatformDataCacheKey key(familyName, fontDescription, fontFaceFeatures, fontFaceCapabilities);

    auto addResult = fontPlatformDataCache().add(key, nullptr);
    FontPlatformDataCache::iterator it = addResult.iterator;
    if (addResult.isNewEntry) {
        it->value = createFontPlatformData(fontDescription, familyName, fontFaceFeatures, fontFaceCapabilities);

        if (!it->value && !checkingAlternateName) {
            // We were unable to find a font.  We have a small set of fonts that we alias to other names,
            // e.g., Arial/Helvetica, Courier/Courier New, etc.  Try looking up the font under the aliased name.
            const AtomString& alternateName = alternateFamilyName(familyName);
            if (!alternateName.isNull()) {
                FontPlatformData* fontPlatformDataForAlternateName = getCachedFontPlatformData(fontDescription, alternateName, fontFaceFeatures, fontFaceCapabilities, true);
                // Lookup the key in the hash table again as the previous iterator may have
                // been invalidated by the recursive call to getCachedFontPlatformData().
                it = fontPlatformDataCache().find(key);
                ASSERT(it != fontPlatformDataCache().end());
                if (fontPlatformDataForAlternateName)
                    it->value = makeUnique<FontPlatformData>(*fontPlatformDataForAlternateName);
            }
        }
    }

    return it->value.get();
}

struct FontDataCacheKeyHash {
    static unsigned hash(const FontPlatformData& platformData)
    {
        return platformData.hash();
    }
         
    static bool equal(const FontPlatformData& a, const FontPlatformData& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FontDataCacheKeyTraits : WTF::GenericHashTraits<FontPlatformData> {
    static const bool emptyValueIsZero = true;
    static const FontPlatformData& emptyValue()
    {
        static NeverDestroyed<FontPlatformData> key(0.f, false, false);
        return key;
    }
    static void constructDeletedValue(FontPlatformData& slot)
    {
        new (NotNull, &slot) FontPlatformData(HashTableDeletedValue);
    }
    static bool isDeletedValue(const FontPlatformData& value)
    {
        return value.isHashTableDeletedValue();
    }
};

typedef HashMap<FontPlatformData, Ref<Font>, FontDataCacheKeyHash, FontDataCacheKeyTraits> FontDataCache;

static FontDataCache& cachedFonts()
{
    static NeverDestroyed<FontDataCache> cache;
    return cache;
}

#if ENABLE(OPENTYPE_VERTICAL)
typedef HashMap<FontPlatformData, RefPtr<OpenTypeVerticalData>, FontDataCacheKeyHash, FontDataCacheKeyTraits> FontVerticalDataCache;

FontVerticalDataCache& fontVerticalDataCache()
{
    static NeverDestroyed<FontVerticalDataCache> fontVerticalDataCache;
    return fontVerticalDataCache;
}

RefPtr<OpenTypeVerticalData> FontCache::verticalData(const FontPlatformData& platformData)
{
    auto addResult = fontVerticalDataCache().ensure(platformData, [&platformData] {
        return OpenTypeVerticalData::create(platformData);
    });
    return addResult.iterator->value;
}
#endif

#if PLATFORM(IOS_FAMILY)
const unsigned cMaxInactiveFontData = 120;
const unsigned cTargetInactiveFontData = 100;
#else
const unsigned cMaxInactiveFontData = 225;
const unsigned cTargetInactiveFontData = 200;
#endif

const unsigned cMaxUnderMemoryPressureInactiveFontData = 50;
const unsigned cTargetUnderMemoryPressureInactiveFontData = 30;

RefPtr<Font> FontCache::fontForFamily(const FontDescription& fontDescription, const AtomString& family, const FontFeatureSettings* fontFaceFeatures, FontSelectionSpecifiedCapabilities fontFaceCapabilities, bool checkingAlternateName)
{
    if (!m_purgeTimer.isActive())
        m_purgeTimer.startOneShot(0_s);

    if (auto* platformData = getCachedFontPlatformData(fontDescription, family, fontFaceFeatures, fontFaceCapabilities, checkingAlternateName))
        return fontForPlatformData(*platformData);

    return nullptr;
}

Ref<Font> FontCache::fontForPlatformData(const FontPlatformData& platformData)
{
#if PLATFORM(IOS_FAMILY)
    auto locker = holdLock(fontLock);
#endif
    
    auto addResult = cachedFonts().ensure(platformData, [&] {
        return Font::create(platformData);
    });

    ASSERT(addResult.iterator->value->platformData() == platformData);

    return addResult.iterator->value.copyRef();
}

void FontCache::purgeInactiveFontDataIfNeeded()
{
    bool underMemoryPressure = MemoryPressureHandler::singleton().isUnderMemoryPressure();
    unsigned inactiveFontDataLimit = underMemoryPressure ? cMaxUnderMemoryPressureInactiveFontData : cMaxInactiveFontData;

    LOG(Fonts, "FontCache::purgeInactiveFontDataIfNeeded() - underMemoryPressure %d, inactiveFontDataLimit %u", underMemoryPressure, inactiveFontDataLimit);

    if (cachedFonts().size() < inactiveFontDataLimit)
        return;
    unsigned inactiveCount = inactiveFontCount();
    if (inactiveCount <= inactiveFontDataLimit)
        return;

    unsigned targetFontDataLimit = underMemoryPressure ? cTargetUnderMemoryPressureInactiveFontData : cTargetInactiveFontData;
    purgeInactiveFontData(inactiveCount - targetFontDataLimit);
}

void FontCache::purgeInactiveFontData(unsigned purgeCount)
{
    LOG(Fonts, "FontCache::purgeInactiveFontData(%u)", purgeCount);

    pruneUnreferencedEntriesFromFontCascadeCache();
    pruneSystemFallbackFonts();

#if PLATFORM(IOS_FAMILY)
    auto locker = holdLock(fontLock);
#endif

    while (purgeCount) {
        Vector<Ref<Font>, 20> fontsToDelete;
        for (auto& font : cachedFonts().values()) {
            LOG(Fonts, " trying to purge font %s (has one ref %d)", font->platformData().description().utf8().data(), font->hasOneRef());
            if (!font->hasOneRef())
                continue;
            fontsToDelete.append(font.copyRef());
            if (!--purgeCount)
                break;
        }
        // Fonts may ref other fonts so we loop until there are no changes.
        if (fontsToDelete.isEmpty())
            break;
        for (auto& font : fontsToDelete) {
            bool success = cachedFonts().remove(font->platformData());
            ASSERT_UNUSED(success, success);
#if ENABLE(OPENTYPE_VERTICAL)
            fontVerticalDataCache().remove(font->platformData());
#endif
        }
    };

    Vector<FontPlatformDataCacheKey> keysToRemove;
    keysToRemove.reserveInitialCapacity(fontPlatformDataCache().size());
    for (auto& entry : fontPlatformDataCache()) {
        if (entry.value && !cachedFonts().contains(*entry.value))
            keysToRemove.uncheckedAppend(entry.key);
    }

    LOG(Fonts, " removing %lu keys", keysToRemove.size());

    for (auto& key : keysToRemove)
        fontPlatformDataCache().remove(key);

    platformPurgeInactiveFontData();
}

size_t FontCache::fontCount()
{
    return cachedFonts().size();
}

size_t FontCache::inactiveFontCount()
{
#if PLATFORM(IOS_FAMILY)
    auto locker = holdLock(fontLock);
#endif
    unsigned count = 0;
    for (auto& font : cachedFonts().values()) {
        if (font->hasOneRef())
            ++count;
    }
    return count;
}

static HashSet<FontSelector*>* gClients;

void FontCache::addClient(FontSelector& client)
{
    if (!gClients)
        gClients = new HashSet<FontSelector*>;

    ASSERT(!gClients->contains(&client));
    gClients->add(&client);
}

void FontCache::removeClient(FontSelector& client)
{
    ASSERT(gClients);
    ASSERT(gClients->contains(&client));

    gClients->remove(&client);
}

static unsigned short gGeneration = 0;

unsigned short FontCache::generation()
{
    return gGeneration;
}

void FontCache::invalidate()
{
    if (!gClients) {
        ASSERT(fontPlatformDataCache().isEmpty());
        return;
    }

    fontPlatformDataCache().clear();
#if ENABLE(OPENTYPE_VERTICAL)
    fontVerticalDataCache().clear();
#endif
    invalidateFontCascadeCache();

    gGeneration++;

    Vector<Ref<FontSelector>> clients;
    clients.reserveInitialCapacity(gClients->size());
    for (auto it = gClients->begin(), end = gClients->end(); it != end; ++it)
        clients.uncheckedAppend(**it);

    for (unsigned i = 0; i < clients.size(); ++i)
        clients[i]->fontCacheInvalidated();

    purgeInactiveFontData();
}

#if !PLATFORM(COCOA)

FontCache::PrewarmInformation FontCache::collectPrewarmInformation() const
{
    return { };
}

void FontCache::prewarmGlobally()
{
}

void FontCache::prewarm(const PrewarmInformation&)
{
}

RefPtr<Font> FontCache::similarFont(const FontDescription&, const AtomString&)
{
    return nullptr;
}
#endif

} // namespace WebCore
