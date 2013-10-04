/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "WebDiskImageCacheClientIOS.h"

#if ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)

#import "WebPreferences.h"
#import "WebPreferencesPrivate.h"
#import <WebCore/DiskImageCacheIOS.h>
#import <sys/param.h>

using namespace WebCore;

void WebDiskImageCacheClient::didCreateDiskImageCacheDirectory(const WTF::String& directory)
{
    // Set the NSFileProtectionComplete attribute on the directory so that its contents are inaccessible when the device is locked.
    NSError *error = nil;
    NSString *directoryPath = directory;
    NSDictionary *attributes = @{NSFileProtectionKey: NSFileProtectionComplete};
    if (![[NSFileManager defaultManager] setAttributes:attributes ofItemAtPath:directoryPath error:&error])
        NSLog(@"DiskImageCache: Failed to set attributes on disk image cache directory: %@", error);

    WebPreferences *standardPreferences = [WebPreferences standardPreferences];
    ASSERT(![[standardPreferences _diskImageCacheSavedCacheDirectory] length]);
    [standardPreferences _setDiskImageCacheSavedCacheDirectory:directory];
}

static void removeOldDiskImageCacheDirectory()
{
    WebPreferences *standardPreferences = [WebPreferences standardPreferences];
    NSString *oldDirectory = [standardPreferences _diskImageCacheSavedCacheDirectory];
    [standardPreferences _setDiskImageCacheSavedCacheDirectory:@""];

    if (![oldDirectory length])
        return;

    const char* oldDirectoryCString = [oldDirectory fileSystemRepresentation];
    const size_t length = strlen(oldDirectoryCString) + 1; // For NULL terminator.
    ASSERT(length < MAXPATHLEN);
    if (length >= MAXPATHLEN)
        return;

    // This deletes whatever directory was named in the user defaults key.
    // To be safe, we resolve the absolute path, and verify that the
    // last path component starts with "DiskImageCache".

    // Resolve the old directory path to an absolute path.
    Vector<char, MAXPATHLEN> path(length);
    memcpy(path.data(), oldDirectoryCString, length);
    char absolutePath[MAXPATHLEN];
    if (!realpath(path.data(), absolutePath)) {
        NSLog(@"DiskImageCache: Could not resolve the absolute path of the old directory.");
        return;
    }

    // Verify the last path component starts with "DiskImageCache".
    const size_t absoluteLength = strlen(absolutePath);
    NSString *resolvedAbsolutePath = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:absolutePath length:absoluteLength];
    NSString *prefix = @"DiskImageCache";
    if (![[resolvedAbsolutePath lastPathComponent] hasPrefix:prefix]) {
        NSLog(@"DiskImageCache: The old directory did not start with the proper prefix.");
        return;
    }

    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        NSError *error;
        if (![[NSFileManager defaultManager] removeItemAtPath:oldDirectory error:&error])
            NSLog(@"DiskImageCache: Failed to Remove Old Directory: %@", [error localizedFailureReason]);
    });
}

void WebKitInitializeWebDiskImageCache()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    removeOldDiskImageCacheDirectory();

    RefPtr<WebDiskImageCacheClient> sharedClient = WebDiskImageCacheClient::create();
    diskImageCache().setClient(sharedClient.release());
}

#endif // ENABLE(DISK_IMAGE_CACHE) && PLATFORM(IOS)
