//
//  IFWebHistory.h
//  WebKit
//
//  Created by John Sullivan on Mon Feb 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <WebKit/IFURIEntry.h>

@class IFWebHistoryPrivate;

// notification sent when history is modified
#define IFWebHistoryEntriesChangedNotification		@"IFWebHistoryEntriesChangedNotification"

@interface IFWebHistory : NSObject {
@private
    IFWebHistoryPrivate *_historyPrivate;
}

+ (IFWebHistory *)webHistoryWithFile: (NSString *)file;
- (id)initWithFile: (NSString *)file;

// modifying contents
- (void)addEntry: (IFURIEntry *)entry;
- (void)removeEntry: (IFURIEntry *)entry;
- (void)removeEntriesForDay: (NSCalendarDate *)calendarDate;
- (void)removeAllEntries;

// Update an entry in place. Any nil "new" parameters aren't updated.
- (void)updateURL:(NSString *)newURLString
            title:(NSString *)newTitle
     displayTitle:(NSString *)newDisplayTitle
           forURL:(NSString *)oldURLString;

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

// storing contents on disk

// The file path used for storing history, specified in -[IFWebHistory initWithFile:] or +[IFWebHistory webHistoryWithFile:]
- (NSString *)file;

// Load history from file. This happens automatically at init time, and need not normally be called.
- (BOOL)loadHistory;

// Save history to file. It is the client's responsibility to call this at appropriate times.
- (BOOL)saveHistory;

@end
