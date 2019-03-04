/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "_WKWebsiteDataStoreConfigurationInternal.h"

#import <wtf/RetainPtr.h>

static void checkURLArgument(NSURL *url)
{
    if (url && ![url isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", url];
}

@implementation _WKWebsiteDataStoreConfiguration

- (NSURL *)_webStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->webStorageDirectory() isDirectory:YES];
}

- (void)_setWebStorageDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _configuration->setWebStorageDirectory(url.path);
}

- (NSURL *)_indexedDBDatabaseDirectory
{
    return [NSURL fileURLWithPath:_configuration->indexedDBDatabaseDirectory() isDirectory:YES];
}

- (void)_setIndexedDBDatabaseDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _configuration->setIndexedDBDatabaseDirectory(url.path);
}

- (NSURL *)_webSQLDatabaseDirectory
{
    return [NSURL fileURLWithPath:_configuration->webSQLDatabaseDirectory() isDirectory:YES];
}

- (void)_setWebSQLDatabaseDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _configuration->setWebSQLDatabaseDirectory(url.path);
}

- (NSURL *)httpProxy
{
    return _configuration->httpProxy();
}

- (void)setHTTPProxy:(NSURL *)proxy
{
    _configuration->setHTTPProxy(proxy);
}

- (NSURL *)httpsProxy
{
    return _configuration->httpsProxy();
}

- (void)setHTTPSProxy:(NSURL *)proxy
{
    _configuration->setHTTPSProxy(proxy);
}

- (NSURL *)_cookieStorageFile
{
    return [NSURL fileURLWithPath:_configuration->cookieStorageFile() isDirectory:NO];
}

- (void)_setCookieStorageFile:(NSURL *)url
{
    checkURLArgument(url);
    if ([url hasDirectoryPath])
        [NSException raise:NSInvalidArgumentException format:@"The cookie storage path must point to a file, not a directory."];

    _configuration->setCookieStorageFile(url.path);
}

- (NSURL *)_resourceLoadStatisticsDirectory
{
    return [NSURL fileURLWithPath:_configuration->resourceLoadStatisticsDirectory() isDirectory:YES];
}

- (void)_setResourceLoadStatisticsDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _configuration->setResourceLoadStatisticsDirectory(url.path);
}

- (NSURL *)_cacheStorageDirectory
{
    return [NSURL fileURLWithPath:_configuration->cacheStorageDirectory() isDirectory:YES];
}

- (void)_setCacheStorageDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _configuration->setCacheStorageDirectory(url.path);
}

- (NSURL *)_serviceWorkerRegistrationDirectory
{
    return [NSURL fileURLWithPath:_configuration->serviceWorkerRegistrationDirectory() isDirectory:YES];
}

- (void)_setServiceWorkerRegistrationDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _configuration->setServiceWorkerRegistrationDirectory(url.path);
}

- (void)setSourceApplicationBundleIdentifier:(NSString *)identifier
{
    _configuration->setSourceApplicationBundleIdentifier(identifier);
}

- (NSString *)sourceApplicationBundleIdentifier
{
    return _configuration->sourceApplicationBundleIdentifier();
}

- (NSString *)sourceApplicationSecondaryIdentifier
{
    return _configuration->sourceApplicationSecondaryIdentifier();
}

- (void)setSourceApplicationSecondaryIdentifier:(NSString *)identifier
{
    _configuration->setSourceApplicationSecondaryIdentifier(identifier);
}

- (API::Object&)_apiObject
{
    return *_configuration;
}

@end
