/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "NotificationData.h"

#import "NotificationDirection.h"
#import <wtf/cocoa/VectorCocoa.h>

static NSString * const WebNotificationDefaultActionURLKey = @"WebNotificationDefaultActionURLKey";
static NSString * const WebNotificationTitleKey = @"WebNotificationTitleKey";
static NSString * const WebNotificationBodyKey = @"WebNotificationBodyKey";
static NSString * const WebNotificationIconURLKey = @"WebNotificationIconURLKey";
static NSString * const WebNotificationTagKey = @"WebNotificationTagKey";
static NSString * const WebNotificationLanguageKey = @"WebNotificationLanguageKey";
static NSString * const WebNotificationDirectionKey = @"WebNotificationDirectionKey";
static NSString * const WebNotificationOriginKey = @"WebNotificationOriginKey";
static NSString * const WebNotificationServiceWorkerRegistrationURLKey = @"WebNotificationServiceWorkerRegistrationURLKey";
static NSString * const WebNotificationUUIDStringKey = @"WebNotificationUUIDStringKey";
static NSString * const WebNotificationContextUUIDStringKey = @"WebNotificationContextUUIDStringKey";
static NSString * const WebNotificationSessionIDKey = @"WebNotificationSessionIDKey";
static NSString * const WebNotificationDataKey = @"WebNotificationDataKey";
static NSString * const WebNotificationSilentKey = @"WebNotificationSilentKey";

namespace WebCore {

static std::optional<bool> nsValueToOptionalBool(id value)
{
    if (![value isKindOfClass:[NSNumber class]])
        return std::nullopt;

    return [(NSNumber *)value boolValue];
}

std::optional<NotificationData> NotificationData::fromDictionary(NSDictionary *dictionary)
{
    RetainPtr<NSString> defaultActionURL = dictionary[WebNotificationDefaultActionURLKey];
    RetainPtr<NSString> title = dictionary[WebNotificationTitleKey];
    RetainPtr<NSString> body = dictionary[WebNotificationBodyKey];
    RetainPtr<NSString> iconURL = dictionary[WebNotificationIconURLKey];
    RetainPtr<NSString> tag = dictionary[WebNotificationTagKey];
    RetainPtr<NSString> language = dictionary[WebNotificationLanguageKey];
    RetainPtr<NSString> originString = dictionary[WebNotificationOriginKey];
    RetainPtr<NSString> serviceWorkerRegistrationURL = dictionary[WebNotificationServiceWorkerRegistrationURLKey];
    RetainPtr<NSNumber> sessionID = dictionary[WebNotificationSessionIDKey];
    RetainPtr<NSData> notificationData = dictionary[WebNotificationDataKey];

    String uuidString = dictionary[WebNotificationUUIDStringKey];
    auto uuid = WTF::UUID::parseVersion4(uuidString);
    if (!uuid)
        return std::nullopt;

    std::optional<ScriptExecutionContextIdentifier> contextIdentifier;
    String contextUUIDString = dictionary[WebNotificationContextUUIDStringKey];
    if (!contextUUIDString.isEmpty()) {
        auto contextUUID = WTF::UUID::parseVersion4(contextUUIDString);
        if (!contextUUID)
            return std::nullopt;

        contextIdentifier = ScriptExecutionContextIdentifier { *contextUUID, Process::identifier() };
    }

    NotificationDirection direction;
    RetainPtr<NSNumber> directionNumber = dictionary[WebNotificationDirectionKey];
    switch ((NotificationDirection)(directionNumber.get().unsignedLongValue)) {
    case NotificationDirection::Auto:
    case NotificationDirection::Ltr:
    case NotificationDirection::Rtl:
        direction = (NotificationDirection)directionNumber.get().unsignedLongValue;
        break;
    default:
        return std::nullopt;
    }

    NotificationData data { URL { String { defaultActionURL.get() } }, title.get(), body.get(), iconURL.get(), tag.get(), language.get(), direction, originString.get(), URL { String { serviceWorkerRegistrationURL.get() } }, *uuid, contextIdentifier, PAL::SessionID { sessionID.get().unsignedLongLongValue }, { }, makeVector(notificationData.get()), nsValueToOptionalBool(dictionary[WebNotificationSilentKey]) };
    return WTF::move(data);
}

NSDictionary *NotificationData::dictionaryRepresentation() const
{
    RetainPtr result = adoptNS(@{
        WebNotificationDefaultActionURLKey : navigateURL.string().createNSString().get(),
        WebNotificationTitleKey : title.createNSString().get(),
        WebNotificationBodyKey : body.createNSString().get(),
        WebNotificationIconURLKey : iconURL.createNSString().get(),
        WebNotificationTagKey : tag.createNSString().get(),
        WebNotificationLanguageKey : language.createNSString().get(),
        WebNotificationOriginKey : originString.createNSString().get(),
        WebNotificationDirectionKey : @((unsigned long)direction),
        WebNotificationServiceWorkerRegistrationURLKey : serviceWorkerRegistrationURL.string().createNSString().get(),
        WebNotificationUUIDStringKey : notificationID.toString().createNSString().get(),
        WebNotificationSessionIDKey : @(sourceSession.toUInt64()),
        WebNotificationDataKey: toNSData(data).autorelease(),
    }.mutableCopy);

    if (contextIdentifier)
        result.get()[WebNotificationContextUUIDStringKey] = contextIdentifier->toString().createNSString().get();

    if (silent != std::nullopt)
        result.get()[WebNotificationSilentKey] = @(*silent);

    return result.autorelease();
}

} // namespace WebKit
