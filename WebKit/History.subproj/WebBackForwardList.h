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

@end
