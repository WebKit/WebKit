//
//  WebHistoryPrivate.h
//  WebKit
//
//  Created by John Sullivan on Tue Feb 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

#import <WebKit/WebHistory.h>

@class WebHistoryItem;
@class NSError;

@interface WebHistoryPrivate : NSObject {
@private
    NSMutableDictionary *_entriesByURL;
    NSMutableArray *_datesWithEntries;
    NSMutableArray *_entriesByDate;
    BOOL itemLimitSet;
    int itemLimit;
    BOOL ageInDaysLimitSet;
    int ageInDaysLimit;
}

- (void)addItem:(WebHistoryItem *)entry;
- (void)addItems:(NSArray *)newEntries;
- (BOOL)removeItem:(WebHistoryItem *)entry;
- (BOOL)removeItems:(NSArray *)entries;
- (BOOL)removeAllItems;

- (NSArray *)orderedLastVisitedDays;
- (NSArray *)orderedItemsLastVisitedOnDay:(NSCalendarDate *)calendarDate;
- (BOOL)containsItemForURLString:(NSString *)URLString;
- (BOOL)containsURL:(NSURL *)URL;
- (WebHistoryItem *)itemForURL:(NSURL *)URL;
- (WebHistoryItem *)itemForURLString:(NSString *)URLString;

- (BOOL)loadFromURL:(NSURL *)URL error:(NSError **)error;
- (BOOL)saveToURL:(NSURL *)URL error:(NSError **)error;

- (NSCalendarDate*)_ageLimitDate;

- (void)setHistoryItemLimit:(int)limit;
- (int)historyItemLimit;
- (void)setHistoryAgeInDaysLimit:(int)limit;
- (int)historyAgeInDaysLimit;

@end

@interface WebHistory (WebPrivate)
- (void)removeItem:(WebHistoryItem *)entry;
- (void)addItem:(WebHistoryItem *)entry;

- (BOOL)loadHistory;
- initWithFile:(NSString *)file;
- (WebHistoryItem *)addItemForURL:(NSURL *)URL;
- (BOOL)containsItemForURLString:(NSString *)URLString;
- (WebHistoryItem *)_itemForURLString:(NSString *)URLString;
- (NSCalendarDate*)ageLimitDate;

@end
