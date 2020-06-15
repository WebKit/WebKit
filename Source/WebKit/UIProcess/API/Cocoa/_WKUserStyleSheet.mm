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
#import "_WKUserStyleSheetInternal.h"

#import "APIArray.h"
#import "WKNSArray.h"
#import "WKNSURLExtras.h"
#import "WKWebViewInternal.h"
#import "WebKit2Initialize.h"
#import "WebPageProxy.h"
#import "_WKUserContentWorldInternal.h"
#import <wtf/cocoa/VectorCocoa.h>

@implementation _WKUserStyleSheet

- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly
{
    if (!(self = [super init]))
        return nil;

    // FIXME: In the API test, we can use generateUniqueURL below before the API::Object constructor has done this... where should this really be?
    WebKit::InitializeWebKit2();

    API::Object::constructInWrapper<API::UserStyleSheet>(self, WebCore::UserStyleSheet { source, API::UserStyleSheet::generateUniqueURL(), { }, { }, forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::UserStyleUserLevel }, API::ContentWorld::pageContentWorld());

    return self;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (instancetype)initWithSource:(NSString *)source forWKWebView:(WKWebView *)webView forMainFrameOnly:(BOOL)forMainFrameOnly level:(_WKUserStyleLevel)level userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    if (!(self = [super init]))
        return nil;

    // FIXME: In the API test, we can use generateUniqueURL below before the API::Object constructor has done this... where should this really be?
    WebKit::InitializeWebKit2();

    API::Object::constructInWrapper<API::UserStyleSheet>(self, WebCore::UserStyleSheet { source, API::UserStyleSheet::generateUniqueURL(), { }, { }, forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, API::toWebCoreUserStyleLevel(level), [webView _page]->webPageID() }, *userContentWorld->_contentWorld->_contentWorld);

    return self;
}

- (instancetype)initWithSource:(NSString *)source forWKWebView:(WKWebView *)webView forMainFrameOnly:(BOOL)forMainFrameOnly baseURL:(NSURL *)baseURL level:(_WKUserStyleLevel)level userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    if (!(self = [super init]))
        return nil;

    // FIXME: In the API test, we can use generateUniqueURL below before the API::Object constructor has done this... where should this really be?
    WebKit::InitializeWebKit2();

    API::Object::constructInWrapper<API::UserStyleSheet>(self, WebCore::UserStyleSheet { source, baseURL, { }, { }, forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, API::toWebCoreUserStyleLevel(level), [webView _page]->webPageID() }, *userContentWorld->_contentWorld->_contentWorld);

    return self;
}

- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    if (!(self = [super init]))
        return nil;

    // FIXME: In the API test, we can use generateUniqueURL below before the API::Object constructor has done this... where should this really be?
    WebKit::InitializeWebKit2();

    API::Object::constructInWrapper<API::UserStyleSheet>(self, WebCore::UserStyleSheet { source, API::UserStyleSheet::generateUniqueURL(), makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::UserStyleUserLevel }, *userContentWorld->_contentWorld->_contentWorld);

    return self;
}

- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist baseURL:(NSURL *)baseURL userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    if (!(self = [super init]))
        return nil;

    // FIXME: In the API test, we can use generateUniqueURL below before the API::Object constructor has done this... where should this really be?
    WebKit::InitializeWebKit2();

    API::Object::constructInWrapper<API::UserStyleSheet>(self, WebCore::UserStyleSheet { source, baseURL, makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::UserStyleUserLevel }, *userContentWorld->_contentWorld->_contentWorld);

    return self;
}

- (instancetype)initWithSource:(NSString *)source forMainFrameOnly:(BOOL)forMainFrameOnly legacyWhitelist:(NSArray<NSString *> *)legacyWhitelist legacyBlacklist:(NSArray<NSString *> *)legacyBlacklist baseURL:(NSURL *)baseURL level:(_WKUserStyleLevel)level userContentWorld:(_WKUserContentWorld *)userContentWorld
{
    if (!(self = [super init]))
        return nil;

    // FIXME: In the API test, we can use generateUniqueURL below before the API::Object constructor has done this... where should this really be?
    WebKit::InitializeWebKit2();

    API::Object::constructInWrapper<API::UserStyleSheet>(self, WebCore::UserStyleSheet { source, baseURL, makeVector<String>(legacyWhitelist), makeVector<String>(legacyBlacklist), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, API::toWebCoreUserStyleLevel(level) }, *userContentWorld->_contentWorld->_contentWorld);

    return self;
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (instancetype)initWithSource:(NSString *)source forWKWebView:(WKWebView *)webView forMainFrameOnly:(BOOL)forMainFrameOnly includeMatchPatternStrings:(NSArray<NSString *> *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray<NSString *> *)excludeMatchPatternStrings baseURL:(NSURL *)baseURL level:(_WKUserStyleLevel)level contentWorld:(WKContentWorld *)contentWorld
{

    WebKit::InitializeWebKit2();

    API::Object::constructInWrapper<API::UserStyleSheet>(self, WebCore::UserStyleSheet { source, baseURL, makeVector<String>(includeMatchPatternStrings), makeVector<String>(excludeMatchPatternStrings), forMainFrameOnly ? WebCore::UserContentInjectedFrames::InjectInTopFrameOnly : WebCore::UserContentInjectedFrames::InjectInAllFrames, API::toWebCoreUserStyleLevel(level), webView ? Optional<WebCore::PageIdentifier>([webView _page]->webPageID()) : WTF::nullopt }, contentWorld ? *contentWorld->_contentWorld : API::ContentWorld::pageContentWorld());

    return self;
}


- (void)dealloc
{
    _userStyleSheet->~UserStyleSheet();

    [super dealloc];
}

- (NSString *)source
{
    return _userStyleSheet->userStyleSheet().source();
}

- (NSURL *)baseURL
{
    return _userStyleSheet->userStyleSheet().url();
}

- (BOOL)isForMainFrameOnly
{
    return _userStyleSheet->userStyleSheet().injectedFrames() == WebCore::UserContentInjectedFrames::InjectInTopFrameOnly;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_userStyleSheet;
}

@end
