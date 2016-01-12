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
    return _processPoolConfiguration->maximumProcessCount();
}

- (void)setMaximumProcessCount:(NSUInteger)maximumProcessCount
{
    _processPoolConfiguration->setMaximumProcessCount(maximumProcessCount);
}

- (NSInteger)diskCacheSizeOverride
{
    return _processPoolConfiguration->diskCacheSizeOverride();
}

- (void)setDiskCacheSizeOverride:(NSInteger)size
{
    _processPoolConfiguration->setDiskCacheSizeOverride(size);
}

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

- (NSString *)description
{
    NSString *description = [NSString stringWithFormat:@"<%@: %p; maximumProcessCount = %lu", NSStringFromClass(self.class), self, static_cast<unsigned long>([self maximumProcessCount])];

    if (!_processPoolConfiguration->injectedBundlePath().isEmpty())
        return [description stringByAppendingFormat:@"; injectedBundleURL: \"%@\">", [self injectedBundleURL]];

    return [description stringByAppendingString:@">"];
}

- (id)copyWithZone:(NSZone *)zone
{
    return wrapper(_processPoolConfiguration->copy().leakRef());
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_processPoolConfiguration;
}

@end

#endif
