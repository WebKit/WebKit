/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This header contains the WebFrame SPI.

// But it also contains a bunch of internal stuff that should be moved to WebFrameInternal.h

#import <WebKit/WebFrame.h>
#import <WebKit/WebPolicyDelegatePrivate.h>

@class DOMDocument;
@class DOMElement;
@class DOMNode;
@class NSMutableURLRequest;
@class NSURLRequest;
@class WebArchive;
@class WebBridge;
@class WebFormState;
@class WebFrameBridge;
@class WebFrameView;
@class WebHistoryItem;
@class WebPolicyDecisionListener;
@class WebView;

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

// Keys for accessing the values in the page cache dictionary.
extern NSString *WebPageCacheEntryDateKey;
extern NSString *WebPageCacheDataSourceKey;
extern NSString *WebPageCacheDocumentViewKey;

@interface WebFramePrivate : NSObject
{
@public
    WebFrame *nextSibling;
    WebFrame *previousSibling;

    NSString *name;
    WebFrameView *webFrameView;
    WebDataSource *dataSource;
    WebDataSource *provisionalDataSource;
    WebBridge *bridge;
    WebView *webView;
    WebFrameState state;
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
    WebDataSource *policyDataSource;
    WebFrameLoadType policyLoadType;

    BOOL justOpenedForTargetedLink;
    BOOL quickRedirectComing;
    BOOL isStoppingLoad;
    BOOL delegateIsHandlingProvisionalLoadError;
    BOOL delegateIsDecidingNavigationPolicy;
    BOOL delegateIsHandlingUnimplementablePolicy;
    
    id internalLoadDelegate;
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

// Other private methods
- (NSURLRequest *)_webDataRequestForData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;
- (void)_loadRequest:(NSURLRequest *)request subresources:(NSArray *)subresources subframeArchives:(NSArray *)subframeArchives;

- (void)_setWebView:(WebView *)webView;
- (void)_setName:(NSString *)name;
- (WebFrame *)_descendantFrameNamed:(NSString *)name sourceFrame:(WebFrame *)source;
- (void)_detachFromParent;
- (void)_closeOldDataSources;
- (void)_setDataSource:(WebDataSource *)d;
- (void)_transitionToCommitted:(NSDictionary *)pageCache;
- (void)_transitionToLayoutAcceptable;
- (WebFrameState)_state;
- (void)_setState:(WebFrameState)newState;
- (void)_checkLoadCompleteForThisFrame;
- (void)_checkLoadComplete;
- (WebBridge *)_bridge;
- (void)_clearProvisionalDataSource;
- (void)_setLoadType:(WebFrameLoadType)loadType;
- (WebFrameLoadType)_loadType;

- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request alwaysFromRequest:(BOOL)f;

- (void)_checkNewWindowPolicyForRequest:(NSURLRequest *)request action:(NSDictionary *)action frameName:(NSString *)frameName formState:(WebFormState *)formState andCall:(id)target withSelector:(SEL)selector;

- (void)_checkNavigationPolicyForRequest:(NSURLRequest *)request dataSource:(WebDataSource *)dataSource formState:(WebFormState *)formState andCall:(id)target withSelector:(SEL)selector;

- (void)_invalidatePendingPolicyDecisionCallingDefaultAction:(BOOL)call;

- (void)_goToItem:(WebHistoryItem *)item withLoadType:(WebFrameLoadType)type;
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer loadType:(WebFrameLoadType)loadType target:(NSString *)target triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values;
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame;
- (void)_postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSArray *)postData contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values;

- (void)_loadRequest:(NSURLRequest *)request inFrameNamed:(NSString *)frameName;

- (void)_clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory isJavaScriptFormAction:(BOOL)isJavaScriptFormAction;
- (void)_clientRedirectCancelled:(BOOL)cancelWithLoadInProgress;

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
- (void)_receivedMainResourceError:(NSError *)error;

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

- (int)_numPendingOrLoadingRequests:(BOOL)recurse;

- (NSColor *)_bodyBackgroundColor;

- (void)_reloadForPluginChanges;

- (NSArray *)_internalChildFrames;

- (BOOL)_isDescendantOfFrame:(WebFrame *)frame;
- (BOOL)_isFrameSet;

@end
