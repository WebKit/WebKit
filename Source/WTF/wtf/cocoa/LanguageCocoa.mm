/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import <wtf/Language.h>

#import <wtf/Logging.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/cocoa/NSLocaleSPI.h>
#import <wtf/text/TextStream.h>
#import <wtf/text/WTFString.h>

namespace WTF {

size_t indexOfBestMatchingLanguageInList(const String& language, const Vector<String>& languageList, bool& exactMatch)
{
    auto matchedLanguages = retainPtr([NSLocale matchedLanguagesFromAvailableLanguages:createNSArray(languageList).get() forPreferredLanguages:@[ static_cast<NSString *>(language) ]]);
    if (![matchedLanguages count]) {
        exactMatch = false;
        return notFound;
    }

    String firstMatchedLanguage = [matchedLanguages firstObject];

    exactMatch = language == firstMatchedLanguage;

    auto index = languageList.find(firstMatchedLanguage);
    ASSERT(index < languageList.size());
    return index;
}

LocaleComponents parseLocale(const String& localeIdentifier)
{
    auto locale = retainPtr([NSLocale localeWithLocaleIdentifier:localeIdentifier]);

    return {
        locale.get().languageCode,
        locale.get().scriptCode,
        locale.get().countryCode
    };
}

bool canMinimizeLanguages()
{
    static const bool result = []() -> bool {
        return linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::MinimizesLanguages) && [NSLocale respondsToSelector:@selector(minimizedLanguagesFromLanguages:)];
    }();
    return result;
}

RetainPtr<CFArrayRef> minimizedLanguagesFromLanguages(CFArrayRef languages)
{
    if (!canMinimizeLanguages()) {
        LOG(Language, "Could not minimize languages.");
        return languages;
    }

ALLOW_NEW_API_WITHOUT_GUARDS_BEGIN
    return (__bridge CFArrayRef)[NSLocale minimizedLanguagesFromLanguages:(__bridge NSArray<NSString *> *)languages];
ALLOW_NEW_API_WITHOUT_GUARDS_END
}

void overrideUserPreferredLanguages(const Vector<String>& override)
{
    LOG_WITH_STREAM(Language, stream << "Languages are being overridden to: " << override);
    NSDictionary *existingArguments = [[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain];
    auto newArguments = adoptNS([existingArguments mutableCopy]);
    [newArguments setValue:createNSArray(override).get() forKey:@"AppleLanguages"];
    [[NSUserDefaults standardUserDefaults] setVolatileDomain:newArguments.get() forName:NSArgumentDomain];
    languageDidChange();
}

}
