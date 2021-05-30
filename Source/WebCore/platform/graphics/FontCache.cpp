/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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

namespace WebCore {

FontFamilyName::FontFamilyName() = default;

inline FontFamilyName::FontFamilyName(const AtomString& name)
    : m_name { name }
{
}

inline const AtomString& FontFamilyName::string() const
{
    return m_name;
}

inline void add(Hasher& hasher, const FontFamilyName& name)
{
    // FIXME: Would be better to hash the characters in the name instead of hashing a hash.
    if (!name.string().isNull())
        add(hasher, FontCascadeDescription::familyNameHash(name.string()));
}

inline bool operator==(const FontFamilyName& a, const FontFamilyName& b)
{
    return (a.string().isNull() || b.string().isNull()) ? a.string() == b.string() : FontCascadeDescription::familyNamesAreEqual(a.string(), b.string());
}

inline bool operator!=(const FontFamilyName& a, const FontFamilyName& b)
{
    return !(a == b);
}

struct FontPlatformDataCacheKey {
    FontDescriptionKey descriptionKey;
    FontFamilyName family;
    FontFeatureSettings features;
    FontSelectionSpecifiedCapabilities capabilities;
};

static bool operator==(const FontPlatformDataCacheKey& a, const FontPlatformDataCacheKey& b)
{
    return a.descriptionKey == b.descriptionKey && a.family == b.family && a.features == b.features && a.capabilities == b.capabilities;
}

struct FontPlatformDataCacheKeyHash {
    static unsigned hash(const FontPlatformDataCacheKey& key) { return computeHash(key.descriptionKey, key.family, key.features, key.capabilities.tied()); }
    static bool equal(const FontPlatformDataCacheKey& a, const FontPlatformDataCacheKey& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

struct FontPlatformDataCacheKeyHashTraits : public SimpleClassHashTraits<FontPlatformDataCacheKey> {
    static constexpr bool emptyValueIsZero = false;
    static void constructDeletedValue(FontPlatformDataCacheKey& slot)
    {
        new (NotNull, &slot.descriptionKey) FontDescriptionKey(WTF::HashTableDeletedValue);
    }
    static bool isDeletedValue(const FontPlatformDataCacheKey& key)
    {
        return key.descriptionKey.isHashTableDeletedValue();
    }
};

struct FontDataCacheKeyHash {
    static unsigned hash(const FontPlatformData& data) { return data.hash(); }
    static bool equal(const FontPlatformData& a, const FontPlatformData& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

struct FontDataCacheKeyTraits : WTF::GenericHashTraits<FontPlatformData> {
    static constexpr bool emptyValueIsZero = true;
    static const FontPlatformData& emptyValue()
    {
        static NeverDestroyed<FontPlatformData> key(0.f, false, false);
        return key;
    }
    static void constructDeletedValue(FontPlatformData& slot)
    {
        new (NotNull, &slot) FontPlatformData(WTF::HashTableDeletedValue);
    }
    static bool isDeletedValue(const FontPlatformData& value)
    {
        return value.isHashTableDeletedValue();
    }
};

using FontPlatformDataCache = HashMap<FontPlatformDataCacheKey, std::unique_ptr<FontPlatformData>, FontPlatformDataCacheKeyHash, FontPlatformDataCacheKeyHashTraits>;
using FontDataCache = HashMap<FontPlatformData, Ref<Font>, FontDataCacheKeyHash, FontDataCacheKeyTraits>;
#if ENABLE(OPENTYPE_VERTICAL)
using FontVerticalDataCache = HashMap<FontPlatformData, RefPtr<OpenTypeVerticalData>, FontDataCacheKeyHash, FontDataCacheKeyTraits>;
#endif

struct FontCache::FontDataCaches {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    FontDataCache data;
    FontPlatformDataCache platformData;
#if ENABLE(OPENTYPE_VERTICAL)
    FontVerticalDataCache verticalData;
#endif
};

Ref<FontCache> FontCache::create()
{
    ASSERT(!isMainThread());
    return adoptRef(*new FontCache());
}

FontCache& FontCache::singleton()
{
    static MainThreadNeverDestroyed<FontCache> globalFontCache;
    return globalFontCache;
}

FontCache::FontCache()
    : m_purgeTimer(*this, &FontCache::purgeInactiveFontDataIfNeeded)
    , m_fontDataCaches(makeUniqueRef<FontDataCaches>())
{
}

FontCache::~FontCache() = default;

std::optional<ASCIILiteral> FontCache::alternateFamilyName(const String& familyName)
{
    if (auto platformSpecificAlternate = platformAlternateFamilyName(familyName))
        return platformSpecificAlternate;

    switch (familyName.length()) {
    case 5:
        if (equalLettersIgnoringASCIICase(familyName, "arial"))
            return "Helvetica"_s;
        if (equalLettersIgnoringASCIICase(familyName, "times"))
            return "Times New Roman"_s;
        break;
    case 7:
        if (equalLettersIgnoringASCIICase(familyName, "courier"))
            return "Courier New"_s;
        break;
    case 9:
        if (equalLettersIgnoringASCIICase(familyName, "helvetica"))
            return "Arial"_s;
        break;
#if !OS(WINDOWS)
    // On Windows, Courier New is a TrueType font that is always present and
    // Courier is a bitmap font that we do not support. So, we don't want to map
    // Courier New to Courier.
    // FIXME: Not sure why this is harmful on Windows, since the alternative will
    // only be tried if Courier New is not found.
    case 11:
        if (equalLettersIgnoringASCIICase(familyName, "courier new"))
            return "Courier"_s;
        break;
#endif
    case 15:
        if (equalLettersIgnoringASCIICase(familyName, "times new roman"))
            return "Times"_s;
        break;
    }

    return std::nullopt;
}

FontPlatformData* FontCache::cachedFontPlatformData(const FontDescription& fontDescription, const String& passedFamilyName,
    const FontFeatureSettings* features, FontSelectionSpecifiedCapabilities capabilities, bool checkingAlternateName)
{
#if PLATFORM(IOS_FAMILY)
    Locker locker { m_fontLock };
#endif

#if OS(WINDOWS) && ENABLE(OPENTYPE_VERTICAL)
    // Leading "@" in the font name enables Windows vertical flow flag for the font.
    // Because we do vertical flow by ourselves, we don't want to use the Windows feature.
    // IE disregards "@" regardless of the orientation, so we follow the behavior.
    const String& familyName = passedFamilyName.substring(passedFamilyName[0] == '@' ? 1 : 0);
#else
    const String& familyName = passedFamilyName;
#endif

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&]() {
        platformInit();
    });

    FontPlatformDataCacheKey key { fontDescription, { familyName }, features ? *features : FontFeatureSettings { }, capabilities };

    auto addResult = m_fontDataCaches->platformData.add(key, nullptr);
    FontPlatformDataCache::iterator it = addResult.iterator;
    if (addResult.isNewEntry) {
        it->value = createFontPlatformData(fontDescription, familyName, features, capabilities);
        if (!it->value && !checkingAlternateName) {
            // We were unable to find a font. We have a small set of fonts that we alias to other names,
            // e.g., Arial/Helvetica, Courier/Courier New, etc. Try looking up the font under the aliased name.
            if (auto alternateName = alternateFamilyName(familyName)) {
                auto* alternateData = cachedFontPlatformData(fontDescription, *alternateName, features, capabilities, true);
                // Look up the key in the hash table again as the previous iterator may have
                // been invalidated by the recursive call to cachedFontPlatformData().
                it = m_fontDataCaches->platformData.find(key);
                ASSERT(it != m_fontDataCaches->platformData.end());
                if (alternateData)
                    it->value = makeUnique<FontPlatformData>(*alternateData);
            }
        }
    }

    return it->value.get();
}

#if PLATFORM(IOS_FAMILY)
const unsigned cMaxInactiveFontData = 120;
const unsigned cTargetInactiveFontData = 100;
#else
const unsigned cMaxInactiveFontData = 225;
const unsigned cTargetInactiveFontData = 200;
#endif

const unsigned cMaxUnderMemoryPressureInactiveFontData = 50;
const unsigned cTargetUnderMemoryPressureInactiveFontData = 30;

RefPtr<Font> FontCache::fontForFamily(const FontDescription& fontDescription, const String& family, const FontFeatureSettings* features, FontSelectionSpecifiedCapabilities capabilities, bool checkingAlternateName)
{
    if (!m_purgeTimer.isActive())
        m_purgeTimer.startOneShot(0_s);

    if (auto* platformData = cachedFontPlatformData(fontDescription, family, features, capabilities, checkingAlternateName))
        return fontForPlatformData(*platformData);

    return nullptr;
}

Ref<Font> FontCache::fontForPlatformData(const FontPlatformData& platformData)
{
#if PLATFORM(IOS_FAMILY)
    Locker locker { m_fontLock };
#endif

    auto addResult = m_fontDataCaches->data.ensure(platformData, [&] {
        return Font::create(platformData, Font::Origin::Local, this);
    });

    ASSERT(addResult.iterator->value->platformData() == platformData);

    return addResult.iterator->value.copyRef();
}

void FontCache::purgeInactiveFontDataIfNeeded()
{
    bool underMemoryPressure = MemoryPressureHandler::singleton().isUnderMemoryPressure();
    unsigned inactiveFontDataLimit = underMemoryPressure ? cMaxUnderMemoryPressureInactiveFontData : cMaxInactiveFontData;

    LOG(Fonts, "FontCache::purgeInactiveFontDataIfNeeded() - underMemoryPressure %d, inactiveFontDataLimit %u", underMemoryPressure, inactiveFontDataLimit);

    if (m_fontDataCaches->data.size() < inactiveFontDataLimit)
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
    Locker locker { m_fontLock };
#endif

    while (purgeCount) {
        Vector<Ref<Font>, 20> fontsToDelete;
        for (auto& font : m_fontDataCaches->data.values()) {
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
            bool success = m_fontDataCaches->data.remove(font->platformData());
            ASSERT_UNUSED(success, success);
#if ENABLE(OPENTYPE_VERTICAL)
            m_fontDataCaches->verticalData.remove(font->platformData());
#endif
        }
    };

    Vector<FontPlatformDataCacheKey> keysToRemove;
    keysToRemove.reserveInitialCapacity(m_fontDataCaches->platformData.size());
    for (auto& entry : m_fontDataCaches->platformData) {
        if (entry.value && !m_fontDataCaches->data.contains(*entry.value))
            keysToRemove.uncheckedAppend(entry.key);
    }

    LOG(Fonts, " removing %lu keys", keysToRemove.size());

    for (auto& key : keysToRemove)
        m_fontDataCaches->platformData.remove(key);

    platformPurgeInactiveFontData();
}

bool operator==(const FontCascadeCacheKey& a, const FontCascadeCacheKey& b)
{
    return a.fontDescriptionKey == b.fontDescriptionKey
        && a.fontSelectorId == b.fontSelectorId
        && a.fontSelectorVersion == b.fontSelectorVersion
        && a.families == b.families;
}

unsigned FontCascadeCacheKeyHash::hash(const FontCascadeCacheKey& key)
{
    return computeHash(key.fontDescriptionKey, key.fontSelectorId, key.fontSelectorVersion, key.families);
}

void FontCache::invalidateFontCascadeCache()
{
    m_fontCascadeCache.clear();
}

void FontCache::clearWidthCaches()
{
    for (auto& value : m_fontCascadeCache.values())
        value->fonts.get().widthCache().clear();
}

#if ENABLE(OPENTYPE_VERTICAL)
RefPtr<OpenTypeVerticalData> FontCache::verticalData(const FontPlatformData& platformData)
{
    auto addResult = m_fontDataCaches->verticalData.ensure(platformData, [&platformData] {
        return OpenTypeVerticalData::create(platformData);
    });
    return addResult.iterator->value;
}
#endif

static FontCascadeCacheKey makeFontCascadeCacheKey(const FontCascadeDescription& description, FontSelector* fontSelector)
{
    FontCascadeCacheKey key;
    key.fontDescriptionKey = FontDescriptionKey(description);
    unsigned familyCount = description.familyCount();
    key.families.reserveInitialCapacity(familyCount);
    for (unsigned i = 0; i < familyCount; ++i)
        key.families.uncheckedAppend(description.familyAt(i));
    key.fontSelectorId = fontSelector ? fontSelector->uniqueId() : 0;
    key.fontSelectorVersion = fontSelector ? fontSelector->version() : 0;
    return key;
}

void FontCache::pruneUnreferencedEntriesFromFontCascadeCache()
{
    m_fontCascadeCache.removeIf([](auto& entry) {
        return entry.value->fonts.get().hasOneRef();
    });
}

void FontCache::pruneSystemFallbackFonts()
{
    for (auto& entry : m_fontCascadeCache.values())
        entry->fonts->pruneSystemFallbacks();
}

Ref<FontCascadeFonts> FontCache::retrieveOrAddCachedFonts(const FontCascadeDescription& fontDescription, RefPtr<FontSelector>&& fontSelector)
{
    auto key = makeFontCascadeCacheKey(fontDescription, fontSelector.get());
    auto addResult = m_fontCascadeCache.add(key, nullptr);
    if (!addResult.isNewEntry)
        return addResult.iterator->value->fonts.get();

    auto& newEntry = addResult.iterator->value;
    newEntry = makeUnique<FontCascadeCacheEntry>(FontCascadeCacheEntry { WTFMove(key), FontCascadeFonts::create(WTFMove(fontSelector)) });
    Ref<FontCascadeFonts> glyphs = newEntry->fonts.get();

    static constexpr unsigned unreferencedPruneInterval = 50;
    static constexpr int maximumEntries = 400;
    static unsigned pruneCounter;
    // Referenced FontCascadeFonts would exist anyway so pruning them saves little memory.
    if (!(++pruneCounter % unreferencedPruneInterval))
        pruneUnreferencedEntriesFromFontCascadeCache();
    // Prevent pathological growth.
    if (m_fontCascadeCache.size() > maximumEntries)
        m_fontCascadeCache.remove(m_fontCascadeCache.random());
    return glyphs;
}

void FontCache::updateFontCascade(const FontCascade& fontCascade, RefPtr<FontSelector>&& fontSelector)
{
    fontCascade.updateFonts(retrieveOrAddCachedFonts(fontCascade.fontDescription(), WTFMove(fontSelector)));
}

size_t FontCache::fontCount()
{
    return m_fontDataCaches->data.size();
}

size_t FontCache::inactiveFontCount()
{
#if PLATFORM(IOS_FAMILY)
    Locker locker { m_fontLock };
#endif
    unsigned count = 0;
    for (auto& font : m_fontDataCaches->data.values()) {
        if (font->hasOneRef())
            ++count;
    }
    return count;
}

void FontCache::addClient(FontSelector& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void FontCache::removeClient(FontSelector& client)
{
    ASSERT(m_clients.contains(&client));

    m_clients.remove(&client);
}

void FontCache::invalidate()
{
    m_fontDataCaches->platformData.clear();
#if ENABLE(OPENTYPE_VERTICAL)
    m_fontDataCaches->verticalData.clear();
#endif
    invalidateFontCascadeCache();

    ++m_generation;

    for (auto& client : copyToVectorOf<RefPtr<FontSelector>>(m_clients))
        client->fontCacheInvalidated();

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

RefPtr<Font> FontCache::similarFont(const FontDescription&, const String&)
{
    return nullptr;
}
#endif

} // namespace WebCore
