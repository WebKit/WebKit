//
//  WebHistory.m
//  WebKit
//
//  Created by John Sullivan on Mon Feb 18 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//
#import <WebKit/WebHistory.h>
#import <WebKit/WebHistoryPrivate.h>
#import <WebKit/WebHistoryItem.h>
#import <WebKit/WebHistoryItemPrivate.h>

#import <WebFoundation/NSError.h>
#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebNSURLExtras.h>

#import <WebCore/WebCoreHistory.h>


NSString *WebHistoryItemsAddedNotification = @"WebHistoryItemsAddedNotification";
NSString *WebHistoryItemsRemovedNotification = @"WebHistoryItemsRemovedNotification";
NSString *WebHistoryAllItemsRemovedNotification = @"WebHistoryAllItemsRemovedNotification";
NSString *WebHistoryLoadedNotification = @"WebHistoryLoadedNotification";
NSString *WebHistorySavedNotification = @"WebHistorySavedNotification";
NSString *WebHistoryItemsKey = @"WebHistoryItems";

static WebHistory *_sharedHistory = nil;

@interface _WebCoreHistoryProvider : NSObject  <WebCoreHistoryProvider> 
{
    WebHistory *history;
}
- initWithHistory: (WebHistory *)h;
@end

@implementation _WebCoreHistoryProvider
- initWithHistory: (WebHistory *)h
{
    history = [h retain];
    return self;
}

- (BOOL)containsItemForURLString: (NSString *)URLString
{
    return [history containsItemForURLString: URLString];
}

- (void)dealloc
{
    [history release];
    [super dealloc];
}

@end

@implementation WebHistory

+ (WebHistory *)optionalSharedHistory
{
    return _sharedHistory;
}


+ (void)setOptionalSharedHistory: (WebHistory *)history
{
    // FIXME.  Need to think about multiple instances of WebHistory per application
    // and correct synchronization of history file between applications.
    [WebCoreHistory setHistoryProvider: [[[_WebCoreHistoryProvider alloc] initWithHistory: history] autorelease]];
    if (_sharedHistory != history){
        [_sharedHistory release];
        _sharedHistory = [history retain];
    }
}

- (id)init
{
    if ((self = [super init]) != nil) {
        _historyPrivate = [[WebHistoryPrivate alloc] init];
    }

    return self;
}

- (void)dealloc
{
    [_historyPrivate release];
    [super dealloc];
}

#pragma mark MODIFYING CONTENTS

- (void)_sendNotification:(NSString *)name entries:(NSArray *)entries
{
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:entries, WebHistoryItemsKey, nil];
    [[NSNotificationCenter defaultCenter]
        postNotificationName: name object: self userInfo: userInfo];
}

- (WebHistoryItem *)addItemForURL: (NSURL *)URL
{
    WebHistoryItem *entry = [[WebHistoryItem alloc] initWithURL:URL title:nil];
    [entry _setLastVisitedTimeInterval: [NSDate timeIntervalSinceReferenceDate]];
    [self addItem: entry];
    [entry release];
    return entry;
}


- (void)addItem: (WebHistoryItem *)entry
{
    [_historyPrivate addItem: entry];
    [self _sendNotification: WebHistoryItemsAddedNotification
                    entries: [NSArray arrayWithObject:entry]];
}

- (void)removeItem: (WebHistoryItem *)entry
{
    if ([_historyPrivate removeItem: entry]) {
        [self _sendNotification: WebHistoryItemsRemovedNotification
                        entries: [NSArray arrayWithObject:entry]];
    }
}

- (void)removeItems: (NSArray *)entries
{
    if ([_historyPrivate removeItems:entries]) {
        [self _sendNotification: WebHistoryItemsRemovedNotification
                        entries: entries];
    }
}

- (void)removeAllItems
{
    if ([_historyPrivate removeAllItems]) {
        [[NSNotificationCenter defaultCenter]
            postNotificationName: WebHistoryAllItemsRemovedNotification
                          object: self];
    }
}

- (void)addItems:(NSArray *)newEntries
{
    [_historyPrivate addItems:newEntries];
    [self _sendNotification: WebHistoryItemsAddedNotification
                    entries: newEntries];
}

#pragma mark DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    return [_historyPrivate orderedLastVisitedDays];
}

- (NSArray *)orderedItemsLastVisitedOnDay: (NSCalendarDate *)date
{
    return [_historyPrivate orderedItemsLastVisitedOnDay: date];
}

#pragma mark URL MATCHING

- (BOOL)containsItemForURLString: (NSString *)URLString
{
    return [_historyPrivate containsItemForURLString: URLString];
}

- (BOOL)containsURL: (NSURL *)URL
{
    return [_historyPrivate containsURL: URL];
}

- (WebHistoryItem *)itemForURL:(NSURL *)URL
{
    return [_historyPrivate itemForURL:URL];
}

#pragma mark SAVING TO DISK

- (BOOL)loadFromURL:(NSURL *)URL error:(NSError **)error
{
    if ([_historyPrivate loadFromURL:URL error:error]) {
        [[NSNotificationCenter defaultCenter]
            postNotificationName: WebHistoryLoadedNotification
                          object: self];
        return YES;
    }
    return NO;
}

- (BOOL)saveToURL:(NSURL *)URL error:(NSError **)error
{
    // FIXME:  Use new foundation API to get error when ready.
    if([_historyPrivate saveToURL:URL error:error]){
        [[NSNotificationCenter defaultCenter]
            postNotificationName: WebHistorySavedNotification
                          object: self];
        return YES;
    }
    return NO;    
}

- (WebHistoryItem *)_itemForURLString:(NSString *)URLString
{
    return [_historyPrivate itemForURLString: URLString];
}


@end
