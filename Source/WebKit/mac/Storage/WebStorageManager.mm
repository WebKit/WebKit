/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if ENABLE(DOM_STORAGE)

#import "WebSecurityOriginInternal.h"
#import "WebStorageManagerPrivate.h"
#import "WebStorageManagerInternal.h"
#import "WebStorageTrackerClient.h"

#import <WebCore/SecurityOrigin.h>
#import <WebCore/StorageTracker.h>

using namespace WebCore;

NSString * const WebStorageDirectoryDefaultsKey = @"WebKitLocalStorageDatabasePathPreferenceKey";
NSString * const WebStorageDidModifyOriginNotification = @"WebStorageDidModifyOriginNotification";

static NSString *storageDirectoryPath();

@implementation WebStorageManager

+ (WebStorageManager *)sharedWebStorageManager
{
    static WebStorageManager *sharedManager = [[WebStorageManager alloc] init];
    return sharedManager;
}

- (NSArray *)origins
{
    Vector<RefPtr<SecurityOrigin> > coreOrigins;

    StorageTracker::tracker().origins(coreOrigins);

    NSMutableArray *webOrigins = [[NSMutableArray alloc] initWithCapacity:coreOrigins.size()];

    for (size_t i = 0; i < coreOrigins.size(); ++i) {
        WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:coreOrigins[i].get()];
        [webOrigins addObject:webOrigin];
        [webOrigin release];
    }

    return [webOrigins autorelease];
}

- (void)deleteAllOrigins
{
    StorageTracker::tracker().deleteAllOrigins();
}

- (void)deleteOrigin:(WebSecurityOrigin *)origin
{
    StorageTracker::tracker().deleteOrigin([origin _core]);
}

- (void)syncLocalStorage
{
    StorageTracker::tracker().syncLocalStorage();
}

- (void)syncFileSystemAndTrackerDatabase
{
    StorageTracker::tracker().syncFileSystemAndTrackerDatabase();
}

static NSString *storageDirectoryPath()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *storageDirectory = [defaults objectForKey:WebStorageDirectoryDefaultsKey];
    if (!storageDirectory || ![storageDirectory isKindOfClass:[NSString class]])
        storageDirectory = @"~/Library/WebKit/LocalStorage";
    
    return [storageDirectory stringByStandardizingPath];
}

void WebKitInitializeStorageIfNecessary()
{
    static BOOL initialized = NO;
    if (initialized)
        return;
    
    StorageTracker::initializeTracker(storageDirectoryPath());
    
    StorageTracker::tracker().setClient(WebStorageTrackerClient::sharedWebStorageTrackerClient());
    
    initialized = YES;
}

@end

#endif
