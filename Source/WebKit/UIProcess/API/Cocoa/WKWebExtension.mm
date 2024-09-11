/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#import "WKWebExtensionInternal.h"

#import "APIData.h"
#import "CocoaHelpers.h"
#import "CocoaImage.h"
#import "WKNSData.h"
#import "WKNSDictionary.h"
#import "WKNSError.h"
#import "WKWebExtensionMatchPatternInternal.h"
#import "WebExtension.h"
#import <wtf/URL.h>
#import <wtf/cocoa/VectorCocoa.h>

NSErrorDomain const WKWebExtensionErrorDomain = @"WKWebExtensionErrorDomain";

@implementation WKWebExtension

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtension, WebExtension, _webExtension);

+ (void)extensionWithAppExtensionBundle:(NSBundle *)appExtensionBundle completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    NSParameterAssert([appExtensionBundle isKindOfClass:NSBundle.class]);

    // FIXME: <https://webkit.org/b/276194> Make the WebExtension class load data on a background thread.
    // Use an async dispatch in the meantime to prevent clients from expecting synchronous results.

    dispatch_async(dispatch_get_main_queue(), ^{
        Ref result = WebKit::WebExtension::create(appExtensionBundle);

        if (!result->manifestParsedSuccessfully()) {
            completionHandler(nil, wrapper(result->errors().last()));
            return;
        }

        completionHandler(result->wrapper(), nil);
    });
}

+ (void)extensionWithResourceBaseURL:(NSURL *)resourceBaseURL completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    NSParameterAssert([resourceBaseURL isKindOfClass:NSURL.class]);
    NSParameterAssert([resourceBaseURL isFileURL]);
    NSParameterAssert([resourceBaseURL hasDirectoryPath]);

    // FIXME: <https://webkit.org/b/276194> Make the WebExtension class load data on a background thread.
    // Use an async dispatch in the meantime to prevent clients from expecting synchronous results.

    dispatch_async(dispatch_get_main_queue(), ^{
        RefPtr result = WebKit::WebExtension::createWithURL(resourceBaseURL);

        if (!result || !result->manifestParsedSuccessfully()) {
            completionHandler(nil, wrapper(result->errors().last()));
            return;
        }

        completionHandler(result->wrapper(), nil);
    });
}

// FIXME: Remove after Safari has adopted new methods.
- (instancetype)initWithAppExtensionBundle:(NSBundle *)appExtensionBundle error:(NSError **)error
{
    return [self _initWithAppExtensionBundle:appExtensionBundle error:error];
}

- (instancetype)_initWithAppExtensionBundle:(NSBundle *)appExtensionBundle error:(NSError **)error
{
    NSParameterAssert([appExtensionBundle isKindOfClass:NSBundle.class]);

    if (error)
        *error = nil;

    if (!(self = [super init]))
        return nil;

    Ref result = WebKit::WebExtension::create(appExtensionBundle);

    if (!result->manifestParsedSuccessfully()) {
        if (error)
            *error = wrapper(result->errors().last());
        return nil;
    }

    return result->wrapper();
}

// FIXME: Remove after Safari has adopted new methods.
- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    return [self _initWithResourceBaseURL:resourceBaseURL error:error];
}

- (instancetype)_initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    NSParameterAssert([resourceBaseURL isKindOfClass:NSURL.class]);
    NSParameterAssert([resourceBaseURL isFileURL]);
    NSParameterAssert([resourceBaseURL hasDirectoryPath]);

    if (error)
        *error = nil;

    if (!(self = [super init]))
        return nil;

    RefPtr result = WebKit::WebExtension::createWithURL(resourceBaseURL);

    if (!result)
        return nil;

    if (!result->manifestParsedSuccessfully()) {
        if (error)
            *error = wrapper(result->errors().last());
        return nil;
    }

    return result->wrapper();
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest
{
    NSParameterAssert([manifest isKindOfClass:NSDictionary.class]);

    return [self _initWithManifestDictionary:manifest resources:nil];
}

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest resources:(NSDictionary<NSString *, id> *)resources
{
    NSParameterAssert([manifest isKindOfClass:NSDictionary.class]);

    if (!(self = [super init]))
        return nil;

    auto jsonManifest = String(WebKit::encodeJSONString(manifest));
    auto manifestData = JSON::Value::parseJSON(jsonManifest);
    auto resourcesData = WebKit::toDataMap(resources);
    API::Object::constructInWrapper<WebKit::WebExtension>(self, *manifestData, WTFMove(resourcesData));

    return self;
}

- (instancetype)_initWithResources:(NSDictionary<NSString *, id> *)resources
{
    NSParameterAssert([resources isKindOfClass:NSDictionary.class]);

    if (!(self = [super init]))
        return nil;

    auto resourcesData = WebKit::toDataMap(resources);
    API::Object::constructInWrapper<WebKit::WebExtension>(self, WTFMove(resourcesData));

    return self;
}

- (NSDictionary<NSString *, id> *)manifest
{
    return parseJSON(self._protectedExtensin->serializeManifest());
}

- (double)manifestVersion
{
    return self._protectedWebExtension->manifestVersion();
}

- (BOOL)supportsManifestVersion:(double)version
{
    return self._protectedWebExtension->supportsManifestVersion(version);
}

- (NSLocale *)defaultLocale
{
    return self._protectedWebExtension->defaultLocale();
}

- (NSString *)displayName
{
    return self._protectedWebExtension->displayName();
}

- (NSString *)displayShortName
{
    return self._protectedWebExtension->displayShortName();
}

- (NSString *)displayVersion
{
    return self._protectedWebExtension->displayVersion();
}

- (NSString *)displayDescription
{
    return self._protectedWebExtension->displayDescription();
}

- (NSString *)displayActionLabel
{
    return self._protectedWebExtension->displayActionLabel();
}

- (NSString *)version
{
    return self._protectedWebExtension->version();
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return self._protectedWebExtension->icon(size)->image().get();
}

- (CocoaImage *)actionIconForSize:(CGSize)size
{
    return self._protectedWebExtension->actionIcon(size)->image().get();
}

- (NSSet<WKWebExtensionPermission> *)requestedPermissions
{
    return WebKit::toAPI(self._protectedWebExtension->requestedPermissions());
}

- (NSSet<WKWebExtensionPermission> *)optionalPermissions
{
    return WebKit::toAPI(self._protectedWebExtension->optionalPermissions());
}

- (NSSet<WKWebExtensionMatchPattern *> *)requestedPermissionMatchPatterns
{
    return toAPI(self._protectedWebExtension->requestedPermissionMatchPatterns());
}

- (NSSet<WKWebExtensionMatchPattern *> *)optionalPermissionMatchPatterns
{
    return toAPI(self._protectedWebExtension->optionalPermissionMatchPatterns());
}

- (NSSet<WKWebExtensionMatchPattern *> *)allRequestedMatchPatterns
{
    return toAPI(self._protectedWebExtension->allRequestedMatchPatterns());
}

- (NSArray<NSError *> *)errors
{
    return createNSArray(self._protectedWebExtension->errors(), [](auto&& child) -> id {
        return wrapper(child);
    }).get();
}

- (BOOL)hasBackgroundContent
{
    return self._protectedWebExtension->hasBackgroundContent();
}

- (BOOL)hasPersistentBackgroundContent
{
    return self._protectedWebExtension->backgroundContentIsPersistent();
}

- (BOOL)hasInjectedContent
{
    return self._protectedWebExtension->hasStaticInjectedContent();
}

- (BOOL)hasOptionsPage
{
    return self._protectedWebExtension->hasOptionsPage();
}

- (BOOL)hasOverrideNewTabPage
{
    return self._protectedWebExtension->hasOverrideNewTabPage();
}

- (BOOL)hasCommands
{
    return self._protectedWebExtension->hasCommands();
}

- (BOOL)hasContentModificationRules
{
    return self._protectedWebExtension->hasContentModificationRules();
}

- (BOOL)_hasServiceWorkerBackgroundContent
{
    return self._protectedWebExtension->backgroundContentIsServiceWorker();
}

- (BOOL)_hasModularBackgroundContent
{
    return self._protectedWebExtension->backgroundContentUsesModules();
}

#if ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
- (BOOL)_hasSidebar
{
    return _webExtension->hasSidebar();
}
#else // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)
- (BOOL)_hasSidebar
{
    return NO;
}
#endif // ENABLE(WK_WEB_EXTENSIONS_SIDEBAR)

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtension;
}

- (WebKit::WebExtension&)_webExtension
{
    return *_webExtension;
}

- (Ref<WebKit::WebExtension>)_protectedWebExtension
{
    return *_webExtension;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

+ (void)extensionWithAppExtensionBundle:(NSBundle *)appExtensionBundle completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil]);
}

+ (void)extensionWithResourceBaseURL:(NSURL *)resourceBaseURL completionHandler:(void (^)(WKWebExtension *extension, NSError *error))completionHandler
{
    completionHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil]);
}

- (instancetype)initWithAppExtensionBundle:(NSBundle *)bundle error:(NSError **)error
{
    return [self _initWithAppExtensionBundle:bundle error:error];
}

- (instancetype)_initWithAppExtensionBundle:(NSBundle *)bundle error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    return nil;
}

- (instancetype)initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    return [self _initWithResourceBaseURL:resourceBaseURL error:error];
}

- (instancetype)_initWithResourceBaseURL:(NSURL *)resourceBaseURL error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
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

- (BOOL)supportsManifestVersion:(double)version
{
    return NO;
}

- (NSLocale *)defaultLocale
{
    return nil;
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

- (NSSet<WKWebExtensionPermission> *)requestedPermissions
{
    return nil;
}

- (NSSet<WKWebExtensionPermission> *)optionalPermissions
{
    return nil;
}

- (NSSet<WKWebExtensionMatchPattern *> *)requestedPermissionMatchPatterns
{
    return nil;
}

- (NSSet<WKWebExtensionMatchPattern *> *)optionalPermissionMatchPatterns
{
    return nil;
}

- (NSSet<WKWebExtensionMatchPattern *> *)allRequestedMatchPatterns
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

- (BOOL)hasPersistentBackgroundContent
{
    return NO;
}

- (BOOL)hasInjectedContent
{
    return NO;
}

- (BOOL)hasOptionsPage
{
    return NO;
}

- (BOOL)hasOverrideNewTabPage
{
    return NO;
}

- (BOOL)hasCommands
{
    return NO;
}

- (BOOL)hasContentModificationRules
{
    return NO;
}

- (BOOL)_hasServiceWorkerBackgroundContent
{
    return NO;
}

- (BOOL)_hasModularBackgroundContent
{
    return NO;
}

- (BOOL)_hasSidebar
{
    return NO;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
