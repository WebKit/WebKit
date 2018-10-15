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

static Vector<String>& preferredLanguages()
{
    static NeverDestroyed<Vector<String>> languages;
    return languages;
}

static String httpStyleLanguageCode(CFStringRef language)
{
    SInt32 languageCode;
    SInt32 regionCode;
    SInt32 scriptCode;
    CFStringEncoding stringEncoding;

    // FIXME: This transformation is very wrong:
    // 1. There is no reason why CFBundle localization names would be at all related to language names as used on the Web.
    // 2. Script Manager codes cannot represent all languages that are now supported by the platform, so the conversion is lossy.
    // 3. This should probably match what is sent by the network layer as Accept-Language, but currently, that's implemented separately.
    CFBundleGetLocalizationInfoForLocalization(language, &languageCode, &regionCode, &scriptCode, &stringEncoding);
    RetainPtr<CFStringRef> preferredLanguageCode = adoptCF(CFBundleCopyLocalizationForLocalizationInfo(languageCode, regionCode, scriptCode, stringEncoding));
    if (preferredLanguageCode)
        language = preferredLanguageCode.get();

    // Turn a '_' into a '-' if it appears after a 2-letter language code
    if (CFStringGetLength(language) >= 3 && CFStringGetCharacterAtIndex(language, 2) == '_') {
        auto mutableLanguageCode = adoptCF(CFStringCreateMutableCopy(kCFAllocatorDefault, 0, language));
        CFStringReplace(mutableLanguageCode.get(), CFRangeMake(2, 1), CFSTR("-"));
        return mutableLanguageCode.get();
    }

    return language;
}

#if PLATFORM(MAC)
static void languagePreferencesDidChange(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    {
        std::lock_guard<Lock> lock(preferredLanguagesMutex);
        preferredLanguages().clear();
    }
    
    languageDidChange();
}
#endif

Vector<String> platformUserPreferredLanguages()
{
#if PLATFORM(MAC)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^ {
        CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), nullptr, &languagePreferencesDidChange, CFSTR("AppleLanguagePreferencesChangedNotification"), nullptr, CFNotificationSuspensionBehaviorCoalesce);
    });
#endif

    std::lock_guard<Lock> lock(preferredLanguagesMutex);
    Vector<String>& userPreferredLanguages = preferredLanguages();

    if (userPreferredLanguages.isEmpty()) {
        RetainPtr<CFArrayRef> languages = adoptCF(CFLocaleCopyPreferredLanguages());
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
