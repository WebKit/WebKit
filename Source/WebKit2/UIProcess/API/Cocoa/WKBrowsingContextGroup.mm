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

#if WK_API_ENABLED

#import "APIArray.h"
#import "APIString.h"
#import "WKArray.h"
#import "WKPageGroup.h"
#import "WKPreferences.h"
#import "WKRetainPtr.h"
#import "WKSharedAPICast.h"
#import "WKStringCF.h"
#import "WKURL.h"
#import "WKURLCF.h"
#import <wtf/Vector.h>

using namespace WebKit;

@implementation WKBrowsingContextGroup {
    API::ObjectStorage<WebPageGroup> _pageGroup;
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

    API::Object::constructInWrapper<WebPageGroup>(self, identifier);

    // Give the WKBrowsingContextGroup a identifier-less preferences, so that they
    // don't get automatically written to the disk. The automatic writing has proven
    // confusing to users of the API.
    WKRetainPtr<WKPreferencesRef> preferences = adoptWK(WKPreferencesCreate());
    WKPageGroupSetPreferences(toAPI(_pageGroup.get()), preferences.get());

    return self;
}

- (BOOL)allowsJavaScript
{
    return WKPreferencesGetJavaScriptEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())));
}

- (void)setAllowsJavaScript:(BOOL)allowsJavaScript
{
    WKPreferencesSetJavaScriptEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())), allowsJavaScript);
}

- (BOOL)allowsJavaScriptMarkup
{
    return WKPreferencesGetJavaScriptMarkupEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())));
}

- (void)setAllowsJavaScriptMarkup:(BOOL)allowsJavaScriptMarkup
{
    WKPreferencesSetJavaScriptMarkupEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())), allowsJavaScriptMarkup);
}

- (BOOL)allowsPlugIns
{
    return WKPreferencesGetPluginsEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())));
}

- (void)setAllowsPlugIns:(BOOL)allowsPlugIns
{
    WKPreferencesSetPluginsEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())), allowsPlugIns);
}

- (BOOL)privateBrowsingEnabled
{
    return WKPreferencesGetPrivateBrowsingEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())));
}

- (void)setPrivateBrowsingEnabled:(BOOL)enablePrivateBrowsing
{
    WKPreferencesSetPrivateBrowsingEnabled(WKPageGroupGetPreferences(toAPI(_pageGroup.get())), enablePrivateBrowsing);
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
            strings.uncheckedAppend(adoptRef(toImpl(WKStringCreateWithCFString((CFStringRef)entry))));
    }

    return toAPI(API::Array::create(std::move(strings)).leakRef());
}

-(void)addUserStyleSheet:(NSString *)source baseURL:(NSURL *)baseURL whitelistedURLPatterns:(NSArray *)whitelist blacklistedURLPatterns:(NSArray *)blacklist mainFrameOnly:(BOOL)mainFrameOnly
{
    if (!source)
        CRASH();

    WKRetainPtr<WKStringRef> wkSource = adoptWK(WKStringCreateWithCFString((CFStringRef)source));
    WKRetainPtr<WKURLRef> wkBaseURL = adoptWK(WKURLCreateWithCFURL((CFURLRef)baseURL));
    auto wkWhitelist = createWKArray(whitelist);
    auto wkBlacklist = createWKArray(blacklist);
    WKUserContentInjectedFrames injectedFrames = mainFrameOnly ? kWKInjectInTopFrameOnly : kWKInjectInAllFrames;

    WKPageGroupAddUserStyleSheet(toAPI(_pageGroup.get()), wkSource.get(), wkBaseURL.get(), wkWhitelist.get(), wkBlacklist.get(), injectedFrames);
}

- (void)removeAllUserStyleSheets
{
    WKPageGroupRemoveAllUserStyleSheets(toAPI(_pageGroup.get()));
}

- (void)addUserScript:(NSString *)source baseURL:(NSURL *)baseURL whitelistedURLPatterns:(NSArray *)whitelist blacklistedURLPatterns:(NSArray *)blacklist injectionTime:(WKUserScriptInjectionTime)injectionTime mainFrameOnly:(BOOL)mainFrameOnly
{
    if (!source)
        CRASH();

    WKRetainPtr<WKStringRef> wkSource = adoptWK(WKStringCreateWithCFString((CFStringRef)source));
    WKRetainPtr<WKURLRef> wkBaseURL = adoptWK(WKURLCreateWithCFURL((CFURLRef)baseURL));
    auto wkWhitelist = createWKArray(whitelist);
    auto wkBlacklist = createWKArray(blacklist);
    WKUserContentInjectedFrames injectedFrames = mainFrameOnly ? kWKInjectInTopFrameOnly : kWKInjectInAllFrames;

    WKPageGroupAddUserScript(toAPI(_pageGroup.get()), wkSource.get(), wkBaseURL.get(), wkWhitelist.get(), wkBlacklist.get(), injectedFrames, injectionTime);
}

- (void)removeAllUserScripts
{
    WKPageGroupRemoveAllUserScripts(toAPI(_pageGroup.get()));
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_pageGroup;
}

@end

@implementation WKBrowsingContextGroup (Private)

- (WKPageGroupRef)_pageGroupRef
{
    return toAPI(_pageGroup.get());
}

@end

#endif // WK_API_ENABLED
