/*	
        WebFramePrivate.h
	    
        Copyright 2001, Apple, Inc. All rights reserved.

        Private header file.
*/

#import <WebKit/WebFrame.h>
#import <WebKit/WebPolicyDelegatePrivate.h>

@class WebBridge;
@class WebFormState;
@class WebFrameBridge;
@class WebFrameView;
@class WebHistoryItem;
@class NSURLRequest;
@class NSMutableURLRequest;
@class WebPolicyDecisionListener;
@class WebView;

@protocol WebDOMElement;

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
    WebFrameLoadTypeSame,		// user loads same URL again (but not reload button)
    WebFrameLoadTypeInternal
} WebFrameLoadType;

#define WebFrameStateChangedNotification @"WebFrameStateChangedNotification"

#define WebPreviousFrameState @"WebPreviousFrameState"
#define WebCurrentFrameState  @"WebCurrentFrameState"

@interface WebFramePrivate : NSObject
{
@public
    NSString *name;
    WebFrameView *webFrameView;
    WebDataSource *dataSource;
    WebDataSource *provisionalDataSource;
    WebBridge *bridge;
    WebView *webView;
    WebFrameState state;
    NSTimer *scheduledLayoutTimer;
    WebFrameLoadType loadType;
    WebFrame *parent;
    NSMutableArray *children;
    WebHistoryItem *currentItem;	// BF item for our current content
    WebHistoryItem *provisionalItem;	// BF item for where we're trying to go
                                        // (only known when navigating to a pre-existing BF item)
    WebHistoryItem *previousItem;	// BF item for previous content, see _itemForSavingDocState

    WebPolicyDecisionListener *listener;
    // state we'll need to continue after waiting for the policy delegate's decision
    NSURLRequest *policyRequest;
    NSString *policyFrameName;
    id policyTarget;
    SEL policySelector;
    WebFormState *policyFormState;

    BOOL justOpenedForTargetedLink;
    BOOL quickRedirectComing;
}

- (void)setName:(NSString *)name;
- (NSString *)name;
- (void)setWebView:(WebView *)wv;
- (WebView *)webView;
- (void)setWebFrameView:(WebFrameView *)v;
- (WebFrameView *)webFrameView;
- (void)setDataSource:(WebDataSource *)d;
- (WebDataSource *)dataSource;
- (void)setProvisionalDataSource:(WebDataSource *)d;
- (WebDataSource *)provisionalDataSource;
- (WebFrameLoadType)loadType;
- (void)setLoadType:(WebFrameLoadType)loadType;

- (void)setProvisionalItem:(WebHistoryItem *)item;
- (WebHistoryItem *)provisionalItem;
- (void)setPreviousItem:(WebHistoryItem *)item;
- (WebHistoryItem *)previousItem;
- (void)setCurrentItem:(WebHistoryItem *)item;
- (WebHistoryItem *)currentItem;

@end

@interface WebFrame (WebPrivate)
- (void)_setWebView:(WebView *)webView;
- (void)_setName:(NSString *)name;
- (WebFrame *)_descendantFrameNamed:(NSString *)name;
- (void)_detachFromParent;
- (void)_closeOldDataSources;
- (void)_setDataSource:(WebDataSource *)d;
- (void)_transitionToCommitted:(NSDictionary *)pageCache;
- (void)_transitionToLayoutAcceptable;
- (WebFrameState)_state;
- (void)_setState:(WebFrameState)newState;
+ (void)_recursiveCheckCompleteFromFrame:(WebFrame *)fromFrame;
- (void)_isLoadComplete;
- (void)_checkLoadComplete;
- (void)_timedLayout:userInfo;
- (WebBridge *)_bridge;
- (void)_clearProvisionalDataSource;
- (void)_setLoadType:(WebFrameLoadType)loadType;
- (WebFrameLoadType)_loadType;

- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request alwaysFromRequest:(BOOL)f;

- (void)_checkNewWindowPolicyForRequest:(NSURLRequest *)request action:(NSDictionary *)action frameName:(NSString *)frameName formState:(WebFormState *)formState andCall:(id)target withSelector:(SEL)selector;

- (void)_checkNavigationPolicyForRequest:(NSURLRequest *)request dataSource:(WebDataSource *)dataSource formState:(WebFormState *)formState andCall:(id)target withSelector:(SEL)selector;

- (void)_invalidatePendingPolicyDecisionCallingDefaultAction:(BOOL)call;

- (void)_goToItem:(WebHistoryItem *)item withLoadType:(WebFrameLoadType)type;
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer loadType:(WebFrameLoadType)loadType target:(NSString *)target triggeringEvent:(NSEvent *)event form:(id <WebDOMElement>)form formValues:(NSDictionary *)values;
- (void)_loadURL:(NSURL *)URL intoChild:(WebFrame *)childFrame;
- (void)_postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSData *)data contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(id <WebDOMElement>)form formValues:(NSDictionary *)values;

- (void)_loadRequest:(NSURLRequest *)request inFrameNamed:(NSString *)frameName;

- (void)_clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory;
- (void)_clientRedirectCancelled;

- (void)_textSizeMultiplierChanged;

- (void)_defersCallbacksChanged;

- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow;
- (void)_viewDidMoveToHostWindow;

- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding;

- (void)_addChild:(WebFrame *)child;
- (void)_removeChild:(WebFrame *)child;

- (NSString *)_generateFrameName;
- (NSDictionary *)_actionInformationForNavigationType:(WebNavigationType)navigationType event:(NSEvent *)event originalURL:(NSURL *)URL;

- (WebHistoryItem *)_itemForSavingDocState;
- (WebHistoryItem *)_itemForRestoringDocState;

- (void)_saveDocumentAndScrollState;

- (void)_handleUnimplementablePolicyWithErrorCode:(int)code forURL:(NSURL *)URL;

- (void)_loadDataSource:(WebDataSource *)dataSource withLoadType:(WebFrameLoadType)type formState:(WebFormState *)formState;

- (void)_setJustOpenedForTargetedLink:(BOOL)justOpened;

- (void)_setProvisionalDataSource:(WebDataSource *)d;

+ (CFAbsoluteTime)_timeOfLastCompletedLoad;
- (BOOL)_canCachePage;
- (void)_purgePageCache;

- (void)_opened;
// used to decide to use loadType=Same
- (BOOL)_shouldTreatURLAsSameAsCurrent:(NSURL *)URL;

- (WebFrame *)_nextFrameWithWrap:(BOOL)wrapFlag;
- (WebFrame *)_previousFrameWithWrap:(BOOL)wrapFlag;

- (void)_setShouldCreateRenderers:(BOOL)f;
- (BOOL)_shouldCreateRenderers;

@end
