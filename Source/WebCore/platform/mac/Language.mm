/*
 * Copyright (C) 2003, 2005, 2006, 2010, 2011, 2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Language.h"

#import "BlockExceptions.h"
#import "CFBundleSPI.h"
#import "WebCoreNSStringExtras.h"
#import <mutex>
#import <unicode/uloc.h>
#import <wtf/Assertions.h>
#import <wtf/Lock.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

static StaticLock preferredLanguagesMutex;

static Vector<String>& preferredLanguages()
{
    static NeverDestroyed<Vector<String>> languages;
    return languages;
}

}

@interface WebLanguageChangeObserver : NSObject
@end

@implementation WebLanguageChangeObserver

+ (void)languagePreferencesDidChange:(NSNotification *)notification
{
    UNUSED_PARAM(notification);

    {
        std::lock_guard<StaticLock> lock(WebCore::preferredLanguagesMutex);
        WebCore::preferredLanguages().clear();
    }

    WebCore::languageDidChange();
}

@end

namespace WebCore {

static String httpStyleLanguageCode(NSString *language, NSString *country)
{
    SInt32 languageCode;
    SInt32 regionCode; 
    SInt32 scriptCode; 
    CFStringEncoding stringEncoding;
    
    bool languageDidSpecifyExplicitVariant = [language rangeOfCharacterFromSet:[NSCharacterSet characterSetWithCharactersInString:@"-_"]].location != NSNotFound;

    // FIXME: This transformation is very wrong:
    // 1. There is no reason why CFBundle localization names would be at all related to language names as used on the Web.
    // 2. Script Manager codes cannot represent all languages that are now supported by the platform, so the conversion is lossy.
    // 3. This should probably match what is sent by the network layer as Accept-Language, but currently, that's implemented separately.
    CFBundleGetLocalizationInfoForLocalization((CFStringRef)language, &languageCode, &regionCode, &scriptCode, &stringEncoding);
    RetainPtr<CFStringRef> preferredLanguageCode = adoptCF(CFBundleCopyLocalizationForLocalizationInfo(languageCode, regionCode, scriptCode, stringEncoding));
    if (preferredLanguageCode)
        language = (NSString *)preferredLanguageCode.get();

    // Make the string lowercase.
    NSString *lowercaseLanguageCode = [language lowercaseString];
    NSString *lowercaseCountryCode = [country lowercaseString];
    
    // If we see a "_" after a 2-letter language code:
    // If the country is valid and the language did not specify a variant, replace the "_" and
    // whatever comes after it with "-" followed by the country code.
    // Otherwise, replace the "_" with a "-" and use whatever country
    // CFBundleCopyLocalizationForLocalizationInfo() returned.
    if ([lowercaseLanguageCode length] >= 3 && [lowercaseLanguageCode characterAtIndex:2] == '_') {
        if (country && !languageDidSpecifyExplicitVariant)
            return [NSString stringWithFormat:@"%@-%@", [lowercaseLanguageCode substringWithRange:NSMakeRange(0, 2)], lowercaseCountryCode];
        
        // Fall back to older behavior, which used the original language-based code but just changed
        // the "_" to a "-".
        RetainPtr<NSMutableString> mutableLanguageCode = adoptNS([lowercaseLanguageCode mutableCopy]);
        [mutableLanguageCode.get() replaceCharactersInRange:NSMakeRange(2, 1) withString:@"-"];
        return mutableLanguageCode.get();
    }

    return lowercaseLanguageCode;
}

static bool isValidICUCountryCode(NSString* countryCode)
{
    const char* const* countries = uloc_getISOCountries();
    const char* countryUTF8 = [countryCode UTF8String];
    for (unsigned i = 0; countries[i]; ++i) {
        const char* possibleCountry = countries[i];
        if (!strcmp(countryUTF8, possibleCountry))
            return true;
    }
    return false;
}

Vector<String> platformUserPreferredLanguages()
{
#if PLATFORM(MAC)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [[NSDistributedNotificationCenter defaultCenter] addObserver:[WebLanguageChangeObserver self] selector:@selector(languagePreferencesDidChange:) name:@"AppleLanguagePreferencesChangedNotification" object:nil];
    });
#endif

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    std::lock_guard<StaticLock> lock(preferredLanguagesMutex);
    Vector<String>& userPreferredLanguages = preferredLanguages();

    if (userPreferredLanguages.isEmpty()) {
        RetainPtr<CFLocaleRef> locale = adoptCF(CFLocaleCopyCurrent());
        NSString *countryCode = (NSString *)CFLocaleGetValue(locale.get(), kCFLocaleCountryCode);
        
        if (!isValidICUCountryCode(countryCode))
            countryCode = nil;
        
        RetainPtr<CFArrayRef> languages = adoptCF(CFLocaleCopyPreferredLanguages());
        CFIndex languageCount = CFArrayGetCount(languages.get());
        if (!languageCount)
            userPreferredLanguages.append("en");
        else {
            for (CFIndex i = 0; i < languageCount; i++)
                userPreferredLanguages.append(httpStyleLanguageCode((NSString *)CFArrayGetValueAtIndex(languages.get(), i), countryCode));
        }
    }

    Vector<String> userPreferredLanguagesCopy;
    userPreferredLanguagesCopy.reserveInitialCapacity(userPreferredLanguages.size());

    for (auto& language : userPreferredLanguages)
        userPreferredLanguagesCopy.uncheckedAppend(language.isolatedCopy());

    return userPreferredLanguagesCopy;

    END_BLOCK_OBJC_EXCEPTIONS;

    return Vector<String>();
}

}
