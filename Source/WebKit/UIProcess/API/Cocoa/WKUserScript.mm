/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKUserScriptInternal.h"

#import "_WKUserContentWorldInternal.h"
#import <wtf/cocoa/VectorCocoa.h>

@implementation WKUserScript

- (instancetype)initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::UserScript>(self, WebCore::UserScript { source, API::UserScript::generateUniqueURL(), { }, { }, API::toWebCoreUserScriptInjectionTime(injectionTime), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::WaitForNotificationBeforeInjecting::No }, API::ContentWorld::pageContentWorld());

    return self;
}

- (void)dealloc
{
    _userScript->~UserScript();

    [super dealloc];
}

- (NSString *)source
{
    return _userScript->userScript().source();
}

- (WKUserScriptInjectionTime)injectionTime
{
    return API::toWKUserScriptInjectionTime(_userScript->userScript().injectionTime());
}

- (BOOL)isForMainFrameOnly
{
    return _userScript->userScript().injectedFrames() == WebCore::UserContentInjectedFrames::InjectInTopFrameOnly;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_userScript;
}

@end

@implementation WKUserScript (WKPrivate)

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray *)legacyWhitelist legacyBlacklist:(NSArray *)legacyBlacklist userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::UserScript>(self, WebCore::UserScript { source, API::UserScript::generateUniqueURL(), makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), API::toWebCoreUserScriptInjectionTime(injectionTime), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::WaitForNotificationBeforeInjecting::No }, *userContentWorld->_contentWorld->_contentWorld);

    return self;
}

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray *)legacyWhitelist legacyBlacklist:(NSArray *)legacyBlacklist associatedURL:(NSURL *)associatedURL userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::UserScript>(self, WebCore::UserScript { source, associatedURL, makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), API::toWebCoreUserScriptInjectionTime(injectionTime), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::WaitForNotificationBeforeInjecting::No }, *userContentWorld->_contentWorld->_contentWorld);

    return self;
}

- (_WKUserContentWorld *)_userContentWorld
{
    return [[[_WKUserContentWorld alloc] _initWithContentWorld:wrapper(_userScript->contentWorld())] autorelease];
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray *)legacyWhitelist legacyBlacklist:(NSArray *)legacyBlacklist contentWorld:(WKContentWorld *)contentWorld
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::UserScript>(self, WebCore::UserScript { source, API::UserScript::generateUniqueURL(), makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), API::toWebCoreUserScriptInjectionTime(injectionTime), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::WaitForNotificationBeforeInjecting::No }, *contentWorld->_contentWorld);

    return self;
}

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray *)legacyWhitelist legacyBlacklist:(NSArray *)legacyBlacklist associatedURL:(NSURL *)associatedURL contentWorld:(WKContentWorld *)contentWorld
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::UserScript>(self, WebCore::UserScript { source, associatedURL, makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), API::toWebCoreUserScriptInjectionTime(injectionTime), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::WaitForNotificationBeforeInjecting::No }, *contentWorld->_contentWorld);

    return self;
}

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist associatedURL:(NSURL *)associatedURL contentWorld:(WKContentWorld *)contentWorld deferRunningUntilNotification:(BOOL)deferRunningUntilNotification
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::UserScript>(self, WebCore::UserScript { source, associatedURL, makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), API::toWebCoreUserScriptInjectionTime(injectionTime), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, deferRunningUntilNotification ? WebCore::WaitForNotificationBeforeInjecting::Yes : WebCore::WaitForNotificationBeforeInjecting::No }, *contentWorld->_contentWorld);

    return self;
}

- (instancetype)_initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)injectionTime forMainFrameOnly:(BOOL)forMainFrameOnly includeMatchPatternStrings:(NSArray<NSString *> *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray<NSString *> *)excludeMatchPatternStrings associatedURL:(NSURL *)associatedURL contentWorld:(WKContentWorld *)contentWorld deferRunningUntilNotification:(BOOL)deferRunningUntilNotification
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::UserScript>(self, WebCore::UserScript { source, associatedURL, makeVector<String>(includeMatchPatternStrings), makeVector<String>(excludeMatchPatternStrings), API::toWebCoreUserScriptInjectionTime(injectionTime), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, deferRunningUntilNotification ? WebCore::WaitForNotificationBeforeInjecting::Yes : WebCore::WaitForNotificationBeforeInjecting::No }, *contentWorld->_contentWorld);

    return self;
}

- (WKContentWorld *)_contentWorld
{
    return wrapper(_userScript->contentWorld());
}

@end
