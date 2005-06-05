/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
