/*	
        IFWebFramePrivate.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebDataSource.h>

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
    id view;
    IFWebDataSource *dataSource;
    IFWebDataSource *provisionalDataSource;
    void *renderFramePart;
    id <IFWebController>controller;
    IFWebFrameState state;
    NSMutableDictionary *errors;
    IFError *mainDocumentError;
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
- (void)setRenderFramePart: (void *)p;
- (void *)renderFramePart;

@end

@interface IFWebFrame (IFPrivate)
- (void)_setRenderFramePart: (void *)p;
- (void *)_renderFramePart;
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
- (void)_initialLayout: userInfo;
@end
