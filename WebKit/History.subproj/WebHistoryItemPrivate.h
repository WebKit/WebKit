/*
    WebHistoryItemPrivate.h
    Copyright 2001, 2002, Apple, Inc. All rights reserved.
 */
#import <Cocoa/Cocoa.h>

#import <WebKit/WebHistoryItem.h>


@interface WebHistoryItem (WebPrivate)
+ (void)_releaseAllPendingPageCaches;
- (BOOL)hasPageCache;
- (void)setHasPageCache: (BOOL)f;
- (NSMutableDictionary *)pageCache;
@end
