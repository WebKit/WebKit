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

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebNSURLExtras.h>

NSString *WebHistoryEntriesAddedNotification = @"WebHistoryEntriesAddedNotification";
NSString *WebHistoryEntriesRemovedNotification = @"WebHistoryEntriesRemovedNotification";
NSString *WebHistoryAllEntriesRemovedNotification = @"WebHistoryAllEntriesRemovedNotification";
NSString *WebHistoryLoadedNotification = @"WebHistoryLoadedNotification";

@implementation WebHistory

+ (WebHistory *)sharedHistory
{
    return (WebHistory *)[super sharedHistory];
}


+ (WebHistory *)webHistoryWithFile: (NSString*)file
{
    // Should only be called once.  Need to rationalize usage
    // of history.    
    WebHistory *h = [[self alloc] initWithFile:file];
    [[self class] setSharedHistory: h];
    [h release];
    
    return h;
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

- (WebHistoryItem *)addEntryForURL: (NSURL *)URL
{
    WebHistoryItem *entry = [[WebHistoryItem alloc] initWithURL:URL title:nil];
    [self addEntry: entry];
    [entry release];
    return entry;
}


- (void)addEntry: (WebHistoryItem *)entry
{
    [_historyPrivate addEntry: entry];
    [self _sendNotification: WebHistoryEntriesAddedNotification
                    entries: [NSArray arrayWithObject:entry]];
}

- (void)removeEntry: (WebHistoryItem *)entry
{
    if ([_historyPrivate removeEntry: entry]) {
        [self _sendNotification: WebHistoryEntriesRemovedNotification
                        entries: [NSArray arrayWithObject:entry]];
    }
}

- (void)removeEntries: (NSArray *)entries
{
    if ([_historyPrivate removeEntries:entries]) {
        [self _sendNotification: WebHistoryEntriesRemovedNotification
                        entries: entries];
    }
}

- (void)removeAllEntries
{
    if ([_historyPrivate removeAllEntries]) {
        [[NSNotificationCenter defaultCenter]
            postNotificationName: WebHistoryAllEntriesRemovedNotification
                          object: self];
    }
}

- (void)addEntries:(NSArray *)newEntries
{
    [_historyPrivate addEntries:newEntries];
    [self _sendNotification: WebHistoryEntriesAddedNotification
                    entries: newEntries];
}

#pragma mark DATE-BASED RETRIEVAL

- (NSArray *)orderedLastVisitedDays
{
    return [_historyPrivate orderedLastVisitedDays];
}

- (NSArray *)orderedEntriesLastVisitedOnDay: (NSCalendarDate *)date
{
    return [_historyPrivate orderedEntriesLastVisitedOnDay: date];
}

#pragma mark URL MATCHING

- (BOOL)containsEntryForURLString: (NSString *)URLString
{
    return [_historyPrivate containsEntryForURLString: URLString];
}

- (BOOL)containsURL: (NSURL *)URL
{
    return [_historyPrivate containsURL: URL];
}

- (WebHistoryItem *)entryForURL:(NSURL *)URL
{
    return [_historyPrivate entryForURL:URL];
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
