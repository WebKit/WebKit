//
//  IFWebHistoryPrivate.h
//  WebKit
//
//  Created by John Sullivan on Tue Feb 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class IFURIEntry;

@interface IFWebHistoryPrivate : NSObject {
@private
    NSMutableDictionary *_urlDictionary;
    NSMutableArray *_datesWithEntries;
    NSMutableArray *_entriesByDate;
    NSString *_file;
}

- (id)initWithFile: (NSString *)file;

- (void)addEntry: (IFURIEntry *)entry;
- (void)addEntries:(NSArray *)newEntries;
- (BOOL)removeEntry: (IFURIEntry *)entry;
- (BOOL)removeEntries: (NSArray *)entries;
- (BOOL)removeAllEntries;
- (IFURIEntry *)updateURL:(NSString *)newURLString
                    title:(NSString *)newTitle
             displayTitle:(NSString *)newDisplayTitle
                   forURL:(NSString *)oldURLString;

- (NSArray *)orderedLastVisitedDays;
- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)calendarDate;
- (NSArray *)entriesWithAddressContainingString: (NSString *)string;
- (NSArray *)entriesWithTitleOrAddressContainingString: (NSString *)string;
- (BOOL)containsURL: (NSURL *)url;

- (NSString *)file;
- (BOOL)loadHistory;
- (BOOL)saveHistory;

@end
