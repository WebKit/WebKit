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
    @param flag Set to YES if pages should be cached
    @discussion Pages in the back/forward list may be cached.  Pages in this cache
    will load much more quickly; however, they may not always be up-to-date.  The
    page cache may not apply to all pages.
*/
+ (void)setUsesPageCache: (BOOL)flag;

/*!
    @method usesPageCache
    @abstract Returns whether page cacheing is enabled. 
    @result YES if the page cache is enabled, otherwise NO. 
*/
+ (BOOL)usesPageCache;

/*!
    @method setPageCacheSize:
    @abstract Sets the size of the page cache.
    @param size The number of pages to allow in the page cache.
*/
+ (void)setPageCacheSize: (unsigned)size;

/*!
    @method pageCacheSize
    @abstract Returns the number of pages that may be cached.
    @result The number of pages that may be cached.
*/
+ (unsigned)pageCacheSize;

/*!
    @method clearPageCache
    @abstract Clears all items in the page cache. 
*/
- (void)clearPageCache;

/*!
    @method addEntry:
    @abstract Adds an entry to the list.
    @param entry The entry to add.
    @discussion The added entry is inserted immediately after the current entry.
    If the current position in the list is not at the end of the list, elements in the
    forward list will be dropped at this point.  In addition, entries may be dropped to keep
    the size of the list within the maximum size.
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
    @param entry The history item to move the pointer to
*/
- (void)goToEntry:(WebHistoryItem *)entry;

/*!
    @method backEntry
    @abstract Returns the entry right before the current entry.
    @result The entry right before the current entry, or nil if there isn't one.
*/
- (WebHistoryItem *)backEntry;

/*!
    @method currentEntry
    @abstract Returns the current entry.
    @result The current entry.
*/
- (WebHistoryItem *)currentEntry;

/*!
    @method forwardEntry
    @abstract Returns the entry right after the current entry.
    @result The entry right after the current entry, or nil if there isn't one.
*/
- (WebHistoryItem *)forwardEntry;

/*!
    @method containsEntry:
    @abstract Returns whether the receiver contains the given entry.
    @param entry The history item to search for.
    @result YES if the list contains the given entry, otherwise NO.
*/
- (BOOL)containsEntry:(WebHistoryItem *)entry;

/*!
    @method backListWithSizeLimit:
    @abstract Returns a portion of the list before the current entry.
    @param limit A cap on the size of the array returned.
    @result An array of items before the current entry, or nil if there are none.  The entries are in the order that they were originally visited.
*/
- (NSArray *)backListWithSizeLimit:(int)limit;

/*!
    @method forwardListWithSizeLimit:
    @abstract Returns a portion of the list after the current entry.
    @param limit A cap on the size of the array returned.
    @result An array of items after the current entry, or nil if there are none.  The entries are in the order that they were originally visited.
*/
- (NSArray *)forwardListWithSizeLimit:(int)limit;

/*!
    @method maximumSize
    @abstract Returns the list's maximum size.
    @result The list's maximum size.
*/
- (int)maximumSize;

/*!
    @method maximumSize
    @abstract Sets the list's maximum size.
    @param size The new maximum size for the list.
*/
- (void)setMaximumSize:(int)size;

/*!
    @method backListCount
    @abstract Returns the back list's current count.
    @result The number of items in the list.
*/
- (int)backListCount;

/*!
    @method entryAtIndex:
    @abstract Returns an entry the given distance from the current entry.
    @param index Index of the desired list item relative to the current item; 0 is current item, -1 is back item, 1 is forward item, etc.
    @result The entry the given distance from the current entry. If index exceeds the limits of the list, the entry furthest in that direction is returned.
*/
- (WebHistoryItem *)entryAtIndex:(int)index;

@end
