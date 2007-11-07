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

#import "WebDatabaseManager.h"
#import "WebDatabaseManagerPrivate.h"

#import <WebCore/DatabaseTracker.h>

using namespace WebCore;

const NSString *WebDatabaseDirectoryDefaultsKey = @"WebDatabaseDirectory";
const NSString *WebDatabaseNameKey = @"WebDatabaseNameKey";
const NSString *WebDatabaseSizeKey = @"WebDatabaseSizeKey";

@implementation WebDatabaseManager

+ (NSArray *)origins
{
    NSMutableArray *array = [[NSMutableArray alloc] init];

    const HashSet<String>& origins = DatabaseTracker::tracker().origins();
    HashSet<String>::const_iterator iter = origins.begin();
    HashSet<String>::const_iterator end = origins.end();

    for (; iter != end; ++iter)
        [array addObject:(NSString *)(*iter)];

    return [array autorelease];
}

+ (NSArray *)databasesWithOrigin:(NSString *)origin
{
    Vector<String> result;
    if (!DatabaseTracker::tracker().databaseNamesForOrigin(origin, result))
        return nil;

    NSMutableArray *array = [[NSMutableArray alloc] initWithCapacity:result.size()];

    for (unsigned i = 0; i < result.size(); ++i)
        [array addObject:(NSString *)result[i]];

    return array;
}

+ (void)deleteAllDatabases
{
    DatabaseTracker::tracker().deleteAllDatabases();
}

+ (void)deleteAllDatabasesWithOrigin:(NSString *)origin
{
    DatabaseTracker::tracker().deleteAllDatabasesForOrigin(origin);
}

+ (void)deleteDatabaseWithOrigin:(NSString *)origin named:(NSString *)name
{
    DatabaseTracker::tracker().deleteDatabase(origin, name);
}

@end

void WebKitSetWebDatabasesPathIfNecessary()
{
    static BOOL pathSet = NO;
    if (pathSet)
        return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];

    NSString *databasesDirectory = [defaults objectForKey:WebDatabaseDirectoryDefaultsKey];
    if (!databasesDirectory)
        databasesDirectory = @"~/Library/WebKit/Databases";

    DatabaseTracker::tracker().setDatabasePath([databasesDirectory stringByStandardizingPath]);

    pathSet = YES;
}
