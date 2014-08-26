/*
 * Copyright (C) 2003, 2005, 2006, 2010, 2011 Apple Inc. All rights reserved.
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
#import "WebCoreNSStringExtras.h"
#import <mutex>
#import <wtf/Assertions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

static std::mutex& preferredLanguagesMutex()
{
    static dispatch_once_t onceToken;
    static std::mutex* mutex;

    dispatch_once(&onceToken, ^{
        mutex = std::make_unique<std::mutex>().release();
    });

    return *mutex;
}

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
        std::lock_guard<std::mutex> lock(WebCore::preferredLanguagesMutex());
        WebCore::preferredLanguages().clear();
    }

    WebCore::languageDidChange();
}

@end

namespace WebCore {

static String httpStyleLanguageCode(NSString *language)
{
    return [[NSLocale canonicalLanguageIdentifierFromString:canonicalLocaleName(language)] lowercaseString];
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

    std::lock_guard<std::mutex> lock(preferredLanguagesMutex());
    Vector<String>& userPreferredLanguages = preferredLanguages();

    if (userPreferredLanguages.isEmpty()) {
        RetainPtr<CFArrayRef> languages = adoptCF(CFLocaleCopyPreferredLanguages());
        CFIndex languageCount = CFArrayGetCount(languages.get());
        if (!languageCount)
            userPreferredLanguages.append("en");
        else {
            for (CFIndex i = 0; i < languageCount; i++)
                userPreferredLanguages.append(httpStyleLanguageCode((NSString *)CFArrayGetValueAtIndex(languages.get(), i)));
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
