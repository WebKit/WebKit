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
#import "NotificationPayload.h"

#if ENABLE(DECLARATIVE_WEB_PUSH)

static NSString * const WebNotificationDefaultActionKey = @"WebNotificationDefaultActionKey";
static NSString * const WebNotificationAppBadgeKey = @"WebNotificationAppBadgeKey";
static NSString * const WebNotificationOptionsKey = @"WebNotificationOptionsKey";
static NSString * const WebNotificationMutableKey = @"WebNotificationMutableKey";

namespace WebCore {

std::optional<NotificationPayload> NotificationPayload::fromDictionary(NSDictionary *dictionary)
{
    NSURL *defaultAction = dictionary[WebNotificationDefaultActionKey];
    if (!defaultAction)
        return std::nullopt;

    NSString *title = dictionary[WebNotificationTitleKey];
    if (!title)
        return std::nullopt;

    NSNumber *appBadge = dictionary[WebNotificationAppBadgeKey];
    if (!appBadge)
        return std::nullopt;

    std::optional<uint64_t> rawAppBadge;
    if (![appBadge isKindOfClass:[NSNull class]]) {
        if (![appBadge isKindOfClass:[NSNumber class]])
            return std::nullopt;
        rawAppBadge = [appBadge unsignedLongLongValue];
    }

    NSDictionary *options = dictionary[WebNotificationOptionsKey];
    if (!options)
        return std::nullopt;

    std::optional<NotificationOptionsPayload> rawOptions;
    if (![options isKindOfClass:[NSNull class]]) {
        rawOptions = NotificationOptionsPayload::fromDictionary(options);
        if (!rawOptions)
            return std::nullopt;
    }

    NSNumber *isMutable = dictionary[WebNotificationMutableKey];
    if (!isMutable)
        return std::nullopt;

    return NotificationPayload { defaultAction, title, WTFMove(rawAppBadge), WTFMove(rawOptions), !![isMutable boolValue] };
}

NSDictionary *NotificationPayload::dictionaryRepresentation() const
{
    id nsAppBadge = appBadge ? @(*appBadge) : [NSNull null];
    id nsOptions = options ? options->dictionaryRepresentation() : [NSNull null];

    return @{
        WebNotificationDefaultActionKey : (NSURL *)defaultActionURL,
        WebNotificationTitleKey : (NSString *)title,
        WebNotificationAppBadgeKey : nsAppBadge,
        WebNotificationOptionsKey : nsOptions,
        WebNotificationMutableKey : @(isMutable),
    };
}

} // namespace WebKit

#endif // ENABLE(DECLARATIVE_WEB_PUSH)
