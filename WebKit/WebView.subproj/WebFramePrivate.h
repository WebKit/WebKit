/*	
        IFWebFramePrivate.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/IFWebFrame.h>

@class IFWebView;

namespace khtml {
    class RenderPart;
}

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

@interface IFWebFramePrivate : NSObject
{
    NSString *name;
    IFWebView *view;
    IFWebDataSource *dataSource;
    IFWebDataSource *provisionalDataSource;
    khtml::RenderPart *renderFramePart;
    id <IFWebController>controller;
    IFWebFrameState state;
    NSMutableDictionary *errors;
    IFError *mainDocumentError;
    BOOL scheduledLayoutPending;
}

- (void)setName: (NSString *)n;
- (NSString *)name;
- (void)setController: (id <IFWebController>)c;
- (id <IFWebController>)controller;
- (void)setView: v;
- view;
- (void)setDataSource: (IFWebDataSource *)d;
- (IFWebDataSource *)dataSource;
- (void)setProvisionalDataSource: (IFWebDataSource *)d;
- (IFWebDataSource *)provisionalDataSource;
- (void)setRenderFramePart: (khtml::RenderPart *)p;
- (khtml::RenderPart *)renderFramePart;

@end

@interface IFWebFrame (IFPrivate)
- (void)_setController: (id <IFWebController>)controller;
- (void)_setRenderFramePart: (khtml::RenderPart *)p;
- (khtml::RenderPart *)_renderFramePart;
- (void)_setDataSource: (IFWebDataSource *)d;
- (void)_transitionProvisionalToCommitted;
- (void)_transitionProvisionalToLayoutAcceptable;
- (IFWebFrameState)_state;
- (void)_setState: (IFWebFrameState)newState;
+ (void)_recursiveCheckCompleteFromFrame: (IFWebFrame *)fromFrame;
- (void)_isLoadComplete;
- (void)_checkLoadCompleteResource: (NSString *)resourceDescription error: (IFError *)error isMainDocument: (BOOL)flag;
- (void)_clearErrors;
- (void)_setMainDocumentError: (IFError *)error;
- (void)_timedLayout: userInfo;
@end
