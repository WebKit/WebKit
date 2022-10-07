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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "_WKWebExtensionInternal.h"

#import "CocoaImage.h"
#import "WebExtension.h"
#import "_WKWebExtensionMatchPatternInternal.h"
#import <WebCore/WebCoreObjCExtras.h>

NSErrorDomain const _WKWebExtensionErrorDomain = @"_WKWebExtensionErrorDomain";
NSNotificationName const _WKWebExtensionErrorsWereUpdatedNotification = @"_WKWebExtensionErrorsWereUpdated";

@implementation _WKWebExtension

#if ENABLE(WK_WEB_EXTENSIONS)

- (instancetype)initWithAppExtensionBundle:(NSBundle *)appExtensionBundle
{
    NSParameterAssert(appExtensionBundle);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtension>(self, appExtensionBundle);

    return self;
}

- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL
{
    NSParameterAssert(resourceBaseURL);
    NSParameterAssert([resourceBaseURL isFileURL]);
    NSParameterAssert([resourceBaseURL hasDirectoryPath]);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtension>(self, resourceBaseURL);

    return self;
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest
{
    return [self _initWithManifestDictionary:manifest resources:nil];
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest resources:(NSDictionary<NSString *, id> *)resources
{
    NSParameterAssert(manifest);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtension>(self, manifest, resources);

    return self;
}

- (instancetype)_initWithResources:(NSDictionary<NSString *, id> *)resources
{
    NSParameterAssert(resources);

    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<WebKit::WebExtension>(self, resources);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebExtension.class, self))
        return;

    _webExtension->~WebExtension();
}

- (NSDictionary<NSString *, id> *)manifest
{
    return _webExtension->manifest();
}

- (double)manifestVersion
{
    return _webExtension->manifestVersion();
}

- (BOOL)usesManifestVersion:(double)version
{
    return _webExtension->usesManifestVersion(version);
}

- (NSString *)displayName
{
    return _webExtension->displayName();
}

- (NSString *)displayShortName
{
    return _webExtension->displayShortName();
}

- (NSString *)displayVersion
{
    return _webExtension->displayVersion();
}

- (NSString *)displayDescription
{
    return _webExtension->displayDescription();
}

- (NSString *)displayActionLabel
{
    return _webExtension->displayActionLabel();
}

- (NSString *)version
{
    return _webExtension->version();
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return _webExtension->icon(size);
}

- (CocoaImage *)actionIconForSize:(CGSize)size
{
    return _webExtension->actionIcon(size);
}

- (NSSet<_WKWebExtensionPermission> *)requestedPermissions
{
    return _webExtension->requestedPermissions();
}

- (NSSet<_WKWebExtensionPermission> *)optionalPermissions
{
    return _webExtension->optionalPermissions();
}

static inline NSSet<_WKWebExtensionMatchPattern *> *toAPI(const HashSet<Ref<WebKit::WebExtensionMatchPattern>>& patterns)
{
    NSMutableSet<_WKWebExtensionMatchPattern *> *result = [NSMutableSet setWithCapacity:patterns.size()];

    for (auto& matchPattern : patterns)
        [result addObject:WebKit::wrapper(matchPattern)];

    return [result copy];
}

- (NSSet<_WKWebExtensionMatchPattern *> *)requestedPermissionOrigins
{
    return toAPI(_webExtension->requestedPermissionOrigins());
}

- (NSSet<_WKWebExtensionMatchPattern *> *)optionalPermissionOrigins
{
    return toAPI(_webExtension->optionalPermissionOrigins());
}

- (NSSet<_WKWebExtensionMatchPattern *> *)allRequestedOrigins
{
    return toAPI(_webExtension->allRequestedOrigins());
}

- (NSArray<NSError *> *)errors
{
    return _webExtension->errors();
}

- (BOOL)hasBackgroundContent
{
    return _webExtension->hasBackgroundContent();
}

- (BOOL)backgroundContentIsPersistent
{
    return _webExtension->backgroundContentIsPersistent();
}

- (BOOL)hasInjectedContentForURL:(NSURL *)url
{
    NSParameterAssert(url);

    return _webExtension->hasInjectedContentForURL(url);
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtension;
}

- (WebKit::WebExtension&)_webExtension
{
    return *_webExtension;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

- (instancetype)initWithAppExtensionBundle:(NSBundle *)bundle
{
    return nil;
}

- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL
{
    return nil;
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest
{
    return [self _initWithManifestDictionary:manifest resources:nil];
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest resources:(NSDictionary<NSString *, id> *)resources
{
    return nil;
}

- (instancetype)_initWithResources:(NSDictionary<NSString *, id> *)resources
{
    return nil;
}

- (NSDictionary<NSString *, id> *)manifest
{
    return nil;
}

- (double)manifestVersion
{
    return 0;
}

- (BOOL)usesManifestVersion:(double)version
{
    return NO;
}

- (NSString *)displayName
{
    return nil;
}

- (NSString *)displayShortName
{
    return nil;
}

- (NSString *)displayVersion
{
    return nil;
}

- (NSString *)displayDescription
{
    return nil;
}

- (NSString *)displayActionLabel
{
    return nil;
}

- (NSString *)version
{
    return nil;
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return nil;
}

- (CocoaImage *)actionIconForSize:(CGSize)size
{
    return nil;
}

- (NSSet<_WKWebExtensionPermission> *)requestedPermissions
{
    return nil;
}

- (NSSet<_WKWebExtensionPermission> *)optionalPermissions
{
    return nil;
}

- (NSSet<_WKWebExtensionMatchPattern *> *)requestedPermissionOrigins
{
    return nil;
}

- (NSSet<_WKWebExtensionMatchPattern *> *)optionalPermissionOrigins
{
    return nil;
}

- (NSSet<_WKWebExtensionMatchPattern *> *)allRequestedOrigins
{
    return nil;
}

- (NSArray<NSError *> *)errors
{
    return nil;
}

- (BOOL)hasBackgroundContent
{
    return NO;
}

- (BOOL)backgroundContentIsPersistent
{
    return NO;
}

- (BOOL)hasInjectedContentForURL:(NSURL *)url
{
    return NO;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
