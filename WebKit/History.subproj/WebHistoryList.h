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

-(WebHistoryItem *)addURL:(NSURL *)URL withTitle:(NSString *)title;
-(void)addEntry:(WebHistoryItem *)entry;
-(WebHistoryItem *)removeURL:(NSURL *)URL;
-(BOOL)removeEntry:(WebHistoryItem *)entry;

-(WebHistoryItem *)entryForURL:(NSURL *)URL;
-(WebHistoryItem *)entryAtIndex:(int)index;
-(WebHistoryItem *)removeEntryAtIndex:(int)index;
-(void)removeEntriesToIndex:(int)index;

@end
