/*	WKBackForwardList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import "WCURIEntry.h"

@protocol WCBackForwardList

-(void)addEntry:(id <WCURIEntry>)entry;
-(id <WCURIEntry>)back;
-(id <WCURIEntry>)forward;

-(NSArray *)backList;
-(NSArray *)forwardList;

-(BOOL)canGoBack;
-(BOOL)canGoForward;

@end

#if defined(__cplusplus)
extern "C" {
#endif

// *** Factory method for WCBackForwardList objects

id <WCBackForwardList> WCCreateBackForwardList(void); 

#if defined(__cplusplus)
} // extern "C"
#endif
