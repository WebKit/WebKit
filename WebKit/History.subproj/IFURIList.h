/*	IFURIList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class IFURIEntry;

typedef struct IFURIListNode IFURIListNode;

@interface IFURIList : NSObject 
{
    IFURIListNode *_head;
    IFURIListNode *_tail;
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

-(IFURIEntry *)addURL:(NSURL *)url withTitle:(NSString *)title;
-(void)addEntry:(IFURIEntry *)entry;
-(IFURIEntry *)removeURL:(NSURL *)url;
-(BOOL)removeEntry:(IFURIEntry *)entry;

-(IFURIEntry *)entryForURL:(NSURL *)url;
-(IFURIEntry *)entryAtIndex:(int)index;
-(IFURIEntry *)removeEntryAtIndex:(int)index;
-(void)removeEntriesToIndex:(int)index;

@end
