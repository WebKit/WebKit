/*
 * Copyright (C) 2003, 2005, 2006, 2010, 2011, 2016, 2017 Apple Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <mutex>
#include <unicode/uloc.h>
#include <wtf/Assertions.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/spi/cf/CFBundleSPI.h>
#include <wtf/text/WTFString.h>

namespace WTF {

static Lock preferredLanguagesMutex;

#if PLATFORM(MAC)
static void languagePreferencesDidChange(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    languageDidChange();
}
#endif

static Vector<String>& preferredLanguages()
{
    static LazyNeverDestroyed<Vector<String>> languages;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        languages.construct();
    });
    return languages;
}

static String httpStyleLanguageCode(CFStringRef language)
{
    RetainPtr<CFStringRef> preferredLanguageCode;
    // If we can minimize the language list to reduce fingerprinting, we can afford to be more lossless when canonicalizing the locale list.
    if (canMinimizeLanguages())
        preferredLanguageCode = adoptCF(CFLocaleCreateCanonicalLanguageIdentifierFromString(kCFAllocatorDefault, language));
    else {
        SInt32 languageCode;
        SInt32 regionCode;
        SInt32 scriptCode;
        CFStringEncoding stringEncoding;

        // FIXME: This transformation is very wrong:
        // 1. There is no reason why CFBundle localization names would be at all related to language names as used on the Web.
        // 2. Script Manager codes cannot represent all languages that are now supported by the platform, so the conversion is lossy.
        // 3. This should probably match what is sent by the network layer as Accept-Language, but currently, that's implemented separately.
        CFBundleGetLocalizationInfoForLocalization(language, &languageCode, &regionCode, &scriptCode, &stringEncoding);
        preferredLanguageCode = adoptCF(CFBundleCopyLocalizationForLocalizationInfo(languageCode, regionCode, scriptCode, stringEncoding));
    }

    if (!preferredLanguageCode)
        preferredLanguageCode = language;
    auto mutableLanguageCode = adoptCF(CFStringCreateMutableCopy(kCFAllocatorDefault, 0, preferredLanguageCode.get()));

    // Turn a '_' into a '-' if it appears after a 2-letter language code
    if (CFStringGetLength(mutableLanguageCode.get()) >= 3 && CFStringGetCharacterAtIndex(mutableLanguageCode.get(), 2) == '_')
        CFStringReplace(mutableLanguageCode.get(), CFRangeMake(2, 1), CFSTR("-"));

    return mutableLanguageCode.get();
}

void platformLanguageDidChange()
{
    {
        auto locker = holdLock(preferredLanguagesMutex);
        preferredLanguages().clear();
    }
}

void listenForLanguageChangeNotifications()
{
#if PLATFORM(MAC)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), nullptr, &languagePreferencesDidChange, CFSTR("AppleLanguagePreferencesChangedNotification"), nullptr, CFNotificationSuspensionBehaviorCoalesce);
    });
#endif
}

Vector<String> platformUserPreferredLanguages()
{
    auto locker = holdLock(preferredLanguagesMutex);
    Vector<String>& userPreferredLanguages = preferredLanguages();

    if (userPreferredLanguages.isEmpty()) {
        RetainPtr<CFArrayRef> languages = adoptCF(CFLocaleCopyPreferredLanguages());
        languages = minimizedLanguagesFromLanguages(languages.get());
        CFIndex languageCount = CFArrayGetCount(languages.get());
        if (!languageCount)
            userPreferredLanguages.append("en");
        else {
            for (CFIndex i = 0; i < languageCount; i++)
                userPreferredLanguages.append(httpStyleLanguageCode(static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages.get(), i))));
        }
    }

    Vector<String> userPreferredLanguagesCopy;
    userPreferredLanguagesCopy.reserveInitialCapacity(userPreferredLanguages.size());

    for (auto& language : userPreferredLanguages)
        userPreferredLanguagesCopy.uncheckedAppend(language.isolatedCopy());

    return userPreferredLanguagesCopy;

    return Vector<String>();
}

} // namespace WTF
