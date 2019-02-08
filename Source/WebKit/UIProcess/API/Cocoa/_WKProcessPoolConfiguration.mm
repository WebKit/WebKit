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
#import "_WKProcessPoolConfigurationInternal.h"

#if WK_API_ENABLED

#import <wtf/RetainPtr.h>

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
    return _processPoolConfiguration->diskCacheSpeculativeValidationEnabled();
}

- (void)setDiskCacheSpeculativeValidationEnabled:(BOOL)enabled
{
    _processPoolConfiguration->setDiskCacheSpeculativeValidationEnabled(enabled);
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

- (NSArray<NSURL *> *)additionalReadAccessAllowedURLs
{
    auto paths = _processPoolConfiguration->additionalReadAccessAllowedPaths();
    if (paths.isEmpty())
        return @[ ];

    NSMutableArray *urls = [NSMutableArray arrayWithCapacity:paths.size()];
    for (const auto& path : paths)
        [urls addObject:[NSURL fileURLWithFileSystemRepresentation:path.data() isDirectory:NO relativeToURL:nil]];

    return urls;
}

- (void)setAdditionalReadAccessAllowedURLs:(NSArray<NSURL *> *)additionalReadAccessAllowedURLs
{
    Vector<CString> paths;
    paths.reserveInitialCapacity(additionalReadAccessAllowedURLs.count);
    for (NSURL *url in additionalReadAccessAllowedURLs) {
        if (!url.isFileURL)
            [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", url];

        paths.uncheckedAppend(url.fileSystemRepresentation);
    }

    _processPoolConfiguration->setAdditionalReadAccessAllowedPaths(WTFMove(paths));
}

#if ENABLE(PROXIMITY_NETWORKING)
- (NSUInteger)wirelessContextIdentifier
{
    return _processPoolConfiguration->wirelessContextIdentifier();
}

- (void)setWirelessContextIdentifier:(NSUInteger)identifier
{
    _processPoolConfiguration->setWirelessContextIdentifier(identifier);
}
#endif

- (NSArray *)cachePartitionedURLSchemes
{
    auto schemes = _processPoolConfiguration->cachePartitionedURLSchemes();
    if (schemes.isEmpty())
        return @[];

    NSMutableArray *array = [NSMutableArray arrayWithCapacity:schemes.size()];
    for (const auto& scheme : schemes)
        [array addObject:(NSString *)scheme];

    return array;
}

- (void)setCachePartitionedURLSchemes:(NSArray *)cachePartitionedURLSchemes
{
    Vector<String> schemes;
    for (id urlScheme in cachePartitionedURLSchemes) {
        if ([urlScheme isKindOfClass:[NSString class]])
            schemes.append(String((NSString *)urlScheme));
    }
    
    _processPoolConfiguration->setCachePartitionedURLSchemes(WTFMove(schemes));
}

- (NSArray *)alwaysRevalidatedURLSchemes
{
    auto& schemes = _processPoolConfiguration->alwaysRevalidatedURLSchemes();
    if (schemes.isEmpty())
        return @[];

    NSMutableArray *array = [NSMutableArray arrayWithCapacity:schemes.size()];
    for (auto& scheme : schemes)
        [array addObject:(NSString *)scheme];

    return array;
}

- (void)setAlwaysRevalidatedURLSchemes:(NSArray *)alwaysRevalidatedURLSchemes
{
    Vector<String> schemes;
    schemes.reserveInitialCapacity(alwaysRevalidatedURLSchemes.count);
    for (id scheme in alwaysRevalidatedURLSchemes) {
        if ([scheme isKindOfClass:[NSString class]])
            schemes.append((NSString *)scheme);
    }

    _processPoolConfiguration->setAlwaysRevalidatedURLSchemes(WTFMove(schemes));
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

- (BOOL)shouldCaptureAudioInUIProcess
{
    return _processPoolConfiguration->shouldCaptureAudioInUIProcess();
}

- (void)setShouldCaptureAudioInUIProcess:(BOOL)shouldCaptureAudioInUIProcess
{
    _processPoolConfiguration->setShouldCaptureAudioInUIProcess(shouldCaptureAudioInUIProcess);
}

- (void)setPresentingApplicationPID:(pid_t)presentingApplicationPID
{
    _processPoolConfiguration->setPresentingApplicationPID(presentingApplicationPID);
}

- (pid_t)presentingApplicationPID
{
    return _processPoolConfiguration->presentingApplicationPID();
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

- (BOOL)pageCacheEnabled
{
    return _processPoolConfiguration->cacheModel() != WebKit::CacheModel::DocumentViewer;
}

- (void)setPageCacheEnabled:(BOOL)enabled
{
    if (!enabled)
        _processPoolConfiguration->setCacheModel(WebKit::CacheModel::DocumentViewer);
    else if (![self pageCacheEnabled])
        _processPoolConfiguration->setCacheModel(WebKit::CacheModel::PrimaryWebBrowser);
}

- (BOOL)usesSingleWebProcess
{
    return _processPoolConfiguration->usesSingleWebProcess();
}

- (void)setUsesSingleWebProcess:(BOOL)enabled
{
    _processPoolConfiguration->setUsesSingleWebProcess(enabled);
}

- (BOOL)suppressesConnectionTerminationOnSystemChange
{
    return _processPoolConfiguration->suppressesConnectionTerminationOnSystemChange();
}

- (BOOL)isJITEnabled
{
    return _processPoolConfiguration->isJITEnabled();
}

- (void)setJITEnabled:(BOOL)enabled
{
    _processPoolConfiguration->setJITEnabled(enabled);
}

- (void)setSuppressesConnectionTerminationOnSystemChange:(BOOL)suppressesConnectionTerminationOnSystemChange
{
    _processPoolConfiguration->setSuppressesConnectionTerminationOnSystemChange(suppressesConnectionTerminationOnSystemChange);
}

#if PLATFORM(IOS_FAMILY)
- (NSString *)CTDataConnectionServiceType
{
    return _processPoolConfiguration->ctDataConnectionServiceType();
}

- (void)setCTDataConnectionServiceType:(NSString *)ctDataConnectionServiceType
{
    _processPoolConfiguration->setCTDataConnectionServiceType(ctDataConnectionServiceType);
}

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

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_processPoolConfiguration;
}

@end

#endif
