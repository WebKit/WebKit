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
#include <wtf/text/TextStream.h>
#include <wtf/text/WTFString.h>

#if USE(CF) && !PLATFORM(WIN)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WTF {

static Lock cachedPlatformPreferredLanguagesLock;
static Vector<String>& cachedFullPlatformPreferredLanguages() WTF_REQUIRES_LOCK(cachedPlatformPreferredLanguagesLock)
{
    static NeverDestroyed<Vector<String>> languages;
    return languages;
}
static Vector<String>& cachedMinimizedPlatformPreferredLanguages() WTF_REQUIRES_LOCK(cachedPlatformPreferredLanguagesLock)
{
    static NeverDestroyed<Vector<String>> languages;
    return languages;
}

static Lock preferredLanguagesOverrideLock;
static Vector<String>& preferredLanguagesOverride() WTF_REQUIRES_LOCK(preferredLanguagesOverrideLock)
{
    static NeverDestroyed<Vector<String>> override;
    return override;
}

typedef HashMap<void*, LanguageChangeObserverFunction> ObserverMap;
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
        Locker locker { cachedPlatformPreferredLanguagesLock };
        cachedFullPlatformPreferredLanguages().clear();
        cachedMinimizedPlatformPreferredLanguages().clear();
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

Vector<String> userPreferredLanguagesOverride()
{
    Locker locker { preferredLanguagesOverrideLock };
    return preferredLanguagesOverride();
}

void overrideUserPreferredLanguages(const Vector<String>& override)
{
    LOG_WITH_STREAM(Language, stream << "Languages are being overridden to: " << override);
    {
        Locker locker { preferredLanguagesOverrideLock };
        preferredLanguagesOverride() = override;
    }
    languageDidChange();
}

Vector<String> userPreferredLanguages(ShouldMinimizeLanguages shouldMinimizeLanguages)
{
    {
        Locker locker { preferredLanguagesOverrideLock };
        Vector<String>& override = preferredLanguagesOverride();
        if (!override.isEmpty()) {
            LOG_WITH_STREAM(Language, stream << "Languages are overridden: " << override);
            return crossThreadCopy(override);
        }
    }

    Locker locker { cachedPlatformPreferredLanguagesLock };
    auto& languages = shouldMinimizeLanguages == ShouldMinimizeLanguages::Yes ? cachedMinimizedPlatformPreferredLanguages() : cachedFullPlatformPreferredLanguages();
    if (languages.isEmpty()) {
        LOG(Language, "userPreferredLanguages() cache miss");
        languages = platformUserPreferredLanguages(shouldMinimizeLanguages);
    } else
        LOG(Language, "userPreferredLanguages() cache hit");
    return crossThreadCopy(languages);
}

#if !PLATFORM(COCOA)

static String canonicalLanguageIdentifier(const String& languageCode)
{
    String lowercaseLanguageCode = languageCode.convertToASCIILowercase();
    
    if (lowercaseLanguageCode.length() >= 3 && lowercaseLanguageCode[2] == '_')
        lowercaseLanguageCode.replace(2, 1, "-");

    return lowercaseLanguageCode;
}

size_t indexOfBestMatchingLanguageInList(const String& language, const Vector<String>& languageList, bool& exactMatch)
{
    String lowercaseLanguage = language.convertToASCIILowercase();
    String languageWithoutLocaleMatch;
    String languageMatchButNotLocale;
    size_t languageWithoutLocaleMatchIndex = 0;
    size_t languageMatchButNotLocaleMatchIndex = 0;
    bool canMatchLanguageOnly = (lowercaseLanguage.length() == 2 || (lowercaseLanguage.length() >= 3 && lowercaseLanguage[2] == '-'));

    for (size_t i = 0; i < languageList.size(); ++i) {
        String canonicalizedLanguageFromList = canonicalLanguageIdentifier(languageList[i]);

        if (lowercaseLanguage == canonicalizedLanguageFromList) {
            exactMatch = true;
            return i;
        }

        if (canMatchLanguageOnly && canonicalizedLanguageFromList.length() >= 2) {
            if (lowercaseLanguage[0] == canonicalizedLanguageFromList[0] && lowercaseLanguage[1] == canonicalizedLanguageFromList[1]) {
                if (!languageWithoutLocaleMatch.length() && canonicalizedLanguageFromList.length() == 2) {
                    languageWithoutLocaleMatch = languageList[i];
                    languageWithoutLocaleMatchIndex = i;
                }
                if (!languageMatchButNotLocale.length() && canonicalizedLanguageFromList.length() >= 3) {
                    languageMatchButNotLocale = languageList[i];
                    languageMatchButNotLocaleMatchIndex = i;
                }
            }
        }
    }

    exactMatch = false;

    // If we have both a language-only match and a languge-but-not-locale match, return the
    // languge-only match as is considered a "better" match. For example, if the list
    // provided has both "en-GB" and "en" and the user prefers "en-US" we will return "en".
    if (languageWithoutLocaleMatch.length())
        return languageWithoutLocaleMatchIndex;

    if (languageMatchButNotLocale.length())
        return languageMatchButNotLocaleMatchIndex;

    return languageList.size();
}

#endif // !PLATFORM(COCOA)

String displayNameForLanguageLocale(const String& localeName)
{
#if USE(CF) && !PLATFORM(WIN)
    if (!localeName.isEmpty())
        return adoptCF(CFLocaleCopyDisplayNameForPropertyValue(adoptCF(CFLocaleCopyCurrent()).get(), kCFLocaleIdentifier, localeName.createCFString().get())).get();
#endif
    return localeName;
}

}
