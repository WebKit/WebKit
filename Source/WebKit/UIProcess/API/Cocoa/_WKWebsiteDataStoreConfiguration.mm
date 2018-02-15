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
#import "_WKWebsiteDataStoreConfiguration.h"

#if WK_API_ENABLED

#import <wtf/RetainPtr.h>

static void checkURLArgument(NSURL *url)
{
    if (url && ![url isFileURL])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not a file URL", url];
}

@implementation _WKWebsiteDataStoreConfiguration {
    RetainPtr<NSURL> _webStorageDirectoryURL;
    RetainPtr<NSURL> _indexedDBDatabaseDirectoryURL;
    RetainPtr<NSURL> _webSQLDatabaseDirectoryURL;
    RetainPtr<NSURL> _cookieStorageFileURL;
    RetainPtr<NSURL> _resourceLoadStatisticsDirectoryURL;
    RetainPtr<NSURL> _cacheStorageDirectoryURL;
    RetainPtr<NSURL> _serviceWorkerRegistrationDirectoryURL;
}

- (NSURL *)_webStorageDirectory
{
    return _webStorageDirectoryURL.get();
}

- (void)_setWebStorageDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _webStorageDirectoryURL = adoptNS([url copy]);
}

- (NSURL *)_indexedDBDatabaseDirectory
{
    return _indexedDBDatabaseDirectoryURL.get();
}

- (void)_setIndexedDBDatabaseDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _indexedDBDatabaseDirectoryURL = adoptNS([url copy]);
}

- (NSURL *)_webSQLDatabaseDirectory
{
    return _webSQLDatabaseDirectoryURL.get();
}

- (void)_setWebSQLDatabaseDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _webSQLDatabaseDirectoryURL = adoptNS([url copy]);
}

- (NSURL *)_cookieStorageFile
{
    return _cookieStorageFileURL.get();
}

- (void)_setCookieStorageFile:(NSURL *)url
{
    checkURLArgument(url);
    if ([url hasDirectoryPath])
        [NSException raise:NSInvalidArgumentException format:@"The cookie storage path must point to a file, not a directory."];

    _cookieStorageFileURL = adoptNS([url copy]);
}

- (NSURL *)_resourceLoadStatisticsDirectory
{
    return _resourceLoadStatisticsDirectoryURL.get();
}

- (void)_setResourceLoadStatisticsDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _resourceLoadStatisticsDirectoryURL = adoptNS([url copy]);
}

- (NSURL *)_cacheStorageDirectory
{
    return _cacheStorageDirectoryURL.get();
}

- (void)_setCacheStorageDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _cacheStorageDirectoryURL = adoptNS([url copy]);
}

- (NSURL *)_serviceWorkerRegistrationDirectory
{
    return _serviceWorkerRegistrationDirectoryURL.get();
}

- (void)_setServiceWorkerRegistrationDirectory:(NSURL *)url
{
    checkURLArgument(url);
    _serviceWorkerRegistrationDirectoryURL = adoptNS([url copy]);
}

@end

#endif
