//
//  IFWebHistory.m
//  WebKit
//
//  Created by John Sullivan on Mon Feb 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFWebHistory.h"
#import "IFWebHistoryPrivate.h"

static IFWebHistory *sharedWebHistory = nil;

@implementation IFWebHistory

+ (IFWebHistory *)sharedWebHistory
{
    if (sharedWebHistory == nil) {
        sharedWebHistory = [[[self class] alloc] init];
    }

    return sharedWebHistory;
}

- (id)init
{
    if ((self = [super init]) != nil) {
        _historyPrivate = [[IFWebHistoryPrivate alloc] init];
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
        postNotificationName: IFWebHistoryEntriesChangedNotification
                      object: self];
}

- (void)addEntry: (IFURIEntry *)entry
{
    [_historyPrivate addEntry: entry];
    [self sendEntriesChangedNotification];
}

- (void)removeEntry: (IFURIEntry *)entry
{
    if ([_historyPrivate removeEntry: entry]) {
        [self sendEntriesChangedNotification];
    }
}

- (void)removeEntriesForDay: (NSCalendarDate *)calendarDate
{
    if ([_historyPrivate removeEntriesForDay: calendarDate]) {
        [self sendEntriesChangedNotification];
    }
}

- (void)removeAllEntries
{
    if ([_historyPrivate removeAllEntries]) {
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

@end
