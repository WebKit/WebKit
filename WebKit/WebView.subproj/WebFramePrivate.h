/*	
        WebFramePrivate.h
	    
        Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/WebFrame.h>
#import <WebKit/WebControllerPolicyDelegate.h>

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

    // We've decided we're complete, and khtml is ending the load
    WebFrameStateCompleting,

    WebFrameStateComplete
} WebFrameState;

typedef enum {
    WebFrameLoadTypeStandard,
    WebFrameLoadTypeBack,
    WebFrameLoadTypeForward,
    WebFrameLoadTypeIndexedBackForward,		// a multi-item hop in the backforward list
    WebFrameLoadTypeReload,
    WebFrameLoadTypeReloadAllowingStaleData,
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
    WebHistoryItem *currentItem;	// BF item for our current content
    WebHistoryItem *provisionalItem;	// BF item for where we're trying to go
                                        // (only known when navigating to a pre-existing BF item)
    WebHistoryItem *previousItem;	// BF item for previous content, see _itemForSavingDocState
    BOOL instantRedirectComing;
    BOOL shortRedirectComing;

    WebPolicyDecisionListener *listener;
    WebResourceRequest *policyRequest;
    id policyTarget;
    SEL policySelector;
    BOOL justOpenedForTargetedLink;
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

- (void)setProvisionalItem: (WebHistoryItem *)item;
- (WebHistoryItem *)provisionalItem;
- (void)setPreviousItem:(WebHistoryItem *)item;
- (WebHistoryItem *)previousItem;
- (void)setCurrentItem:(WebHistoryItem *)item;
- (WebHistoryItem *)currentItem;

@end

@interface WebFrame (WebPrivate)
- (WebFrame *)_descendantFrameNamed:(NSString *)name;
- (void)_controllerWillBeDeallocated;
- (void)_detachFromParent;
- (void)_setController: (WebController *)controller;
- (void)_setDataSource: (WebDataSource *)d;
- (void)_transitionToCommitted: (NSDictionary *)pageCache;
- (void)_transitionToLayoutAcceptable;
- (WebFrameState)_state;
- (void)_setState: (WebFrameState)newState;
+ (void)_recursiveCheckCompleteFromFrame: (WebFrame *)fromFrame;
- (void)_isLoadComplete;
- (void)_checkLoadComplete;
- (void)_timedLayout: userInfo;
- (WebBridge *)_bridge;
- (void)_clearProvisionalDataSource;
- (void)_setLoadType: (WebFrameLoadType)loadType;
- (WebFrameLoadType)_loadType;

- (void)_addExtraFieldsToRequest:(WebResourceRequest *)request;

- (void)_checkNavigationPolicyForRequest:(WebResourceRequest *)request dataSource:(WebDataSource *)dataSource andCall:(id)target withSelector:(SEL)selector;

- (void)_invalidatePendingPolicyDecisionCallingDefaultAction:(BOOL)call;

- (NSDictionary *)_actionInformationForNavigationType:(WebNavigationType)navigationType event:(NSEvent *)event originalURL:(NSURL *)URL;
- (void)_goToItem: (WebHistoryItem *)item withLoadType: (WebFrameLoadType)type;
- (void)_loadURL:(NSURL *)URL loadType:(WebFrameLoadType)loadType triggeringEvent:(NSEvent *)event isFormSubmission:(BOOL)isFormSubmission;
- (void)_loadURL:(NSURL *)URL intoChild:(WebFrame *)childFrame;
- (void)_postWithURL:(NSURL *)URL data:(NSData *)data contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event;

- (void)_clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date;
- (void)_clientRedirectCancelled;

- (void)_saveScrollPositionToItem:(WebHistoryItem *)item;
- (void)_restoreScrollPosition;
- (void)_scrollToTop;
- (void)_textSizeMultiplierChanged;

- (void)_defersCallbacksChanged;

- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding;

- (void)_addChild:(WebFrame *)child;

- (NSString *)_generateFrameName;

- (WebPluginController *)_pluginController;

- (WebHistoryItem *)_createItemTreeWithTargetFrame:(WebFrame *)targetFrame clippedAtTarget:(BOOL)doClip;

- (WebHistoryItem *)_itemForSavingDocState;
- (WebHistoryItem *)_itemForRestoringDocState;
- (void)_handleUnimplementablePolicy:(WebPolicyAction)policy errorCode:(int)code forURL:(NSURL *)URL;

- (void)_loadDataSource:(WebDataSource *)dataSource withLoadType:(WebFrameLoadType)type;

- (void)_downloadRequest:(WebResourceRequest *)request toPath:(NSString *)path;

- (void)_setJustOpenedForTargetedLink:(BOOL)justOpened;

- (void)_setProvisionalDataSource: (WebDataSource *)d;

- (BOOL)_canCachePage;
- (void)_purgePageCache;

@end
