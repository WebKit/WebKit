/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "TextEncodingRegistry.h"

#if PLATFORM(IOS_FAMILY)

namespace WebCore {

CFStringEncoding webDefaultCFStringEncoding()
{
    // FIXME: we can do better than this hard-coded list once this radar is addressed:
    // <rdar://problem/4433165> Need API that can get preferred web (and mail) encoding(s) w/o region code.
    // Alternatively, we could have our own table of preferred encodings in WebKit, shared with Mac.

    NSArray *preferredLanguages = [NSLocale preferredLanguages];
    if (!preferredLanguages.count)
        return kCFStringEncodingISOLatin1;

    // We pass in the first language as "en" because if preferredLocalizationsFromArray:forPreferences:
    // doesn't find a match, it will return the result of the first value in languagesWithCustomDefaultEncodings.
    // We could really pass anything as this first value, as long as it's not something we're trying to match against
    // below. We don't want to implictly default to "zh-Hans", that's why it's not first in the array.
    NSArray *languagesWithCustomDefaultEncodings = @[ @"en", @"zh-Hans", @"zh-Hant", @"zh-HK", @"ja", @"ko", @"ru" ];
    NSArray *languagesToUse = [NSBundle preferredLocalizationsFromArray:languagesWithCustomDefaultEncodings forPreferences:@[ [preferredLanguages firstObject] ]];
    if (!languagesToUse.count)
        return kCFStringEncodingISOLatin1;

    NSString *firstLanguage = languagesToUse.firstObject;
    if ([firstLanguage isEqualToString:@"zh-Hans"])
        return kCFStringEncodingEUC_CN;
    if ([firstLanguage isEqualToString:@"zh-Hant"])
        return kCFStringEncodingBig5;
    if ([firstLanguage isEqualToString:@"zh-HK"])
        return kCFStringEncodingBig5_HKSCS_1999;
    if ([firstLanguage isEqualToString:@"ja"])
        return kCFStringEncodingShiftJIS;
    if ([firstLanguage isEqualToString:@"ko"])
        return kCFStringEncodingEUC_KR;
    if ([firstLanguage isEqualToString:@"ru"])
        return kCFStringEncodingWindowsCyrillic;

    return kCFStringEncodingISOLatin1;
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
