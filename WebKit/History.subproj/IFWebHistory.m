//
//  IFWebHistory.m
//  WebKit
//
//  Created by John Sullivan on Mon Feb 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import "IFWebHistory.h"

static IFWebHistory *sharedWebHistory = nil;

@interface NSCalendarDate (IFExtensions)
- (int)daysSinceDate: (NSCalendarDate *)date;
@end

@implementation IFWebHistory

+ (IFWebHistory *)sharedWebHistory
{
    if (sharedWebHistory == nil) {
        sharedWebHistory = [[[self class] alloc] init];
    }

    return sharedWebHistory;
}

- (IFURIEntry *)createTestEntryWithURLString: (NSString *)urlString
                                       title: (NSString *)title
                                        date: (NSDate *)date
{
    IFURIEntry *entry = [[[IFURIEntry alloc] initWithURL: [NSURL URLWithString: urlString]
                                                  title: title] autorelease];
    [entry setLastVisitedDate: date];
    return entry;
    
}

- (NSArray *)testDataDates
{
    static NSArray *testDataDates;
    
    if (testDataDates == nil) {
        NSCalendarDate *today, *yesterday;
        
        today = [NSCalendarDate calendarDate];
        yesterday = [today dateByAddingYears:0 months:0 days:-1 hours:0 minutes:0 seconds:0];

        testDataDates = [NSArray arrayWithObjects: today, yesterday, nil];
        [testDataDates retain];
    }

    return testDataDates;
}

- (NSArray *)testData
{
    static NSArray *testData = nil;

    if (testData == nil) {
        NSCalendarDate *date1 = [[self testDataDates] objectAtIndex: 0];
        NSCalendarDate *date2 = [[self testDataDates] objectAtIndex: 1];

        testData = [NSArray arrayWithObjects:
            [NSArray arrayWithObjects:
                [self createTestEntryWithURLString: @"http://www.apple.com" title: @"Apple" date: date1],
                [self createTestEntryWithURLString: @"http://www.google.com" title: @"Google" date: date1],
                nil],
            [NSArray arrayWithObjects:
                [self createTestEntryWithURLString: @"http://www.amazon.com" title: @"Amazon" date: date2],
                [self createTestEntryWithURLString: @"http://www.salon.com" title: @"Salon" date: date2],
                nil],
            nil];
        [testData retain];
    }

    return testData;
}

#pragma mark MODIFYING CONTENTS

- (void)addEntry: (IFURIEntry *)entry
{
    //FIXME: not yet implemented
}

- (void)removeEntry: (IFURIEntry *)entry
{
    //FIXME: not yet implemented
}

- (void)removeAllEntries
{
    //FIXME: not yet implemented
}


#pragma mark DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    //FIXME: not yet implemented
    return [self testDataDates];
}

- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)date
{
    //FIXME: not yet implemented
    int index, count;
    NSArray *dataDates;

    dataDates = [self testDataDates];
    count = [dataDates count];
    for (index = 0; index < count; ++index) {
        if ([date daysSinceDate: [dataDates objectAtIndex: index]] == 0) {
            return [[self testData] objectAtIndex: index];
        }
    }
    
    return nil;
}

#pragma mark STRING-BASED RETRIEVAL

- (NSArray *)entriesWithAddressContainingString: (NSString *)string
{
    // FIXME: not yet implemented
    return nil;
}

- (NSArray *)entriesWithTitleOrAddressContainingString: (NSString *)string
{
    // FIXME: not yet implemented
    return nil;
}

#pragma mark URL MATCHING

- (BOOL)containsURL: (NSURL *)url
{
    // FIXME: not yet implemented
    return NO;
}

@end

@implementation NSCalendarDate (IFExtensions)

- (int)daysSinceDate: (NSCalendarDate *)date
{
    int deltaDays;

    if (self == date) {
        deltaDays = 0;
    } else {
        [self years:NULL months:NULL days:&deltaDays
            hours:NULL minutes:NULL seconds:NULL sinceDate: date];
    }

    return deltaDays;
}

@end
