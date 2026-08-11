/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "IntlDateTimeFormat.h"
#include <atomic>
#include <optional>
#include <unicode/udatpg.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/icu/ICUHelpers.h>

namespace JSC {

struct IntlDateTimeFormatImplKey {
    std::optional<String> locales;
    IntlDateTimeFormat::RequiredComponent required { IntlDateTimeFormat::RequiredComponent::Any };
    IntlDateTimeFormat::Defaults defaults { IntlDateTimeFormat::Defaults::All };

    friend bool operator==(const IntlDateTimeFormatImplKey&, const IntlDateTimeFormatImplKey&) = default;
};

class IntlCache {
    WTF_MAKE_NONCOPYABLE(IntlCache);
    WTF_MAKE_TZONE_ALLOCATED(IntlCache);
public:
    IntlCache();
    ~IntlCache();

    static void ensureLanguageChangeObserver();

    Vector<char16_t, 32> getBestDateTimePattern(const CString& locale, std::span<const char16_t> skeleton, UErrorCode&);
    Vector<char16_t, 32> getFieldDisplayName(const CString& locale, UDateTimePatternField, UDateTimePGDisplayWidth, UErrorCode&);

    String canonicalizeUnicodeLocaleID(const String& languageTag);

    // The cache is reset only at the VM entry service scope; get and insert never check.
    RefPtr<const IntlDateTimeFormatImpl> findCachedDateTimeFormatImpl(const IntlDateTimeFormatImplKey& key)
    {
        if (auto cached = m_cachedDateTimeFormatImpls.findIfCached(key))
            return *cached;
        return nullptr;
    }

    void cacheDateTimeFormatImpl(const IntlDateTimeFormatImplKey& key, Ref<const IntlDateTimeFormatImpl>&& impl)
    {
        m_cachedDateTimeFormatImpls.insert(key, RefPtr<const IntlDateTimeFormatImpl>(WTF::move(impl)));
    }

    void clearForTimeZoneChange() { clearDateTimeFormatImplCache(); }
    uint64_t languagesEpoch() const { return m_lastSeenLanguagesEpoch; }
    bool hasLanguageChange() const { return m_lastSeenLanguagesEpoch != s_languagesEpoch.load(std::memory_order_acquire); }
    void clearForLanguageChange()
    {
        m_lastSeenLanguagesEpoch = s_languagesEpoch.load(std::memory_order_acquire);
        clearDateTimeFormatImplCache();
    }

private:
    void clearDateTimeFormatImplCache() { m_cachedDateTimeFormatImpls.clear(); }

    static constexpr size_t dateTimeFormatImplCacheCapacity = 4;
    using DateTimeFormatImplCache = WTF::TinyLRUCache<IntlDateTimeFormatImplKey, RefPtr<const IntlDateTimeFormatImpl>, dateTimeFormatImplCacheCapacity>;

    UDateTimePatternGenerator* getSharedPatternGenerator(const CString& locale, UErrorCode& status)
    {
        if (m_cachedDateTimePatternGenerator) {
            if (locale == m_cachedDateTimePatternGeneratorLocale)
                return m_cachedDateTimePatternGenerator.get();
        }
        return cacheSharedPatternGenerator(locale, status);
    }

    UDateTimePatternGenerator* cacheSharedPatternGenerator(const CString& locale, UErrorCode&);

    std::unique_ptr<UDateTimePatternGenerator, ICUDeleter<udatpg_close>> m_cachedDateTimePatternGenerator;
    CString m_cachedDateTimePatternGeneratorLocale;
    UncheckedKeyHashMap<String, String> m_cachedCanonicalizedLocaleIDs;

    static std::atomic<uint64_t> s_languagesEpoch;

    DateTimeFormatImplCache m_cachedDateTimeFormatImpls;
    uint64_t m_lastSeenLanguagesEpoch { 0 };
};

} // namespace JSC
