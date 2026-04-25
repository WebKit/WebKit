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
#import "_WKNotificationDataInternal.h"

#import <WebCore/NotificationData.h>
#import <WebCore/NotificationDirection.h>
#import <WebCore/SecurityOriginData.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

static NSString *iconURLKey = @"iconURL";
static NSString *tagKey = @"tag";
static NSString *languageKey = @"language";
static NSString *dataKey = @"data";

@interface _WKNotificationData()
- (instancetype)_init;

@property (nonatomic, readwrite) NSString *title;
@property (nonatomic, readwrite) _WKNotificationDirection dir;
@property (nonatomic, readwrite) NSString *lang;
@property (nonatomic, readwrite) NSString *body;
@property (nonatomic, readwrite) NSString *tag;
@property (nonatomic, readwrite) _WKNotificationAlert alert;
@property (nonatomic, readwrite) NSData *data;
@property (nonatomic, readwrite) NSURL *serviceWorkerRegistrationURL;
@property (nonatomic, readwrite) NSUUID *uuid;

@end

@implementation _WKNotificationData {
@package
    WebCore::NotificationData _coreData;
}

- (instancetype)_init
{
    if (!(self = [super init]))
        return nil;

    return self;
}

- (instancetype)_initWithCoreData:(const WebCore::NotificationData&)coreData
{
    if (!(self = [super init]))
        return nil;

    _coreData = coreData;

    return self;
}

- (const WebCore::NotificationData&)_getCoreData
{
    return _coreData;
}

- (void)setTitle:(NSString *)title
{
    _coreData.title = title;
}

- (NSString *)title
{
    return _coreData.title.createNSString().autorelease();
}

- (void)setDir:(_WKNotificationDirection)dir
{
    switch (dir) {
    case _WKNotificationDirectionAuto:
        _coreData.direction = WebCore::NotificationDirection::Auto;
        break;
    case _WKNotificationDirectionLTR:
        _coreData.direction = WebCore::NotificationDirection::Ltr;
        break;
    case _WKNotificationDirectionRTL:
        _coreData.direction = WebCore::NotificationDirection::Rtl;
        break;
    };
}

- (_WKNotificationDirection)dir
{
    switch (_coreData.direction) {
    case WebCore::NotificationDirection::Auto:
        return _WKNotificationDirectionAuto;
    case WebCore::NotificationDirection::Ltr:
        return _WKNotificationDirectionLTR;
    case WebCore::NotificationDirection::Rtl:
        return _WKNotificationDirectionRTL;
    };
}

- (void)setLang:(NSString *)lang
{
    _coreData.language = lang;
}

- (NSString *)lang
{
    return _coreData.language.createNSString().autorelease();
}

- (void)setBody:(NSString *)body
{
    _coreData.body = body;
}

- (NSString *)body
{
    return _coreData.body.createNSString().autorelease();
}

- (void)setTag:(NSString *)tag
{
    _coreData.tag = tag;
}

- (NSString *)tag
{
    return _coreData.tag.createNSString().autorelease();
}

- (void)setAlert:(_WKNotificationAlert)alert
{
    switch (alert) {
    case _WKNotificationAlertDefault:
        _coreData.silent = std::nullopt;
        break;
    case _WKNotificationAlertSilent:
        _coreData.silent = true;
        break;
    case _WKNotificationAlertEnabled:
        _coreData.silent = false;
        break;
    }
}

- (_WKNotificationAlert)alert
{
    if (_coreData.silent == std::nullopt)
        return _WKNotificationAlertDefault;
    return *(_coreData.silent) ? _WKNotificationAlertSilent : _WKNotificationAlertEnabled;
}

- (void)setData:(NSData *)data
{
    _coreData.data = makeVector(data);
}

- (NSData *)data
{
    return toNSData(_coreData.data.span()).autorelease();
}

- (NSString *)origin
{
    return _coreData.originString.createNSString().autorelease();
}

- (void)setSecurityOrigin:(NSURL *)securityOrigin
{
    _coreData.originString = WebCore::SecurityOriginData::fromURL(securityOrigin).toString();
}

- (NSURL *)securityOrigin
{
    return URL { _coreData.originString }.createNSURL().autorelease();
}

- (void)setServiceWorkerRegistrationURL:(NSURL *)serviceWorkerRegistrationURL
{
    _coreData.serviceWorkerRegistrationURL = serviceWorkerRegistrationURL;
}

- (NSURL *)serviceWorkerRegistrationURL
{
    return _coreData.serviceWorkerRegistrationURL.createNSURL().autorelease();
}

- (NSString *)identifier
{
    return _coreData.notificationID.toString().createNSString().autorelease();
}

- (void)setUuid:(NSUUID *)uuid
{
    auto wtfUUID = WTF::UUID::fromNSUUID(uuid);
    if (wtfUUID)
        _coreData.notificationID = *wtfUUID;
}

- (NSUUID *)uuid
{
    return (NSUUID *)_coreData.notificationID.createNSUUID().autorelease();
}

- (NSDictionary *)userInfo
{
    return _coreData.dictionaryRepresentation();
}

- (NSDictionary *)dictionaryRepresentation
{
    return [self userInfo];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKNotificationData.class, self))
        return;
    [super dealloc];
}

@end

@implementation _WKMutableNotificationData

@dynamic title;
@dynamic dir;
@dynamic lang;
@dynamic body;
@dynamic tag;
@dynamic alert;
@dynamic data;
@dynamic securityOrigin;
@dynamic serviceWorkerRegistrationURL;
@dynamic uuid;

- (instancetype)init
{
    if (!(self = [super _init]))
        return nil;

    return self;
}

@end
