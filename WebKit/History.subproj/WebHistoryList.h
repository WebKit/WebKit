/*	WebHistoryList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebHistoryItem;

typedef struct WebHistoryListNode WebHistoryListNode;

@interface WebHistoryList : NSObject 
{
    WebHistoryListNode *_head;
    WebHistoryListNode *_tail;
    int _count;
    BOOL _allowsDuplicates;
    int _maximumSize;
}

-(id)init;

-(BOOL)allowsDuplicates;
-(void)setAllowsDuplicates:(BOOL)allowsDuplicates;

-(int)count;

-(int)maximumSize;
-(void)setMaximumSize:(int)size;

-(WebHistoryItem *)entryAtIndex:(int)index;
-(WebHistoryItem *)entryForURL:(NSURL *)URL;

-(void)addEntry:(WebHistoryItem *)entry;

-(void)removeEntry:(WebHistoryItem *)entry;
-(WebHistoryItem *)removeEntryAtIndex:(int)index;
-(void)removeEntriesToIndex:(int)index;

-(void)replaceEntryAtIndex:(int)index withEntry:(WebHistoryItem *)entry;

@end
