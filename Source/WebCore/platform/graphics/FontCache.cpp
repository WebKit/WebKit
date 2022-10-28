/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
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
#include "FontCreationContext.h"
#include "FontPlatformData.h"
#include "FontSelector.h"
#include "Logging.h"
#include "SystemFontDatabase.h"
#include "ThreadGlobalData.h"
#include "WebKitFontFamilyNames.h"
#include "WorkerOrWorkletThread.h"
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/StringHash.h>

#if ENABLE(OPENTYPE_VERTICAL)
#include "OpenTypeVerticalData.h"
#endif

namespace WebCore {

struct FontPlatformDataCacheKey {
    FontDescriptionKey descriptionKey;
    FontFamilyName family;
    FontCreationContext fontCreationContext;
};

inline void add(Hasher& hasher, const FontPlatformDataCacheKey& key)
{
    add(hasher, key.descriptionKey, key.family, key.fontCreationContext);
}

static bool operator==(const FontPlatformDataCacheKey& a, const FontPlatformDataCacheKey& b)
{
    return a.descriptionKey == b.descriptionKey
        && a.family == b.family
        && a.fontCreationContext == b.fontCreationContext;
}

struct FontPlatformDataCacheKeyHash {
    static unsigned hash(const FontPlatformDataCacheKey& key) { return computeHash(key); }
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

static FontCache*& fontCacheForCurrentThread()
{
    static ThreadSpecific<FontCache*>* fontCache;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        fontCache = new ThreadSpecific<FontCache*>;
    });
    return **fontCache;
}

FontCache& FontCache::forCurrentThread()
{
    auto& fontCache = fontCacheForCurrentThread();
    if (UNLIKELY(!fontCache))
        fontCache = new FontCache;
    return *fontCache;
}

FontCache* FontCache::forCurrentThreadIfExists()
{
    return fontCacheForCurrentThread();
}

void FontCache::destroy()
{
    if (auto& fontCache = fontCacheForCurrentThread()) {
        fontCache->invalidate();
        FontCache* ptr = fontCache;
        fontCache = nullptr;
        delete ptr;
    }
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
        if (equalLettersIgnoringASCIICase(familyName, "arial"_s))
            return "Helvetica"_s;
        if (equalLettersIgnoringASCIICase(familyName, "times"_s))
            return "Times New Roman"_s;
        break;
    case 7:
        if (equalLettersIgnoringASCIICase(familyName, "courier"_s))
            return "Courier New"_s;
        break;
    case 9:
        if (equalLettersIgnoringASCIICase(familyName, "helvetica"_s))
            return "Arial"_s;
        break;
#if !OS(WINDOWS)
    // On Windows, Courier New is a TrueType font that is always present and
    // Courier is a bitmap font that we do not support. So, we don't want to map
    // Courier New to Courier.
    // FIXME: Not sure why this is harmful on Windows, since the alternative will
    // only be tried if Courier New is not found.
    case 11:
        if (equalLettersIgnoringASCIICase(familyName, "courier new"_s))
            return "Courier"_s;
        break;
#endif
    case 15:
        if (equalLettersIgnoringASCIICase(familyName, "times new roman"_s))
            return "Times"_s;
        break;
    }

    return std::nullopt;
}

FontPlatformData* FontCache::cachedFontPlatformData(const FontDescription& fontDescription, const String& passedFamilyName, const FontCreationContext& fontCreationContext, bool checkingAlternateName)
{
#if PLATFORM(IOS_FAMILY)
    Locker locker { m_fontLock };
#endif

#if OS(WINDOWS) && ENABLE(OPENTYPE_VERTICAL)
    // Leading "@" in the font name enables Windows vertical flow flag for the font.
    // Because we do vertical flow by ourselves, we don't want to use the Windows feature.
    // IE disregards "@" regardless of the orientation, so we follow the behavior.
    auto familyName = StringView(passedFamilyName).substring(passedFamilyName[0] == '@' ? 1 : 0).toAtomString();
#else
    AtomString familyName { passedFamilyName };
#endif

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&]() {
        platformInit();
    });

    FontPlatformDataCacheKey key { fontDescription, { familyName }, fontCreationContext };

    auto addResult = m_fontDataCaches->platformData.add(key, nullptr);
    FontPlatformDataCache::iterator it = addResult.iterator;
    if (addResult.isNewEntry) {
        it->value = createFontPlatformData(fontDescription, familyName, fontCreationContext);
        if (!it->value && !checkingAlternateName) {
            // We were unable to find a font. We have a small set of fonts that we alias to other names,
            // e.g., Arial/Helvetica, Courier/Courier New, etc. Try looking up the font under the aliased name.
            if (auto alternateName = alternateFamilyName(familyName)) {
                auto* alternateData = cachedFontPlatformData(fontDescription, *alternateName, fontCreationContext, true);
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

RefPtr<Font> FontCache::fontForFamily(const FontDescription& fontDescription, const String& family, const FontCreationContext& fontCreationContext, bool checkingAlternateName)
{
    if (!m_purgeTimer.isActive())
        m_purgeTimer.startOneShot(0_s);

    if (auto* platformData = cachedFontPlatformData(fontDescription, family, fontCreationContext, checkingAlternateName))
        return fontForPlatformData(*platformData);

    return nullptr;
}

Ref<Font> FontCache::fontForPlatformData(const FontPlatformData& platformData)
{
#if PLATFORM(IOS_FAMILY)
    Locker locker { m_fontLock };
#endif

    auto addResult = m_fontDataCaches->data.ensure(platformData, [&] {
        return Font::create(platformData, Font::Origin::Local);
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

    m_fontCascadeCache.pruneUnreferencedEntries();
    m_fontCascadeCache.pruneSystemFallbackFonts();

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

#if ENABLE(OPENTYPE_VERTICAL)
RefPtr<OpenTypeVerticalData> FontCache::verticalData(const FontPlatformData& platformData)
{
    auto addResult = m_fontDataCaches->verticalData.ensure(platformData, [&platformData] {
        return OpenTypeVerticalData::create(platformData);
    });
    return addResult.iterator->value;
}
#endif

void FontCache::updateFontCascade(const FontCascade& fontCascade, RefPtr<FontSelector>&& fontSelector)
{
    fontCascade.updateFonts(m_fontCascadeCache.retrieveOrAddCachedFonts(fontCascade.fontDescription(), WTFMove(fontSelector)));
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
    m_fontCascadeCache.invalidate();

    SystemFontDatabase::singleton().invalidate();

    platformInvalidate();

    ++m_generation;

    for (auto& client : copyToVectorOf<RefPtr<FontSelector>>(m_clients))
        client->fontCacheInvalidated();

    purgeInactiveFontData();
}

static Function<void()>& fontCacheInvalidationCallback()
{
    static NeverDestroyed<Function<void()>> callback;
    return callback.get();
}

void FontCache::registerFontCacheInvalidationCallback(Function<void()>&& callback)
{
    fontCacheInvalidationCallback() = WTFMove(callback);
}

template<typename F>
static void dispatchToAllFontCaches(F function)
{
    ASSERT(isMainThread());

    function(FontCache::forCurrentThread());

    Locker locker { WorkerOrWorkletThread::workerOrWorkletThreadsLock() };
    for (auto thread : WorkerOrWorkletThread::workerOrWorkletThreads()) {
        thread->runLoop().postTask([function](ScriptExecutionContext&) {
            if (auto fontCache = FontCache::forCurrentThreadIfExists())
                function(*fontCache);
        });
    }
}

void FontCache::invalidateAllFontCaches(ShouldRunInvalidationCallback shouldRunInvalidationCallback)
{
    dispatchToAllFontCaches([](FontCache& fontCache) {
        fontCache.invalidate();
    });

    if (shouldRunInvalidationCallback == ShouldRunInvalidationCallback::Yes && fontCacheInvalidationCallback())
        fontCacheInvalidationCallback()();
}

void FontCache::releaseNoncriticalMemory()
{
    purgeInactiveFontData();
    m_fontCascadeCache.clearWidthCaches();
    platformReleaseNoncriticalMemory();
}

void FontCache::releaseNoncriticalMemoryInAllFontCaches()
{
    dispatchToAllFontCaches([](FontCache& fontCache) {
        fontCache.releaseNoncriticalMemory();
    });
}

bool FontCache::useBackslashAsYenSignForFamily(const AtomString& family)
{
    if (family.isEmpty())
        return false;

    if (m_familiesUsingBackslashAsYenSign.isEmpty()) {
        auto add = [&] (ASCIILiteral name, std::initializer_list<UChar> unicodeName) {
            m_familiesUsingBackslashAsYenSign.add(AtomString { name });
            unsigned unicodeNameLength = unicodeName.size();
            m_familiesUsingBackslashAsYenSign.add(AtomString { unicodeName.begin(), unicodeNameLength });
        };
        add("MS PGothic"_s, { 0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x30B4, 0x30B7, 0x30C3, 0x30AF });
        add("MS PMincho"_s, { 0xFF2D, 0xFF33, 0x0020, 0xFF30, 0x660E, 0x671D });
        add("MS Gothic"_s, { 0xFF2D, 0xFF33, 0x0020, 0x30B4, 0x30B7, 0x30C3, 0x30AF });
        add("MS Mincho"_s, { 0xFF2D, 0xFF33, 0x0020, 0x660E, 0x671D });
        add("Meiryo"_s, { 0x30E1, 0x30A4, 0x30EA, 0x30AA });
    }

    return m_familiesUsingBackslashAsYenSign.contains(family);
}

#if !PLATFORM(COCOA)

FontCache::PrewarmInformation FontCache::collectPrewarmInformation() const
{
    return { };
}

void FontCache::prewarmGlobally()
{
}

void FontCache::prewarm(PrewarmInformation&&)
{
}

RefPtr<Font> FontCache::similarFont(const FontDescription&, const String&)
{
    return nullptr;
}

void FontCache::platformReleaseNoncriticalMemory()
{
}
#endif

} // namespace WebCore
