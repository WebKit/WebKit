//
//  WebHistory.m
//  WebKit
//
//  Created by John Sullivan on Mon Feb 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "WebHistory.h"
#import "WebHistoryPrivate.h"

@implementation WebHistory

+ (WebHistory *)webHistoryWithFile: (NSString*)file
{
    return [[[self alloc] initWithFile:file] autorelease];
}

- (id)initWithFile: (NSString *)file;
{
    if ((self = [super init]) != nil) {
        _historyPrivate = [[WebHistoryPrivate alloc] initWithFile:file];
    }

    return self;
}

- (void)dealloc
{
    [_historyPrivate release];
    [super dealloc];
}

#pragma mark MODIFYING CONTENTS

- (void)sendEntriesChangedNotification
{
    [[NSNotificationCenter defaultCenter]
        postNotificationName: WebHistoryEntriesChangedNotification
                      object: self];
}

- (void)addEntry: (WebHistoryItem *)entry
{
    [_historyPrivate addEntry: entry];
    [self sendEntriesChangedNotification];
}

- (void)removeEntry: (WebHistoryItem *)entry
{
    if ([_historyPrivate removeEntry: entry]) {
        [self sendEntriesChangedNotification];
    }
}

- (void)removeEntries: (NSArray *)entries
{
    if ([_historyPrivate removeEntries:entries]) {
        [self sendEntriesChangedNotification];
    }
}

- (void)removeAllEntries
{
    if ([_historyPrivate removeAllEntries]) {
        [self sendEntriesChangedNotification];
    }
}

- (void)addEntries:(NSArray *)newEntries
{
    [_historyPrivate addEntries:newEntries];
    [self sendEntriesChangedNotification];
}

- (void)updateURL:(NSString *)newURLString
            title:(NSString *)newTitle
     displayTitle:(NSString *)newDisplayTitle
           forURL:(NSString *)oldURLString
{
    if ([_historyPrivate updateURL:newURLString title:newTitle displayTitle:newDisplayTitle forURL:oldURLString] != nil) {
        // Consider passing changed entry as parameter to notification
        [self sendEntriesChangedNotification];
    }
}


#pragma mark DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    return [_historyPrivate orderedLastVisitedDays];
}

- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)date
{
    return [_historyPrivate orderedEntriesLastVisitedOnDay: date];
}

#pragma mark STRING-BASED RETRIEVAL

- (NSArray *)entriesWithAddressContainingString: (NSString *)string
{
    return [_historyPrivate entriesWithAddressContainingString: string];
}

- (NSArray *)entriesWithTitleOrAddressContainingString: (NSString *)string
{
    return [_historyPrivate entriesWithTitleOrAddressContainingString: string];
}

#pragma mark URL MATCHING

- (BOOL)containsURL: (NSURL *)url
{
    return [_historyPrivate containsURL: url];
}

#pragma mark SAVING TO DISK

- (NSString *)file
{
    return [_historyPrivate file];
}

- (BOOL)loadHistory
{
    if ([_historyPrivate loadHistory]) {
        [self sendEntriesChangedNotification];
        return YES;
    }
    return NO;
}

- (BOOL)saveHistory
{
    return [_historyPrivate saveHistory];
}

@end
