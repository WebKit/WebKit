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

static NSString *iconURLKey = @"iconURL";
static NSString *tagKey = @"tag";
static NSString *languageKey = @"language";
static NSString *dataKey = @"data";

@implementation _WKNotificationData

- (instancetype)initWithCoreData:(const WebCore::NotificationData&)coreData dataStore:(WKWebsiteDataStore *)dataStore
{
    self = [super init];
    if (!self)
        return nil;

    _title = [(NSString *)coreData.title retain];
    _body = [(NSString *)coreData.body retain];
    _origin = [(NSString *)coreData.originString retain];
    _identifier = [(NSString *)coreData.notificationID.toString() retain];
    _userInfo = [coreData.dictionaryRepresentation() retain];

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKNotificationData.class, self))
        return;

    [_title release];
    [_body release];
    [_origin release];
    [_identifier release];
    [_userInfo release];

    [super dealloc];
}

@end
