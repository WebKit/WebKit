/*	WKBackForwardList.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <Foundation/Foundation.h>
#import <WebKit/WKURIList.h>
#import <WebKit/WKURIEntry.h>

@interface WKBackForwardList : NSObject {
    WKURIList *uriList;
    int index;
    NSLock *mutex;
    int state;
}

-(id)init;

-(void)addEntry:(WKURIEntry *)entry;
-(WKURIEntry *)back;
-(WKURIEntry *)forward;

-(NSArray *)backList;
-(NSArray *)forwardList;

-(BOOL)canGoBack;
-(BOOL)canGoForward;

@end


#ifdef NEW_WEBKIT_API

//=============================================================================
//
// WKBackForwardList.h
//
// It provides the list that enables the "Back" and "Forward" buttons to
// work correctly. As such, it is merely a user convenience that aids in
// basic navigation in ways that users have come to expect.
//

@interface WKBackForwardList
{

-(id)init;

-(void)addAttributedURL:(WKAttributedURL *)url;

-(WKAttributedURL *)back;
-(WKAttributedURL *)forward;

-(NSArray *)backList;
-(NSArray *)forwardList;

-(BOOL)canGoBack;
-(BOOL)canGoForward;

}

//=============================================================================

#endif // NEW_WEBKIT_API
