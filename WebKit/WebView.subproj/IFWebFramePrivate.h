/*	
        IFWebFramePrivate.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/IFWebFrame.h>

@class IFWebCoreFrame;
@class IFWebView;
@protocol IFDocumentLoading;

typedef enum {
    IFWEBFRAMESTATE_UNINITIALIZED = 1,
    IFWEBFRAMESTATE_PROVISIONAL = 2,
    
    // This state indicates we are ready to commit to a page,
    // that means the view will transition to use the new
    // datasource.
    IFWEBFRAMESTATE_COMMITTED_PAGE = 3,
    
    // This state indicates that it is reasonable to perform
    // a layout.
    IFWEBFRAMESTATE_LAYOUT_ACCEPTABLE = 4,
    
    IFWEBFRAMESTATE_COMPLETE = 5
} IFWebFrameState;

#define IFFrameStateChangedNotification @"IFFrameStateChangedNotification"

#define IFPreviousFrameState @"IFPreviousFrameState"
#define IFCurrentFrameState  @"IFCurrentFrameState"

@interface IFWebFramePrivate : NSObject
{
    NSString *name;
    IFWebView *webView;
    IFWebDataSource *dataSource;
    IFWebDataSource *provisionalDataSource;
    IFWebController *controller;
    IFWebFrameState state;
    BOOL scheduledLayoutPending;
    IFWebCoreFrame *frameBridge;
}

- (void)setName: (NSString *)n;
- (NSString *)name;
- (void)setController: (IFWebController *)c;
- (IFWebController *)controller;
- (void)setWebView: (IFWebView *)v;
- (IFWebView *)webView;
- (void)setDataSource: (IFWebDataSource *)d;
- (IFWebDataSource *)dataSource;
- (void)setProvisionalDataSource: (IFWebDataSource *)d;
- (IFWebDataSource *)provisionalDataSource;

@end

@interface IFWebFrame (IFPrivate)
- (void)_parentDataSourceWillBeDeallocated;
- (void)_setController: (IFWebController *)controller;
- (void)_setDataSource: (IFWebDataSource *)d;
- (void)_transitionProvisionalToCommitted;
- (void)_transitionProvisionalToLayoutAcceptable;
- (IFWebFrameState)_state;
- (void)_setState: (IFWebFrameState)newState;
+ (void)_recursiveCheckCompleteFromFrame: (IFWebFrame *)fromFrame;
- (void)_isLoadComplete;
- (void)_checkLoadComplete;
- (void)_timedLayout: userInfo;
- (IFWebCoreFrame *)_frameBridge;
- (BOOL)_shouldShowDataSource:(IFWebDataSource *)dataSource;
@end
