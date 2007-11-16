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

extern const NSString *WebDatabaseDisplayNameKey;
extern const NSString *WebDatabaseExpectedSizeKey;
extern const NSString *WebDatabaseUsageKey;

// Posted with an origin is created from scratch, gets a new database, has a database deleted, has a quota change, etc
extern const NSString *WebDatabaseDidModifyOriginNotification;

// Posted when a database is created, its size increases, its display name changes, or its estimated size changes, or the database is removed
extern const NSString *WebDatabaseDidModifyDatabaseNotification;

@class WebSecurityOrigin;

@interface WebDatabaseManager : NSObject
{
}

+ (WebDatabaseManager *) sharedWebDatabaseManager;

// Will return an array of WebSecurityOrigins
- (NSArray *)origins;

// Will return an array of strings, the names of each database in the given origin
- (NSArray *)databasesWithOrigin:(WebSecurityOrigin *)origin;

// Will return the dictionary describing everything about the database for the passed origin and name
- (NSDictionary *)detailsForDatabase:(NSString *)databaseName withOrigin:(WebSecurityOrigin *)origin;

- (void)deleteAllDatabases;
- (void)deleteDatabasesWithOrigin:(WebSecurityOrigin *)origin;
- (void)deleteDatabase:(NSString *)databaseName withOrigin:(WebSecurityOrigin *)origin;

@end
