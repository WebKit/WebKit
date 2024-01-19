/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "_WKWebExtensionWebRequestFilter.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WebExtensionTabIdentifier.h"
#import "WebExtensionUtilities.h"
#import "WebExtensionWindowIdentifier.h"
#import "_WKResourceLoadInfo.h"
#import "_WKWebExtensionMatchPattern.h"

using namespace WebKit;

static NSString *urlsKey = @"urls";
static NSString *typesKey = @"types";

static NSString *tabIdKey = @"tabId";
static NSString *windowIdKey = @"windowId";

_WKWebExtensionWebRequestResourceType _WKWebExtensionWebRequestResourceTypeFromWKResourceLoadInfo(_WKResourceLoadInfo *resourceLoadInfo)
{
    switch (resourceLoadInfo.resourceType) {
    case _WKResourceLoadInfoResourceTypeDocument:
        return resourceLoadInfo.parentFrame ? _WKWebExtensionWebRequestResourceTypeMainFrame : _WKWebExtensionWebRequestResourceTypeSubframe;
    case _WKResourceLoadInfoResourceTypeStylesheet:
        return _WKWebExtensionWebRequestResourceTypeStylesheet;
    case _WKResourceLoadInfoResourceTypeScript:
        return _WKWebExtensionWebRequestResourceTypeScript;
    case _WKResourceLoadInfoResourceTypeImage:
        return _WKWebExtensionWebRequestResourceTypeImage;
    case _WKResourceLoadInfoResourceTypeFont:
        return _WKWebExtensionWebRequestResourceTypeFont;
    case _WKResourceLoadInfoResourceTypeObject:
        return _WKWebExtensionWebRequestResourceTypeObject;
    case _WKResourceLoadInfoResourceTypeFetch:
    case _WKResourceLoadInfoResourceTypeXMLHTTPRequest:
        return _WKWebExtensionWebRequestResourceTypeXMLHTTPRequest;
    case _WKResourceLoadInfoResourceTypeCSPReport:
        return _WKWebExtensionWebRequestResourceTypeCSPReport;
    case _WKResourceLoadInfoResourceTypeMedia:
        return _WKWebExtensionWebRequestResourceTypeMedia;
    case _WKResourceLoadInfoResourceTypeApplicationManifest:
        return _WKWebExtensionWebRequestResourceTypeApplicationManifest;
    case _WKResourceLoadInfoResourceTypeXSLT:
        return _WKWebExtensionWebRequestResourceTypeXSLT;
    case _WKResourceLoadInfoResourceTypePing:
        return _WKWebExtensionWebRequestResourceTypePing;
    case _WKResourceLoadInfoResourceTypeBeacon:
        return _WKWebExtensionWebRequestResourceTypeBeacon;
    case _WKResourceLoadInfoResourceTypeOther:
        return _WKWebExtensionWebRequestResourceTypeOther;
    }

    ASSERT_NOT_REACHED();
    return _WKWebExtensionWebRequestResourceTypeOther;
}

@implementation _WKWebExtensionWebRequestFilter {
    std::optional<WebExtensionTabIdentifier> _tabID;
    std::optional<WebExtensionWindowIdentifier> _windowID;
    NSArray<_WKWebExtensionMatchPattern *> *_urlPatterns;
    NSSet<NSNumber *> *_types;
}

- (instancetype)initWithDictionary:(NSDictionary<NSString *, id> *)dictionary outErrorMessage:(NSString **)outErrorMessage
{
    if (!(self = [super init])) {
        *outErrorMessage = @"Runtime failure.";
        return nil;
    }

    static NSArray<NSString *> *requiredKeys = @[
        urlsKey,
    ];

    static NSDictionary<NSString *, id> *expectedTypes = @{
        urlsKey: @[ NSString.class ],
        typesKey: NSArray.class,
        tabIdKey: NSNumber.class,
        windowIdKey: NSNumber.class,
    };

    if (!validateDictionary(dictionary, nil, requiredKeys, expectedTypes, outErrorMessage))
        return nil;

    if ((*outErrorMessage = [self _initializeWithDictionary:dictionary]))
        return nil;

    return self;
}

static NSArray<_WKWebExtensionMatchPattern *> *toMatchPatterns(NSArray<NSString *> *value, NSString **outErrorMessage)
{
    if (!value.count)
        return nil;

    NSError *error;

    NSMutableArray<_WKWebExtensionMatchPattern *> *patterns = [[NSMutableArray alloc] init];
    for (NSString *rawPattern in value) {
        _WKWebExtensionMatchPattern *pattern = [[_WKWebExtensionMatchPattern alloc] initWithString:rawPattern error:&error];
        if (!pattern) {
            if (outErrorMessage)
                *outErrorMessage = toErrorString(nil, urlsKey, @"'%@' is an invalid match pattern. %@", rawPattern, error.userInfo[NSDebugDescriptionErrorKey]);
            return nil;
        }
        [patterns addObject:pattern];
    }

    return patterns;
}

static NSNumber *toResourceType(NSString *typeString, NSString **outErrorMessage)
{
    static NSDictionary<NSString *, NSNumber *> *validTypes = @{
        @"main_frame": @(_WKWebExtensionWebRequestResourceTypeMainFrame),
        @"sub_frame": @(_WKWebExtensionWebRequestResourceTypeSubframe),
        @"stylesheet": @(_WKWebExtensionWebRequestResourceTypeStylesheet),
        @"script": @(_WKWebExtensionWebRequestResourceTypeScript),
        @"image": @(_WKWebExtensionWebRequestResourceTypeImage),
        @"font": @(_WKWebExtensionWebRequestResourceTypeFont),
        @"object": @(_WKWebExtensionWebRequestResourceTypeObject),
        @"xmlhttprequest": @(_WKWebExtensionWebRequestResourceTypeXMLHTTPRequest),
        @"ping": @(_WKWebExtensionWebRequestResourceTypePing),
        @"csp_report": @(_WKWebExtensionWebRequestResourceTypeCSPReport),
        @"media": @(_WKWebExtensionWebRequestResourceTypeMedia),
        @"websocket": @(_WKWebExtensionWebRequestResourceTypeWebsocket),
        @"web_manifest": @(_WKWebExtensionWebRequestResourceTypeApplicationManifest),
        @"xslt": @(_WKWebExtensionWebRequestResourceTypeXSLT),
        @"beacon": @(_WKWebExtensionWebRequestResourceTypeBeacon),
        @"other": @(_WKWebExtensionWebRequestResourceTypeOther),
    };

    NSNumber *typeAsNumber = validTypes[typeString];
    if (!typeAsNumber) {
        *outErrorMessage = toErrorString(nil, typesKey, @"'%@' is an unknown resource type", typeString);
        return nil;
    }

    return typeAsNumber;
}

static NSSet<NSNumber *> *toResourceTypes(NSArray<NSString *> *rawTypes, NSString **outErrorMessage)
{
    if (!rawTypes.count)
        return nil;

    NSMutableSet<NSNumber *> *types = [[NSMutableSet alloc] init];
    for (NSString *rawType in rawTypes) {
        NSNumber *type = toResourceType(rawType, outErrorMessage);
        if (!type)
            return nil;
        [types addObject:type];
    }

    return types;
}

static std::optional<WebExtensionTabIdentifier> toTabID(NSNumber *rawValue)
{
    if (!rawValue)
        return std::nullopt;

    return toWebExtensionTabIdentifier(rawValue.doubleValue);
}

static std::optional<WebExtensionWindowIdentifier> toWindowID(NSNumber *rawValue)
{
    if (!rawValue)
        return std::nullopt;

    return toWebExtensionWindowIdentifier(rawValue.doubleValue);
}

- (NSString *)_initializeWithDictionary:(NSDictionary<NSString *, id> *)dictionary
{
    NSString *errorMessage;
    _urlPatterns = toMatchPatterns(dictionary[urlsKey], &errorMessage);
    if (errorMessage)
        return errorMessage;

    _types = toResourceTypes(dictionary[typesKey], &errorMessage);
    if (errorMessage)
        return errorMessage;

    _tabID = toTabID(dictionary[tabIdKey]);
    _windowID = toWindowID(dictionary[windowIdKey]);

    return nil;
}

- (BOOL)matchesRequestForResourceOfType:(_WKWebExtensionWebRequestResourceType)resourceType URL:(NSURL *)URL tabID:(double)tabID windowID:(double)windowID
{
    if (_types && ![_types containsObject:@(resourceType)])
        return NO;

    if (_urlPatterns) {
        BOOL hasURLMatch = NO;
        for (_WKWebExtensionMatchPattern *pattern in _urlPatterns) {
            if ([pattern matchesURL:URL]) {
                hasURLMatch = YES;
                break;
            }
        }

        if (!hasURLMatch)
            return NO;
    }

    if (isValid(_tabID) && toWebAPI(_tabID.value()) != tabID)
        return NO;

    if (isValid(_windowID) && toWebAPI(_windowID.value()) != windowID)
        return NO;

    return YES;
}

@end

#else

@implementation _WKWebExtensionWebRequestFilter

- (instancetype)initWithDictionary:(NSDictionary<NSString *, id> *)dictionary outErrorMessage:(NSString **)outErrorMessage
{
    return nil;
}

- (BOOL)matchesRequestForResourceOfType:(_WKWebExtensionWebRequestResourceType)resourceType URL:(NSURL *)URL tabID:(double)tabID windowID:(double)windowID
{
    return NO;
}

_WKWebExtensionWebRequestResourceType _WKWebExtensionWebRequestResourceTypeFromWKResourceLoadInfo(_WKResourceLoadInfo *resourceLoadInfo)
{
    return _WKWebExtensionWebRequestResourceTypeOther;
}

@end

#endif // ENABLE(WEB_EXTENSIONS)
