//
//  WebHistory.h
//  WebKit
//
//  Created by John Sullivan on Mon Feb 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebCore/WebCoreHistory.h>

@class WebHistoryItem;
@class WebHistoryPrivate;

// notification sent when history is modified
extern NSString *WebHistoryEntriesChangedNotification;

@interface WebHistory : WebCoreHistory {
@private
    WebHistoryPrivate *_historyPrivate;
}

+ (WebHistory *)webHistoryWithFile: (NSString *)file;
- (id)initWithFile: (NSString *)file;

// modifying contents
- (void)addEntry: (WebHistoryItem *)entry;
- (void)addEntries:(NSArray *)newEntries;
- (void)removeEntry: (WebHistoryItem *)entry;
- (void)removeEntries: (NSArray *)entries;
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

// get an array of WebHistoryItem that were last visited on the day represented by the
// specified NSCalendarDate, ordered from most recent to oldest.
- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)calendarDate;

// testing contents for visited-link mechanism
- (BOOL)containsURL: (NSURL *)URL;

// retreiving information for a specific URL
- (WebHistoryItem *)entryForURL:(NSURL *)URL;

// storing contents on disk

// The file path used for storing history, specified in -[WebHistory initWithFile:] or +[WebHistory webHistoryWithFile:]
- (NSString *)file;

// Load history from file. This happens automatically at init time, and need not normally be called.
- (BOOL)loadHistory;

// Save history to file. It is the client's responsibility to call this at appropriate times.
- (BOOL)saveHistory;

@end
