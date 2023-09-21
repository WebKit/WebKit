/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "NotificationOptionsPayload.h"

#if ENABLE(DECLARATIVE_WEB_PUSH)

static NSString * const WebDirKey = @"WebDirKey";
static NSString * const WebLangKey = @"WebLangKey";
static NSString * const WebBodyKey = @"WebBodyKey";
static NSString * const WebTagKey = @"WebTagKey";
static NSString * const WebIconKey = @"WebIconKey";
static NSString * const WebDataJSONKey = @"WebDataJSONKey";
static NSString * const WebSilentKey = @"WebSilentKey";

namespace WebCore {

std::optional<NotificationOptionsPayload> NotificationOptionsPayload::fromDictionary(NSDictionary *dictionary)
{
    if (![dictionary isKindOfClass:[NSDictionary class]])
        return std::nullopt;

    NSNumber *dir = dictionary[WebDirKey];
    if (![dir isKindOfClass:[NSNumber class]])
        return std::nullopt;

    auto dirValue = [dir unsignedCharValue];
    if (!isValidEnum<NotificationDirection>(dirValue))
        return std::nullopt;
    auto rawDir = (NotificationDirection)dirValue;

    NSString *lang = dictionary[WebLangKey];
    NSString *body = dictionary[WebBodyKey];
    NSString *tag = dictionary[WebTagKey];
    NSString *icon = dictionary[WebIconKey];
    NSString *dataJSON = dictionary[WebDataJSONKey];

    NSNumber *silent = dictionary[WebSilentKey];
    if (!silent)
        return std::nullopt;

    std::optional<bool> rawSilent;
    if (![silent isKindOfClass:[NSNull class]]) {
        if (![silent isKindOfClass:[NSNumber class]])
            return std::nullopt;

        rawSilent = [silent boolValue];
    }

    return NotificationOptionsPayload { (NotificationDirection)rawDir, lang, body, tag, icon, dataJSON, rawSilent };
}

NSDictionary *NotificationOptionsPayload::dictionaryRepresentation() const
{
    return @{
        WebDirKey : @((uint8_t)dir),
        WebLangKey : (NSString *)lang,
        WebBodyKey : (NSString *)body,
        WebTagKey : (NSString *)tag,
        WebIconKey : (NSString *)icon,
        WebDataJSONKey : (NSString *)dataJSONString,
        WebSilentKey : silent.has_value() ? @(*silent) : [NSNull null],
    };
}

} // namespace WebKit

#endif // ENABLE(DECLARATIVE_WEB_PUSH)
