/*	IFBackForwardList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class IFURIList;
@class IFURIEntry;

@interface IFBackForwardList : NSObject {
    IFURIList *uriList;
    int index;
    NSLock *mutex;
    int state;
}

-(id)init;

// add to the list
-(void)addEntry:(IFURIEntry *)entry;

// change position in the list
-(void)goBack;
-(void)goForward;

// examine entries without changing position
-(IFURIEntry *)backEntry;
-(IFURIEntry *)currentEntry;
-(IFURIEntry *)forwardEntry;

// examine entire list
-(NSArray *)backList;
-(NSArray *)forwardList;

// check whether entries exist
-(BOOL)canGoBack;
-(BOOL)canGoForward;

@end