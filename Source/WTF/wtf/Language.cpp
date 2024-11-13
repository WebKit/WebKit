/*
 * Copyright (C) 2010, 2013, 2016, 2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/Language.h>

#include <wtf/CrossThreadCopier.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Logging.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WTF {

static Lock languagesLock;
static Vector<String>& cachedFullPlatformPreferredLanguages() WTF_REQUIRES_LOCK(languagesLock)
{
    static NeverDestroyed<Vector<String>> languages;
    return languages;
}
static Vector<String>& cachedMinimizedPlatformPreferredLanguages() WTF_REQUIRES_LOCK(languagesLock)
{
    static NeverDestroyed<Vector<String>> languages;
    return languages;
}
static Vector<String>& preferredLanguagesOverride() WTF_REQUIRES_LOCK(languagesLock)
{
    static NeverDestroyed<Vector<String>> override;
    return override;
}
static std::optional<bool> cachedUserPrefersSimplifiedChinese WTF_GUARDED_BY_LOCK(languagesLock);

using ObserverMap = HashMap<void*, LanguageChangeObserverFunction>;
static ObserverMap& observerMap()
{
    static LazyNeverDestroyed<ObserverMap> map;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        map.construct();
    });
    return map.get();
}

void addLanguageChangeObserver(void* context, LanguageChangeObserverFunction customObserver)
{
    observerMap().set(context, customObserver);
}

void removeLanguageChangeObserver(void* context)
{
    ASSERT(observerMap().contains(context));
    observerMap().remove(context);
}

void languageDidChange()
{
    {
        Locker locker { languagesLock };
        cachedFullPlatformPreferredLanguages().clear();
        cachedMinimizedPlatformPreferredLanguages().clear();
        cachedUserPrefersSimplifiedChinese = std::nullopt;
    }

    for (auto& observer : copyToVector(observerMap())) {
        if (observerMap().contains(observer.key))
            observer.value(observer.key);
    }
}

String defaultLanguage(ShouldMinimizeLanguages shouldMinimizeLanguages)
{
    auto languages = userPreferredLanguages(shouldMinimizeLanguages);
    if (languages.size()) {
        LOG_WITH_STREAM(Language, stream << "defaultLanguage() is returning " << languages[0]);
        return languages[0];
    }

    LOG(Language, "defaultLanguage() is returning the empty string.");
    return emptyString();
}

// This returns a reference to a Vector<String> protected by languagesLock.
// Callers should not let it escape past the lock.
static Vector<String>& computeUserPreferredLanguages(ShouldMinimizeLanguages shouldMinimizeLanguages) WTF_REQUIRES_LOCK(languagesLock)
{
    Vector<String>& override = preferredLanguagesOverride();
    if (!override.isEmpty()) {
        LOG_WITH_STREAM(Language, stream << "Languages are overridden: " << override);
        return override;
    }

    auto& languages = shouldMinimizeLanguages == ShouldMinimizeLanguages::Yes ? cachedMinimizedPlatformPreferredLanguages() : cachedFullPlatformPreferredLanguages();
    if (languages.isEmpty()) {
        LOG(Language, "userPreferredLanguages() cache miss");
        languages = platformUserPreferredLanguages(shouldMinimizeLanguages);
    } else
        LOG(Language, "userPreferredLanguages() cache hit");
    return languages;
}

Vector<String> userPreferredLanguages(ShouldMinimizeLanguages shouldMinimizeLanguages)
{
    Locker locker { languagesLock };
    return crossThreadCopy(computeUserPreferredLanguages(shouldMinimizeLanguages));
}

static bool computeUserPrefersSimplifiedChinese() WTF_REQUIRES_LOCK(languagesLock)
{
    for (const auto& language : computeUserPreferredLanguages(ShouldMinimizeLanguages::Yes)) {
        if (equalLettersIgnoringASCIICase(language, "zh-tw"_s))
            return false;
        if (equalLettersIgnoringASCIICase(language, "zh-cn"_s))
            return true;
    }
    return true;
}

bool userPrefersSimplifiedChinese()
{
    Locker locker { languagesLock };
    if (!cachedUserPrefersSimplifiedChinese)
        cachedUserPrefersSimplifiedChinese = computeUserPrefersSimplifiedChinese();
    return *cachedUserPrefersSimplifiedChinese;
}

#if !PLATFORM(COCOA)

void overrideUserPreferredLanguages(const Vector<String>& override)
{
    LOG_WITH_STREAM(Language, stream << "Languages are being overridden to: " << override);
    {
        Locker locker { languagesLock };
        preferredLanguagesOverride() = override;
        cachedUserPrefersSimplifiedChinese = std::nullopt;
    }
    languageDidChange();
}

static String canonicalLanguageIdentifier(const String& languageCode)
{
    return makeStringByReplacingAll(languageCode.convertToASCIILowercase(), '_', '-');
}

LocaleComponents parseLocale(const String& localeIdentifier)
{
    // Parse the components of the identifier per BCP47. https://www.ietf.org/rfc/bcp/bcp47.txt

    LocaleComponents result;

    auto components = canonicalLanguageIdentifier(localeIdentifier).split('-');
    if (components.isEmpty())
        return result;

    // First component is always the language code.
    result.languageCode = components[0].convertToASCIILowercase();

    // If only a language code is present, return it.
    if (components.size() <= 1)
        return result;

    // If the second component is four characters, interpret it as a script code.
    if (components[1].length() == 4)
        result.scriptCode = makeString(components[1].left(1).convertToASCIIUppercase(), components[1].substring(1));

    // Determine region code component based on presence of script code.
    auto regionCodeCandidate = components.size() >= 3 && !result.scriptCode.isEmpty() ? components[2] : components[1];

    // 2 character codes (e.g., "US") are ISO 3166-1 alpha-2; 3 character codes (e.g., "001") are ISO 3166-1 numeric.
    if (regionCodeCandidate.length() == 2)
        result.regionCode = regionCodeCandidate.convertToASCIIUppercase();
    else if (regionCodeCandidate.length() == 3 && parseInteger<uint16_t>(regionCodeCandidate, 10))
        result.regionCode = regionCodeCandidate;

    return result;
}

size_t indexOfBestMatchingLanguageInList(const String& language, const Vector<String>& languageList, bool& exactMatch)
{
    exactMatch = false;

    if (language.isEmpty() || languageList.isEmpty())
        return notFound;

    auto canonicalizedLanguage = canonicalLanguageIdentifier(language);
    auto components = parseLocale(canonicalizedLanguage);

    auto languageOnlyMatchIndex = notFound;
    auto partialMatchIndex = notFound;

    for (size_t i = 0; i < languageList.size(); ++i) {
        auto canonicalizedLanguageFromList = canonicalLanguageIdentifier(languageList[i]);
        if (canonicalizedLanguage == canonicalizedLanguageFromList) {
            exactMatch = true;
            return i;
        }

        auto componentsFromList = parseLocale(canonicalizedLanguageFromList);
        if (components.languageCode == componentsFromList.languageCode) {
            // If it's a language-only match, store the first occurrence.
            if (languageOnlyMatchIndex == notFound && componentsFromList.scriptCode.isEmpty() && componentsFromList.regionCode.isEmpty())
                languageOnlyMatchIndex = i;

            // If it's a partial match (language and script), store the first occurrence.
            if (partialMatchIndex == notFound && components.scriptCode == componentsFromList.scriptCode && !componentsFromList.regionCode.isEmpty())
                partialMatchIndex = i;
        }
    }

    // If we have both a language-only match and a language-but-not-locale match, return the
    // language-only match as it is considered a "better" match. For example, if the list
    // provided has both "en-GB" and "en" and the user prefers "en-US", we will return "en".
    if (languageOnlyMatchIndex != notFound)
        return languageOnlyMatchIndex;

    if (partialMatchIndex != notFound)
        return partialMatchIndex;

    return notFound;
}

#endif // !PLATFORM(COCOA)

String displayNameForLanguageLocale(const String& localeName)
{
#if USE(CF)
    if (!localeName.isEmpty())
        return adoptCF(CFLocaleCopyDisplayNameForPropertyValue(adoptCF(CFLocaleCopyCurrent()).get(), kCFLocaleIdentifier, localeName.createCFString().get())).get();
#endif
    return localeName;
}

}
