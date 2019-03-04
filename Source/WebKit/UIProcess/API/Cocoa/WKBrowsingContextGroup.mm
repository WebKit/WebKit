/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "WKBrowsingContextGroupInternal.h"
#import "WKBrowsingContextGroupPrivate.h"

#import "APIArray.h"
#import "APIString.h"
#import "WKAPICast.h"
#import "WKArray.h"
#import "WKPageGroup.h"
#import "WKPreferencesRef.h"
#import "WKRetainPtr.h"
#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import "WKURL.h"
#import "WKURLCF.h"
#import <wtf/Vector.h>

IGNORE_WARNINGS_BEGIN("deprecated-implementations")
@implementation WKBrowsingContextGroup {
IGNORE_WARNINGS_END
    API::ObjectStorage<WebKit::WebPageGroup> _pageGroup;
}

- (void)dealloc
{
    _pageGroup->~WebPageGroup();

    [super dealloc];
}

- (id)initWithIdentifier:(NSString *)identifier
{
    self = [super init];
    if (!self)
        return nil;

    API::Object::constructInWrapper<WebKit::WebPageGroup>(self, identifier);

    // Give the WKBrowsingContextGroup a identifier-less preferences, so that they
    // don't get automatically written to the disk. The automatic writing has proven
    // confusing to users of the API.
    WKRetainPtr<WKPreferencesRef> preferences = adoptWK(WKPreferencesCreate());
    WKPageGroupSetPreferences(WebKit::toAPI(_pageGroup.get()), preferences.get());

    return self;
}

- (BOOL)allowsJavaScript
{
    return WKPreferencesGetJavaScriptEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())));
}

- (void)setAllowsJavaScript:(BOOL)allowsJavaScript
{
    WKPreferencesSetJavaScriptEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())), allowsJavaScript);
}

- (BOOL)allowsJavaScriptMarkup
{
    return WKPreferencesGetJavaScriptMarkupEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())));
}

- (void)setAllowsJavaScriptMarkup:(BOOL)allowsJavaScriptMarkup
{
    WKPreferencesSetJavaScriptMarkupEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())), allowsJavaScriptMarkup);
}

- (BOOL)allowsPlugIns
{
    return WKPreferencesGetPluginsEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())));
}

- (void)setAllowsPlugIns:(BOOL)allowsPlugIns
{
    WKPreferencesSetPluginsEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())), allowsPlugIns);
}

- (BOOL)privateBrowsingEnabled
{
    return WKPreferencesGetPrivateBrowsingEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())));
}

- (void)setPrivateBrowsingEnabled:(BOOL)enablePrivateBrowsing
{
    WKPreferencesSetPrivateBrowsingEnabled(WKPageGroupGetPreferences(WebKit::toAPI(_pageGroup.get())), enablePrivateBrowsing);
}

static WKRetainPtr<WKArrayRef> createWKArray(NSArray *array)
{
    NSUInteger count = [array count];

    if (!count)
        return nullptr;

    Vector<RefPtr<API::Object>> strings;
    strings.reserveInitialCapacity(count);

    for (id entry in array) {
        if ([entry isKindOfClass:[NSString class]])
            strings.uncheckedAppend(adoptRef(WebKit::toImpl(WKStringCreateWithCFString((__bridge CFStringRef)entry))));
    }

    return WebKit::toAPI(&API::Array::create(WTFMove(strings)).leakRef());
}

-(void)addUserStyleSheet:(NSString *)source baseURL:(NSURL *)baseURL whitelistedURLPatterns:(NSArray *)whitelist blacklistedURLPatterns:(NSArray *)blacklist mainFrameOnly:(BOOL)mainFrameOnly
{
    if (!source)
        CRASH();

    WKRetainPtr<WKStringRef> wkSource = adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)source));
    WKRetainPtr<WKURLRef> wkBaseURL = adoptWK(WKURLCreateWithCFURL((__bridge CFURLRef)baseURL));
    auto wkWhitelist = createWKArray(whitelist);
    auto wkBlacklist = createWKArray(blacklist);
    WKUserContentInjectedFrames injectedFrames = mainFrameOnly ? kWKInjectInTopFrameOnly : kWKInjectInAllFrames;

    WKPageGroupAddUserStyleSheet(WebKit::toAPI(_pageGroup.get()), wkSource.get(), wkBaseURL.get(), wkWhitelist.get(), wkBlacklist.get(), injectedFrames);
}

- (void)removeAllUserStyleSheets
{
    WKPageGroupRemoveAllUserStyleSheets(WebKit::toAPI(_pageGroup.get()));
}

- (void)addUserScript:(NSString *)source baseURL:(NSURL *)baseURL whitelistedURLPatterns:(NSArray *)whitelist blacklistedURLPatterns:(NSArray *)blacklist injectionTime:(_WKUserScriptInjectionTime)injectionTime mainFrameOnly:(BOOL)mainFrameOnly
{
    if (!source)
        CRASH();

    WKRetainPtr<WKStringRef> wkSource = adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)source));
    WKRetainPtr<WKURLRef> wkBaseURL = adoptWK(WKURLCreateWithCFURL((__bridge CFURLRef)baseURL));
    auto wkWhitelist = createWKArray(whitelist);
    auto wkBlacklist = createWKArray(blacklist);
    WKUserContentInjectedFrames injectedFrames = mainFrameOnly ? kWKInjectInTopFrameOnly : kWKInjectInAllFrames;

    WKPageGroupAddUserScript(WebKit::toAPI(_pageGroup.get()), wkSource.get(), wkBaseURL.get(), wkWhitelist.get(), wkBlacklist.get(), injectedFrames, injectionTime);
}

- (void)removeAllUserScripts
{
    WKPageGroupRemoveAllUserScripts(WebKit::toAPI(_pageGroup.get()));
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_pageGroup;
}

@end

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
IGNORE_WARNINGS_BEGIN("deprecated-implementations")
@implementation WKBrowsingContextGroup (Private)
IGNORE_WARNINGS_END

- (WKPageGroupRef)_pageGroupRef
{
    return WebKit::toAPI(_pageGroup.get());
}

@end
ALLOW_DEPRECATED_DECLARATIONS_END
