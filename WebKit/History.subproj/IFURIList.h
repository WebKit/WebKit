/*	WKURIList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

#import "WKURIEntry.h"

typedef struct WKURIListNode WKURIListNode;

@interface WKURIList : NSObject 
{
    WKURIListNode *_head;
    WKURIListNode *_tail;
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

-(WKURIEntry *)addURL:(NSURL *)url withTitle:(NSString *)title;
-(void)addEntry:(WKURIEntry *)entry;
-(WKURIEntry *)removeURL:(NSURL *)url;
-(BOOL)removeEntry:(WKURIEntry *)entry;

-(WKURIEntry *)entryForURL:(NSURL *)url;
-(WKURIEntry *)entryAtIndex:(int)index;
-(WKURIEntry *)removeEntryAtIndex:(int)index;
-(void)removeEntriesToIndex:(int)index;

@end
