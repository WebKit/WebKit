//
//  IFWebHistory.h
//  WebKit
//
//  Created by John Sullivan on Mon Feb 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <WebKit/IFURIEntry.h>

@interface IFWebHistory : NSObject {
@private
    id _historyPrivate;
}

+ (IFWebHistory *)sharedWebHistory;

// modifying contents
- (void)addEntry: (IFURIEntry *)entry;
- (void)removeEntry: (IFURIEntry *)entry;
- (void)removeAllEntries;

// retrieving contents for date-based presentation

// get an array of NSCalendarDate, each one representing a unique day that contains one
// or more history entries, ordered from most recent to oldest.
- (NSArray *)orderedLastVisitedDays;

// get an array of IFURIEntry that were last visited on the day represented by the
// specified NSCalendarDate, ordered from most recent to oldest.
- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)calendarDate;

// retrieving contents for autocompletion in location field
- (NSArray *)entriesWithAddressContainingString: (NSString *)string;

// retrieving contents for searching (maybe replace with state-based search API)
- (NSArray *)entriesWithTitleOrAddressContainingString: (NSString *)string;

// testing contents for visited-link mechanism
- (BOOL)containsURL: (NSURL *)url;

@end
