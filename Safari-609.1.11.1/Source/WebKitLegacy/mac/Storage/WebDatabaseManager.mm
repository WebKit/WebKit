/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebDatabaseManagerPrivate.h"

#import "WebDatabaseManagerClient.h"
#import "WebPlatformStrategies.h"
#import "WebSecurityOriginInternal.h"

#import <WebCore/DatabaseManager.h>
#import <WebCore/DatabaseTracker.h>
#import <WebCore/SecurityOrigin.h>

#if ENABLE(INDEXED_DATABASE)
#import "WebDatabaseProvider.h"
#endif

#if PLATFORM(IOS_FAMILY)
#import "WebDatabaseManagerInternal.h"
#import <WebCore/DatabaseTracker.h>
#import <WebCore/WebCoreThread.h>
#endif

using namespace WebCore;

NSString *WebDatabaseDirectoryDefaultsKey = @"WebDatabaseDirectory";

NSString *WebDatabaseDisplayNameKey = @"WebDatabaseDisplayNameKey";
NSString *WebDatabaseExpectedSizeKey = @"WebDatabaseExpectedSizeKey";
NSString *WebDatabaseUsageKey = @"WebDatabaseUsageKey";

NSString *WebDatabaseDidModifyOriginNotification = @"WebDatabaseDidModifyOriginNotification";
NSString *WebDatabaseDidModifyDatabaseNotification = @"WebDatabaseDidModifyDatabaseNotification";
NSString *WebDatabaseIdentifierKey = @"WebDatabaseIdentifierKey";

#if PLATFORM(IOS_FAMILY)
CFStringRef WebDatabaseOriginsDidChangeNotification = CFSTR("WebDatabaseOriginsDidChangeNotification");
#endif

static NSString *databasesDirectoryPath();

@implementation WebDatabaseManager

+ (WebDatabaseManager *) sharedWebDatabaseManager
{
    static WebDatabaseManager *sharedManager = [[WebDatabaseManager alloc] init];
    return sharedManager;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;

    WebPlatformStrategies::initializeIfNecessary();

    DatabaseManager& dbManager = DatabaseManager::singleton();

    // Set the database root path in WebCore
    dbManager.initialize(databasesDirectoryPath());

    // Set the DatabaseManagerClient
    dbManager.setClient(WebDatabaseManagerClient::sharedWebDatabaseManagerClient());

    return self;
}

- (NSArray *)origins
{
    auto coreOrigins = DatabaseTracker::singleton().origins();
    NSMutableArray *webOrigins = [[NSMutableArray alloc] initWithCapacity:coreOrigins.size()];
    for (auto& coreOrigin : coreOrigins) {
        WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:coreOrigin.securityOrigin().ptr()];
        [webOrigins addObject:webOrigin];
        [webOrigin release];
    }
    return [webOrigins autorelease];
}

- (NSArray *)databasesWithOrigin:(WebSecurityOrigin *)origin
{
    if (!origin)
        return nil;
    Vector<String> nameVector = DatabaseTracker::singleton().databaseNames([origin _core]->data());
    NSMutableArray *names = [[NSMutableArray alloc] initWithCapacity:nameVector.size()];
    for (auto& name : nameVector)
        [names addObject:(NSString *)name];
    return [names autorelease];
}

- (NSDictionary *)detailsForDatabase:(NSString *)databaseIdentifier withOrigin:(WebSecurityOrigin *)origin
{
    if (!origin)
        return nil;

    auto details = DatabaseManager::singleton().detailsForNameAndOrigin(databaseIdentifier, *[origin _core]);
    if (details.name().isNull())
        return nil;

    return @{
        WebDatabaseDisplayNameKey: details.displayName().isEmpty() ? databaseIdentifier : (NSString *)details.displayName(),
        WebDatabaseExpectedSizeKey: @(details.expectedUsage()),
        WebDatabaseUsageKey: @(details.currentUsage()),
    };
}

- (void)deleteAllDatabases
{
    DatabaseTracker::singleton().deleteAllDatabasesImmediately();
#if PLATFORM(IOS_FAMILY)
    // FIXME: This needs to be removed once DatabaseTrackers in multiple processes
    // are in sync: <rdar://problem/9567500> Remove Website Data pane is not kept in sync with Safari
    [[NSFileManager defaultManager] removeItemAtPath:databasesDirectoryPath() error:NULL];
#endif
}

- (BOOL)deleteOrigin:(WebSecurityOrigin *)origin
{
    return origin && DatabaseTracker::singleton().deleteOrigin([origin _core]->data());
}

- (BOOL)deleteDatabase:(NSString *)databaseIdentifier withOrigin:(WebSecurityOrigin *)origin
{
    return origin && DatabaseTracker::singleton().deleteDatabase([origin _core]->data(), databaseIdentifier);
}

// For DumpRenderTree support only
- (void)deleteAllIndexedDatabases
{
#if ENABLE(INDEXED_DATABASE)
    WebDatabaseProvider::singleton().deleteAllDatabases();
#endif
}

#if PLATFORM(IOS_FAMILY)

static bool isFileHidden(NSString *file)
{
    ASSERT([file length]);
    return [file characterAtIndex:0] == '.';
}

+ (void)removeEmptyDatabaseFiles
{
    NSString *databasesDirectory = databasesDirectoryPath();
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSArray *array = [fileManager contentsOfDirectoryAtPath:databasesDirectory error:0];
    if (!array)
        return;
    
    NSUInteger count = [array count];
    for (NSUInteger i = 0; i < count; ++i) {
        NSString *fileName = [array objectAtIndex:i];
        // Skip hidden files.
        if (![fileName length] || isFileHidden(fileName))
            continue;
        
        NSString *path = [databasesDirectory stringByAppendingPathComponent:fileName];
        // Look for directories that contain database files belonging to the same origins.
        BOOL isDirectory;
        if (![fileManager fileExistsAtPath:path isDirectory:&isDirectory] || !isDirectory)
            continue;
        
        // Make sure the directory is not a symbolic link that points to something else.
        NSDictionary *attributes = [fileManager attributesOfItemAtPath:path error:0];
        if ([attributes fileType] == NSFileTypeSymbolicLink)
            continue;
        
        NSArray *databaseFilesInOrigin = [fileManager contentsOfDirectoryAtPath:path error:0];
        NSUInteger databaseFileCount = [databaseFilesInOrigin count];
        NSUInteger deletedDatabaseFileCount = 0;
        for (NSUInteger j = 0; j < databaseFileCount; ++j) {
            NSString *dbFileName = [databaseFilesInOrigin objectAtIndex:j];
            // Skip hidden files.
            if (![dbFileName length] || isFileHidden(dbFileName))
                continue;
            
            NSString *dbFilePath = [path stringByAppendingPathComponent:dbFileName];
            
            // There shouldn't be any directories in this folder - but check for it anyway.
            if (![fileManager fileExistsAtPath:dbFilePath isDirectory:&isDirectory] || isDirectory)
                continue;
            
            if (DatabaseTracker::deleteDatabaseFileIfEmpty(dbFilePath))
                ++deletedDatabaseFileCount;
        }
        
        // If we have removed every database file for this origin, delete the folder for this origin.
        if (databaseFileCount == deletedDatabaseFileCount || ![fileManager contentsOfDirectoryAtPath:path error:nullptr].count) {
            // Use rmdir - we don't want the deletion to happen if the folder is not empty.
            rmdir([path fileSystemRepresentation]);
        }
    }
}

+ (void)scheduleEmptyDatabaseRemoval
{
    DatabaseTracker::emptyDatabaseFilesRemovalTaskWillBeScheduled();
    
    dispatch_async(dispatch_get_global_queue(0, 0), ^{
        [WebDatabaseManager removeEmptyDatabaseFiles];
        DatabaseTracker::emptyDatabaseFilesRemovalTaskDidFinish();
    });
}

#endif // PLATFORM(IOS_FAMILY)

@end

static NSString *databasesDirectoryPath()
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *databasesDirectory = [defaults objectForKey:WebDatabaseDirectoryDefaultsKey];
    if (!databasesDirectory || ![databasesDirectory isKindOfClass:[NSString class]])
        databasesDirectory = @"~/Library/WebKit/Databases";
    
    return [databasesDirectory stringByStandardizingPath];
}
