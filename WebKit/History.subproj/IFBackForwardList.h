/*	IFBackForwardList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import <WebKit/IFURIList.h>
#import <WebKit/IFURIEntry.h>

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


#ifdef NEW_WEBKIT_API

//=============================================================================
//
// IFBackForwardList.h
//
// It provides the list that enables the "Back" and "Forward" buttons to
// work correctly. As such, it is merely a user convenience that aids in
// basic navigation in ways that users have come to expect.
//

@interface IFBackForwardList
{

-(id)init;

-(void)addAttributedURL:(IFAttributedURL *)url;

-(IFAttributedURL *)back;
-(IFAttributedURL *)forward;

-(NSArray *)backList;
-(NSArray *)forwardList;

-(BOOL)canGoBack;
-(BOOL)canGoForward;

}

//=============================================================================

#endif // NEW_WEBKIT_API
