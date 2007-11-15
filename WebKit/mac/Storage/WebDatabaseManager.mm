/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#import "WebDatabaseManagerInternal.h"

#import "WebDatabaseTrackerClient.h"
#import "WebSecurityOriginPrivate.h"

#import <WebCore/DatabaseTracker.h>
#import <WebCore/SecurityOriginData.h>

using namespace WebCore;

const NSString *WebDatabaseDirectoryDefaultsKey = @"WebDatabaseDirectory";

const NSString *WebDatabaseOriginKey = @"WebDatabaseOriginKey";
const NSString *WebDatabaseOriginQuotaKey = @"WebDatabaseOriginQuotaKey";
const NSString *WebDatabaseOriginUsageKey = @"WebDatabaseOriginUsageKey";

const NSString *WebDatabaseNameKey = @"WebDatabaseNameKey";
const NSString *WebDatabaseDisplayNameKey = @"WebDatabaseDisplayNameKey";
const NSString *WebDatabaseExpectedSizeKey = @"WebDatabaseExpectedSizeKey";
const NSString *WebDatabaseUsageKey = @"WebDatabaseUsageKey";

const NSString *WebDatabaseDidModifyOriginNotification = @"WebDatabaseDidModifyOriginNotification";
const NSString *WebDatabaseDidModifyDatabaseNotification = @"WebDatabaseDidModifyDatabaseNotification";

@implementation WebDatabaseManager

+ (WebDatabaseManager *) sharedWebDatabaseManager
{
    static WebDatabaseManager *sharedManager = [[WebDatabaseManager alloc] init];
    return sharedManager;
}

- (NSArray *)origins
{
    return nil;
}

- (NSDictionary *)detailsForOrigin:(WebSecurityOrigin *)origin
{
    return nil;
}

- (NSArray *)databasesWithOrigin:(WebSecurityOrigin *)origin
{
    return nil;
}

- (NSDictionary *)detailsForDatabase:(NSString *)databaseName withOrigin:(WebSecurityOrigin *)origin
{
    return nil;
}

- (void)setQuota:(unsigned long long)quota forOrigin:(WebSecurityOrigin *)origin
{

}

- (void)deleteAllDatabases
{
    DatabaseTracker::tracker().deleteAllDatabases();
}

- (void)deleteDatabasesWithOrigin:(WebSecurityOrigin *)origin
{
    SecurityOriginData coreOrigin([origin protocol], [origin domain], [origin port]);

    DatabaseTracker::tracker().deleteDatabasesWithOrigin(coreOrigin);
}

- (void)deleteDatabase:(NSString *)databaseName withOrigin:(WebSecurityOrigin *)origin
{
    SecurityOriginData coreOrigin([origin protocol], [origin domain], [origin port]);
    
    DatabaseTracker::tracker().deleteDatabase(coreOrigin, databaseName);
}
@end

void WebKitInitializeDatabasesIfNecessary()
{
    static BOOL initialized = NO;
    if (initialized)
        return;

    // Set the database root path in WebCore
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSString *databasesDirectory = [defaults objectForKey:WebDatabaseDirectoryDefaultsKey];
    if (!databasesDirectory || ![databasesDirectory isKindOfClass:[NSString class]])
        databasesDirectory = @"~/Library/WebKit/Databases";

    DatabaseTracker::tracker().setDatabasePath([databasesDirectory stringByStandardizingPath]);

    // Set the DatabaseTrackerClient
    DatabaseTracker::tracker().setClient(WebDatabaseTrackerClient::sharedWebDatabaseTrackerClient());
    
    initialized = YES;
}
