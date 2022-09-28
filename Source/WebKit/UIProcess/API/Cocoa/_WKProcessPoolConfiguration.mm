/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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
#import "_WKProcessPoolConfigurationInternal.h"

#import "LegacyGlobalSettings.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation _WKProcessPoolConfiguration

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    API::Object::constructInWrapper<API::ProcessPoolConfiguration>(self);

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKProcessPoolConfiguration.class, self))
        return;

    _processPoolConfiguration->~ProcessPoolConfiguration();

    [super dealloc];
}

- (NSURL *)injectedBundleURL
{
    return [NSURL fileURLWithPath:_processPoolConfiguration->injectedBundlePath()];
}

- (void)setInjectedBundleURL:(NSURL *)injectedBundleURL
{
    if (injectedBundleURL && !injectedBundleURL.isFileURL)
        [NSException raise:NSInvalidArgumentException format:@"Injected Bundle URL must be a file URL"];

    _processPoolConfiguration->setInjectedBundlePath(injectedBundleURL.path);
}

- (NSSet<Class> *)customClassesForParameterCoder
{
    auto classes = _processPoolConfiguration->customClassesForParameterCoder();
    if (classes.isEmpty())
        return [NSSet set];

    auto result = adoptNS([[NSMutableSet alloc] initWithCapacity:classes.size()]);
    for (const auto& value : classes)
        [result addObject: objc_lookUpClass(value.utf8().data())];

    return result.autorelease();
}

- (void)setCustomClassesForParameterCoder:(NSSet<Class> *)classesForCoder
{
    Vector<WTF::String> classes;
    classes.reserveInitialCapacity(classesForCoder.count);
    for (id classObj : classesForCoder) {
        if (auto* string = NSStringFromClass(classObj))
            classes.uncheckedAppend(string);
    }

    _processPoolConfiguration->setCustomClassesForParameterCoder(WTFMove(classes));
}

- (NSUInteger)maximumProcessCount
{
    // Deprecated.
    return NSUIntegerMax;
}

- (void)setMaximumProcessCount:(NSUInteger)maximumProcessCount
{
    // Deprecated.
}

- (NSInteger)diskCacheSizeOverride
{
    return 0;
}

- (void)setDiskCacheSizeOverride:(NSInteger)size
{
}

- (BOOL)diskCacheSpeculativeValidationEnabled
{
    return NO;
}

- (void)setDiskCacheSpeculativeValidationEnabled:(BOOL)enabled
{
}

- (BOOL)ignoreSynchronousMessagingTimeoutsForTesting
{
    return _processPoolConfiguration->ignoreSynchronousMessagingTimeoutsForTesting();
}

- (void)setIgnoreSynchronousMessagingTimeoutsForTesting:(BOOL)ignoreSynchronousMessagingTimeoutsForTesting
{
    _processPoolConfiguration->setIgnoreSynchronousMessagingTimeoutsForTesting(ignoreSynchronousMessagingTimeoutsForTesting);
}

- (BOOL)attrStyleEnabled
{
    return _processPoolConfiguration->attrStyleEnabled();
}

- (void)setAttrStyleEnabled:(BOOL)enabled
{
    return _processPoolConfiguration->setAttrStyleEnabled(enabled);
}

- (BOOL)shouldThrowExceptionForGlobalConstantRedeclaration
{
    return _processPoolConfiguration->shouldThrowExceptionForGlobalConstantRedeclaration();
}

- (void)setShouldThrowExceptionForGlobalConstantRedeclaration:(BOOL)shouldThrow
{
    return _processPoolConfiguration->setShouldThrowExceptionForGlobalConstantRedeclaration(shouldThrow);
}

- (NSArray<NSURL *> *)additionalReadAccessAllowedURLs
{
    auto paths = _processPoolConfiguration->additionalReadAccessAllowedPaths();
    if (paths.isEmpty())
        return @[ ];

    return createNSArray(paths, [] (auto& path) {
        return [NSURL fileURLWithFileSystemRepresentation:path.utf8().data() isDirectory:NO relativeToURL:nil];
    }).autorelease();
}

- (void)setAdditionalReadAccessAllowedURLs:(NSArray<NSURL *> *)additionalReadAccessAllowedURLs
{
    Vector<String> paths;
    paths.reserveInitialCapacity(additionalReadAccessAllowedURLs.count);
    for (NSURL *url in additionalReadAccessAllowedURLs) {
        if (!url.isFileURL)
            [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", url];

        paths.uncheckedAppend(String::fromUTF8(url.fileSystemRepresentation));
    }

    _processPoolConfiguration->setAdditionalReadAccessAllowedPaths(WTFMove(paths));
}

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
- (NSUInteger)wirelessContextIdentifier
{
    return 0;
}

- (void)setWirelessContextIdentifier:(NSUInteger)identifier
{
}
#endif

- (NSArray *)cachePartitionedURLSchemes
{
    return createNSArray(_processPoolConfiguration->cachePartitionedURLSchemes()).autorelease();
}

- (void)setCachePartitionedURLSchemes:(NSArray *)cachePartitionedURLSchemes
{
    _processPoolConfiguration->setCachePartitionedURLSchemes(makeVector<String>(cachePartitionedURLSchemes));
}

- (NSArray *)alwaysRevalidatedURLSchemes
{
    return createNSArray(_processPoolConfiguration->alwaysRevalidatedURLSchemes()).autorelease();
}

- (void)setAlwaysRevalidatedURLSchemes:(NSArray *)alwaysRevalidatedURLSchemes
{
    _processPoolConfiguration->setAlwaysRevalidatedURLSchemes(makeVector<String>(alwaysRevalidatedURLSchemes));
}

- (NSString *)sourceApplicationBundleIdentifier
{
    return nil;
}

- (void)setSourceApplicationBundleIdentifier:(NSString *)sourceApplicationBundleIdentifier
{
}

- (NSString *)sourceApplicationSecondaryIdentifier
{
    return nil;
}

- (void)setSourceApplicationSecondaryIdentifier:(NSString *)sourceApplicationSecondaryIdentifier
{
}

- (void)setPresentingApplicationPID:(pid_t)presentingApplicationPID
{
    _processPoolConfiguration->setPresentingApplicationPID(presentingApplicationPID);
}

- (pid_t)presentingApplicationPID
{
    return _processPoolConfiguration->presentingApplicationPID();
}

- (void)setPresentingApplicationProcessToken:(audit_token_t)token
{
    _processPoolConfiguration->setPresentingApplicationProcessToken(token);
}

- (audit_token_t)presentingApplicationProcessToken
{
    if (_processPoolConfiguration->presentingApplicationProcessToken())
        return *_processPoolConfiguration->presentingApplicationProcessToken();
    return { };
}

- (void)setProcessSwapsOnNavigation:(BOOL)swaps
{
    _processPoolConfiguration->setProcessSwapsOnNavigation(swaps);
}

- (BOOL)processSwapsOnNavigation
{
    return _processPoolConfiguration->processSwapsOnNavigation();
}

- (void)setPrewarmsProcessesAutomatically:(BOOL)prewarms
{
    _processPoolConfiguration->setIsAutomaticProcessWarmingEnabled(prewarms);
}

- (BOOL)prewarmsProcessesAutomatically
{
    return _processPoolConfiguration->isAutomaticProcessWarmingEnabled();
}

- (void)setUsesWebProcessCache:(BOOL)value
{
    _processPoolConfiguration->setUsesWebProcessCache(value);
}

- (BOOL)usesWebProcessCache
{
    return _processPoolConfiguration->usesWebProcessCache();
}

- (void)setAlwaysKeepAndReuseSwappedProcesses:(BOOL)swaps
{
    _processPoolConfiguration->setAlwaysKeepAndReuseSwappedProcesses(swaps);
}

- (BOOL)alwaysKeepAndReuseSwappedProcesses
{
    return _processPoolConfiguration->alwaysKeepAndReuseSwappedProcesses();
}

- (void)setProcessSwapsOnWindowOpenWithOpener:(BOOL)swaps
{
    _processPoolConfiguration->setProcessSwapsOnWindowOpenWithOpener(swaps);
}

- (BOOL)processSwapsOnWindowOpenWithOpener
{
    return _processPoolConfiguration->processSwapsOnWindowOpenWithOpener();
}

- (void)setProcessSwapsOnNavigationWithinSameNonHTTPFamilyProtocol:(BOOL)swaps
{
    _processPoolConfiguration->setProcessSwapsOnNavigationWithinSameNonHTTPFamilyProtocol(swaps);
}

- (BOOL)processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol
{
    return _processPoolConfiguration->processSwapsOnNavigationWithinSameNonHTTPFamilyProtocol();
}

- (BOOL)pageCacheEnabled
{
    return _processPoolConfiguration->usesBackForwardCache();
}

- (void)setPageCacheEnabled:(BOOL)enabled
{
    return _processPoolConfiguration->setUsesBackForwardCache(enabled);
}

- (BOOL)usesSingleWebProcess
{
    return _processPoolConfiguration->usesSingleWebProcess();
}

- (void)setUsesSingleWebProcess:(BOOL)enabled
{
    _processPoolConfiguration->setUsesSingleWebProcess(enabled);
}

- (BOOL)isJITEnabled
{
    return _processPoolConfiguration->isJITEnabled();
}

- (void)setJITEnabled:(BOOL)enabled
{
    _processPoolConfiguration->setJITEnabled(enabled);
}

#if PLATFORM(IOS_FAMILY)
- (BOOL)alwaysRunsAtBackgroundPriority
{
    return _processPoolConfiguration->alwaysRunsAtBackgroundPriority();
}

- (void)setAlwaysRunsAtBackgroundPriority:(BOOL)alwaysRunsAtBackgroundPriority
{
    _processPoolConfiguration->setAlwaysRunsAtBackgroundPriority(alwaysRunsAtBackgroundPriority);
}

- (BOOL)shouldTakeUIBackgroundAssertion
{
    return _processPoolConfiguration->shouldTakeUIBackgroundAssertion();
}

- (void)setShouldTakeUIBackgroundAssertion:(BOOL)shouldTakeUIBackgroundAssertion
{
    return _processPoolConfiguration->setShouldTakeUIBackgroundAssertion(shouldTakeUIBackgroundAssertion);
}
#endif

- (NSString *)description
{
    NSString *description = [NSString stringWithFormat:@"<%@: %p", NSStringFromClass(self.class), self];

    if (!_processPoolConfiguration->injectedBundlePath().isEmpty())
        return [description stringByAppendingFormat:@"; injectedBundleURL: \"%@\">", [self injectedBundleURL]];

    return [description stringByAppendingString:@">"];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [wrapper(_processPoolConfiguration->copy()) retain];
}

- (NSString *)customWebContentServiceBundleIdentifier
{
    return _processPoolConfiguration->customWebContentServiceBundleIdentifier();
}

- (void)setCustomWebContentServiceBundleIdentifier:(NSString *)customWebContentServiceBundleIdentifier
{
    _processPoolConfiguration->setCustomWebContentServiceBundleIdentifier(customWebContentServiceBundleIdentifier);
}

- (BOOL)configureJSCForTesting
{
    return _processPoolConfiguration->shouldConfigureJSCForTesting();
}

- (void)setConfigureJSCForTesting:(BOOL)value
{
    _processPoolConfiguration->setShouldConfigureJSCForTesting(value);
}

- (NSString *)timeZoneOverride
{
    return _processPoolConfiguration->timeZoneOverride();
}

- (void)setTimeZoneOverride:(NSString *)timeZone
{
    _processPoolConfiguration->setTimeZoneOverride(timeZone);
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_processPoolConfiguration;
}

@end
