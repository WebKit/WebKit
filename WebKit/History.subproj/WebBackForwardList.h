/*	WebBackForwardList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebHistoryItem;

/*!
    @class WebBackForwardList
    WebBackForwardList holds an ordered list of WebHistoryItems that comprises the back and
    forward lists.
    
    Note that the methods which modify instances of this class do not cause
    navigation to happen in other layers of the stack;  they are only for maintaining this data
    structure.
*/
@interface WebBackForwardList : NSObject {
    @private
    NSMutableArray *_entries;
    int _current;
    int _maximumSize;
}

/*!
    @method setUsesPageCache:
    @param flag set to true if pages should be cached
    @abstract Pages in the back/forward list may be cached.  Pages in this cache
    will load much more quickly, however they may not always be up-to-date.  The
    page cache may not apply to all pages.
*/
+ (void)setUsesPageCache: (BOOL)flag;

/*!
    @method usesPageCache
    @result Returns YES if the page cache is enabled.
*/
+ (BOOL)usesPageCache;

/*!
    @method setPageCacheSize:
    @param size The number of pages to allow in the page cache.
*/
+ (void)setPageCacheSize: (unsigned)size;

/*!
    @method pageCacheSize
    @result Returns the number of pages that may be cached.
*/
+ (unsigned)pageCacheSize;

/*!
    @method clearPageCache
    @discussion Clears all items in the page cache. 
*/
- (void)clearPageCache;

/*!
    @method addEntry:
    @abstract Adds an entry to the list.
    @discussion Add an entry to the back-forward list, immediately after the current entry.
    If the current position in the list is not at the end of the list, elements in the
    forward list will be dropped at this point.  In addition, entries may be dropped to keep
    the size of the list within the maximum size.
    @param entry The entry to add.
*/    
- (void)addEntry:(WebHistoryItem *)entry;

/*!
    @method goBack
    @abstract Move the current pointer back to the entry before the current entry.
*/
- (void)goBack;

/*!
    @method goForward
    @abstract Move the current pointer ahead to the entry after the current entry.
*/
- (void)goForward;

/*!
    @method goToEntry:
    @abstract Move the current pointer to the given entry.
*/
- (void)goToEntry:(WebHistoryItem *)entry;

/*!
    @method backEntry
    @result Returns the entry right before the current entry, or nil if there isn't one.
*/
- (WebHistoryItem *)backEntry;

/*!
    @method currentEntry
    @result Returns the current entry.
*/
- (WebHistoryItem *)currentEntry;

/*!
    @method forwardEntry
    @result Returns the entry right after the current entry, or nil if there isn't one.
*/
- (WebHistoryItem *)forwardEntry;

/*!
    @method containsEntry:
    @result Returns whether the receiver contains the given entry.
*/
- (BOOL)containsEntry:(WebHistoryItem *)entry;

/*!
    @method backListWithSizeLimit:
    @param limit A cap on the size of the array returned.
    @result Returns a portion of the list before current entry, or nil if there are none.  The entries are in the order that they were originally visited.
*/
- (NSArray *)backListWithSizeLimit:(int)limit;

/*!
    @method forwardListWithSizeLimit:
    @param limit A cap on the size of the array returned.
    @result Returns a portion of the list after current entry, or nil if there are none.  The entries are in the order that they were originally visited.
*/
- (NSArray *)forwardListWithSizeLimit:(int)limit;

/*!
    @method maximumSize
    @result Returns this list's maximum size.
*/
- (int)maximumSize;

/*!
    @method maximumSize
    @abstract Sets this list's maximum size.
*/
- (void)setMaximumSize:(int)size;

/*!
    @method backListCount
    @result Returns the back list's current count.
*/
- (int)backListCount;

/*!
    @method entryAtIndex:
    @param index Index of the back/forward list item; 0 is current item, -1 is back item, 1 is forward item, etc.
    @result Returns an entry the given distance from the current entry, or the furthest in that direction if there is none.
*/
- (WebHistoryItem *)entryAtIndex:(int)index;

@end
