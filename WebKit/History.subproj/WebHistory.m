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

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSURLExtras.h>

#import <WebCore/WebCoreHistory.h>


NSString *WebHistoryItemsAddedNotification = @"WebHistoryItemsAddedNotification";
NSString *WebHistoryItemsRemovedNotification = @"WebHistoryItemsRemovedNotification";
NSString *WebHistoryAllItemsRemovedNotification = @"WebHistoryAllItemsRemovedNotification";
NSString *WebHistoryLoadedNotification = @"WebHistoryLoadedNotification";

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

+ (WebHistory *)sharedHistory
{
    return _sharedHistory;
}


+ (void)setSharedHistory: (WebHistory *)history
{
    // FIXME.  Need to think about multiple instances of WebHistory per application
    // and correct synchronization of history file between applications.
    [WebCoreHistory setHistoryProvider: [[[_WebCoreHistoryProvider alloc] initWithHistory: history] autorelease]];
    if (_sharedHistory != history){
        [_sharedHistory release];
        _sharedHistory = [history retain];
    }
}

- (id)initWithFile: (NSString *)file;
{
    if ((self = [super init]) != nil) {
        _historyPrivate = [[WebHistoryPrivate alloc] initWithFile:file];
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
    NSDictionary *userInfo = [NSDictionary dictionaryWithObjectsAndKeys:entries, @"Entries", nil];
    [[NSNotificationCenter defaultCenter]
        postNotificationName: name object: self userInfo: userInfo];
}

- (WebHistoryItem *)addItemForURL: (NSURL *)URL
{
    WebHistoryItem *entry = [[WebHistoryItem alloc] initWithURL:URL title:nil];
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

- (NSString *)file
{
    return [_historyPrivate file];
}

- (BOOL)loadHistory
{
    if ([_historyPrivate loadHistory]) {
        [[NSNotificationCenter defaultCenter]
            postNotificationName: WebHistoryLoadedNotification
                          object: self];
        return YES;
    }
    return NO;
}

- (BOOL)saveHistory
{
    return [_historyPrivate saveHistory];
}

@end
