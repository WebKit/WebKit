/*	
        WebFramePrivate.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/WebFrame.h>

@class WebFrameBridge;
@class WebView;
@protocol WebDocumentLoading;

typedef enum {
    WebFrameStateUninitialized = 1,
    WebFrameStateProvisional = 2,
    
    // This state indicates we are ready to commit to a page,
    // that means the view will transition to use the new
    // datasource.
    WebFrameStateCommittedPage = 3,
    
    // This state indicates that it is reasonable to perform
    // a layout.
    WebFrameStateLayoutAcceptable = 4,
    
    WebFrameStateComplete = 5
} WebFrameState;

#define WebFrameStateChangedNotification @"WebFrameStateChangedNotification"

#define WebPreviousFrameState @"WebPreviousFrameState"
#define WebCurrentFrameState  @"WebCurrentFrameState"

@interface WebFramePrivate : NSObject
{
@public
    NSString *name;
    WebView *webView;
    WebDataSource *dataSource;
    WebDataSource *provisionalDataSource;
    WebController *controller;
    WebFrameState state;
    BOOL scheduledLayoutPending;
    WebFrameBridge *frameBridge;
}

- (void)setName: (NSString *)n;
- (NSString *)name;
- (void)setController: (WebController *)c;
- (WebController *)controller;
- (void)setWebView: (WebView *)v;
- (WebView *)webView;
- (void)setDataSource: (WebDataSource *)d;
- (WebDataSource *)dataSource;
- (void)setProvisionalDataSource: (WebDataSource *)d;
- (WebDataSource *)provisionalDataSource;

@end

@interface WebFrame (WebPrivate)
- (void)_parentDataSourceWillBeDeallocated;
- (void)_setController: (WebController *)controller;
- (void)_setDataSource: (WebDataSource *)d;
- (void)_transitionProvisionalToCommitted;
- (void)_transitionProvisionalToLayoutAcceptable;
- (WebFrameState)_state;
- (void)_setState: (WebFrameState)newState;
+ (void)_recursiveCheckCompleteFromFrame: (WebFrame *)fromFrame;
- (void)_isLoadComplete;
- (void)_checkLoadComplete;
- (void)_timedLayout: userInfo;
- (WebFrameBridge *)_frameBridge;
- (BOOL)_shouldShowDataSource:(WebDataSource *)dataSource;
- (void)_setProvisionalDataSource:(WebDataSource *)d;
@end
