/*	
        WebFramePrivate.h
	    
        Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/WebFrame.h>

@class WebBridge;
@class WebFrameBridge;
@class WebHistoryItem;
@class WebPluginController;
@class WebView;
@class WebResourceRequest;

typedef enum {
    WebFrameStateProvisional,
    
    // This state indicates we are ready to commit to a page,
    // which means the view will transition to use the new data source.
    WebFrameStateCommittedPage,
    
    // This state indicates that it is reasonable to perform a layout.
    WebFrameStateLayoutAcceptable,
    
    WebFrameStateComplete
} WebFrameState;

typedef enum {
    WebFrameLoadTypeStandard,
    WebFrameLoadTypeBack,
    WebFrameLoadTypeForward,
    WebFrameLoadTypeIndexedBackForward,		// a multi-item hop in the backforward list
    WebFrameLoadTypeReload,
    WebFrameLoadTypeReloadAllowingStaleData,
    WebFrameLoadTypeClientRedirect,
    WebFrameLoadTypeInternal
} WebFrameLoadType;

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
    WebBridge *bridge;
    WebController *controller;
    WebFrameState state;
    NSTimer *scheduledLayoutTimer;
    WebFrameLoadType loadType;
    WebFrame *parent;
    NSMutableArray *children;
    WebPluginController *pluginController;
}

- (void)setName:(NSString *)name;
- (NSString *)name;
- (void)setController:(WebController *)c;
- (WebController *)controller;
- (void)setWebView:(WebView *)v;
- (WebView *)webView;
- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;
- (void)setProvisionalDataSource:(WebDataSource *)d;
- (WebDataSource *)provisionalDataSource;
- (WebFrameLoadType)loadType;
- (void)setLoadType:(WebFrameLoadType)loadType;

@end

@interface WebFrame (WebPrivate)
- (WebFrame *)_descendantFrameNamed:(NSString *)name;
- (void)_controllerWillBeDeallocated;
- (void)_detachFromParent;
- (void)_setController: (WebController *)controller;
- (void)_setDataSource: (WebDataSource *)d;
- (void)_transitionToCommitted;
- (void)_transitionToLayoutAcceptable;
- (WebFrameState)_state;
- (void)_setState: (WebFrameState)newState;
+ (void)_recursiveCheckCompleteFromFrame: (WebFrame *)fromFrame;
- (void)_isLoadComplete;
- (void)_checkLoadComplete;
- (void)_timedLayout: userInfo;
- (WebBridge *)_bridge;
- (BOOL)_shouldShowRequest:(WebResourceRequest *)request;
- (void)_setProvisionalDataSource:(WebDataSource *)d;
- (void)_setLoadType: (WebFrameLoadType)loadType;
- (WebFrameLoadType)_loadType;
- (void)_goToItem: (WebHistoryItem *)item withFrameLoadType: (WebFrameLoadType)type;
- (void)_restoreScrollPosition;
- (void)_scrollToTop;
- (void)_textSizeMultiplierChanged;

- (void)_defersCallbacksChanged;

- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding;

- (void)_addChild:(WebFrame *)child;

- (WebPluginController *)_pluginController;

@end
