//
//  WebHistoryPrivate.h
//  WebKit
//
//  Created by John Sullivan on Tue Feb 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WebHistoryItem;

@interface WebHistoryPrivate : NSObject {
@private
    NSMutableDictionary *_entriesByURL;
    NSMutableArray *_datesWithEntries;
    NSMutableArray *_entriesByDate;
    NSString *_file;
}

- (id)initWithFile: (NSString *)file;

- (void)addEntry: (WebHistoryItem *)entry;
- (void)addEntries:(NSArray *)newEntries;
- (BOOL)removeEntry: (WebHistoryItem *)entry;
- (BOOL)removeEntries: (NSArray *)entries;
- (BOOL)removeAllEntries;
- (WebHistoryItem *)updateURL:(NSString *)newURLString
                    title:(NSString *)newTitle
             displayTitle:(NSString *)newDisplayTitle
                   forURL:(NSString *)oldURLString;

- (NSArray *)orderedLastVisitedDays;
- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)calendarDate;
- (NSArray *)entriesWithAddressContainingString: (NSString *)string;
- (NSArray *)entriesWithTitleOrAddressContainingString: (NSString *)string;
- (BOOL)containsURL: (NSURL *)URL;

- (NSString *)file;
- (BOOL)loadHistory;
- (BOOL)saveHistory;

@end
