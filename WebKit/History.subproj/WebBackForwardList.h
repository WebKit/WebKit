/*	WebBackForwardList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>

@class WebHistoryList;
@class WebHistoryItem;

@interface WebBackForwardList : NSObject {
    WebHistoryList *uriList;
    int index;
    int state;
}

-(id)init;

// add to the list
-(void)addEntry:(WebHistoryItem *)entry;

// change position in the list
-(void)goBack;
-(void)goBackToIndex: (int)pos;
-(void)goForward;
-(void)goForwardToIndex: (int)pos;

// examine entries without changing position
-(WebHistoryItem *)backEntry;
-(WebHistoryItem *)currentEntry;
-(WebHistoryItem *)forwardEntry;

// examine entire list
-(NSArray *)backList;
-(NSArray *)forwardList;

// check whether entries exist
-(BOOL)canGoBack;
-(BOOL)canGoForward;

@end