/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#import "WKWebpagePreferencesInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "_WKWebsitePoliciesInternal.h"

@implementation _WKWebsitePolicies

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _webpagePreferences = adoptNS([[WKWebpagePreferences alloc] init]);
    return self;
}

- (WKWebpagePreferences *)webpagePreferences
{
    return _webpagePreferences.get();
}

- (void)setContentBlockersEnabled:(BOOL)contentBlockersEnabled
{
    [_webpagePreferences _setContentBlockersEnabled:contentBlockersEnabled];
}

- (BOOL)contentBlockersEnabled
{
    return [_webpagePreferences _contentBlockersEnabled];
}

- (void)setAllowedAutoplayQuirks:(_WKWebsiteAutoplayQuirk)allowedQuirks
{
    [_webpagePreferences _setAllowedAutoplayQuirks:allowedQuirks];
}

- (_WKWebsiteAutoplayQuirk)allowedAutoplayQuirks
{
    return [_webpagePreferences _allowedAutoplayQuirks];
}

- (void)setAutoplayPolicy:(_WKWebsiteAutoplayPolicy)policy
{
    [_webpagePreferences _setAutoplayPolicy:policy];
}

- (_WKWebsiteAutoplayPolicy)autoplayPolicy
{
    return [_webpagePreferences _autoplayPolicy];
}

- (void)setDeviceOrientationAndMotionAccessPolicy:(_WKWebsiteDeviceOrientationAndMotionAccessPolicy)policy
{
    [_webpagePreferences _setDeviceOrientationAndMotionAccessPolicy:policy];
}

- (_WKWebsiteDeviceOrientationAndMotionAccessPolicy)deviceOrientationAndMotionAccessPolicy
{
    return [_webpagePreferences _deviceOrientationAndMotionAccessPolicy];
}

- (void)setPopUpPolicy:(_WKWebsitePopUpPolicy)policy
{
    [_webpagePreferences _setPopUpPolicy:policy];
}

- (_WKWebsitePopUpPolicy)popUpPolicy
{
    return [_webpagePreferences _popUpPolicy];
}

- (NSDictionary<NSString *, NSString *> *)customHeaderFields
{
    auto& fields = static_cast<API::WebsitePolicies&>([_webpagePreferences _apiObject]).legacyCustomHeaderFields();
    auto dictionary = [NSMutableDictionary dictionaryWithCapacity:fields.size()];
    for (const auto& field : fields)
        [dictionary setObject:field.value() forKey:field.name()];
    return dictionary;
}

- (void)setCustomHeaderFields:(NSDictionary<NSString *, NSString *> *)fields
{
    auto websitePolicies = static_cast<API::WebsitePolicies&>([_webpagePreferences _apiObject]);
    Vector<WebCore::HTTPHeaderField> parsedFields;
    parsedFields.reserveInitialCapacity(fields.count);
    for (NSString *name in fields) {
        auto field = WebCore::HTTPHeaderField::create(name, [fields objectForKey:name]);
        if (field && startsWithLettersIgnoringASCIICase(field->name(), "x-"))
            parsedFields.uncheckedAppend(WTFMove(*field));
    }
    websitePolicies.setLegacyCustomHeaderFields(WTFMove(parsedFields));
}

- (WKWebsiteDataStore *)websiteDataStore
{
    return [_webpagePreferences _websiteDataStore];
}

- (void)setWebsiteDataStore:(WKWebsiteDataStore *)websiteDataStore
{
    [_webpagePreferences _setWebsiteDataStore:websiteDataStore];
}

- (void)setCustomUserAgent:(NSString *)customUserAgent
{
    [_webpagePreferences _setCustomUserAgent:customUserAgent];
}

- (NSString *)customUserAgent
{
    return [_webpagePreferences _customUserAgent];
}

- (void)setCustomJavaScriptUserAgentAsSiteSpecificQuirks:(NSString *)customUserAgent
{
    [_webpagePreferences _setCustomJavaScriptUserAgentAsSiteSpecificQuirks:customUserAgent];
}

- (NSString *)customJavaScriptUserAgentAsSiteSpecificQuirks
{
    return [_webpagePreferences _customJavaScriptUserAgentAsSiteSpecificQuirks];
}

- (void)setCustomNavigatorPlatform:(NSString *)customNavigatorPlatform
{
    [_webpagePreferences _setCustomNavigatorPlatform:customNavigatorPlatform];
}

- (NSString *)customNavigatorPlatform
{
    return [_webpagePreferences _customNavigatorPlatform];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %@>", self.class, [_webpagePreferences description]];
}

- (API::Object&)_apiObject
{
    return [_webpagePreferences _apiObject];
}

@end
