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

// This header contains WebFrame declarations that can be used anywhere in WebKit, but are neither SPI nor API.

#import <WebKit/WebFramePrivate.h>
#import "WebPolicyDelegatePrivate.h"

@class WebDocumentLoader;
@class WebInspector;
@class WebFrameLoader;
@class WebFrameView;
@class WebFrameBridge;
@class WebFormState;

// One day we might want to expand the use of this kind of class such that we'd receive one
// over the bridge, and possibly hand it on through to the FormsDelegate.
// Today it is just used internally to keep some state as we make our way through a bunch
// layers while doing a load.
@interface WebFormState : NSObject
{
    DOMElement *_form;
    NSDictionary *_values;
    WebFrame *_sourceFrame;
}
- (id)initWithForm:(DOMElement *)form values:(NSDictionary *)values sourceFrame:(WebFrame *)sourceFrame;
- (DOMElement *)form;
- (NSDictionary *)values;
- (WebFrame *)sourceFrame;
@end

@interface WebFrame (WebInternal)

- (void)_updateBackground;
- (void)_setInternalLoadDelegate:(id)internalLoadDelegate;
- (id)_internalLoadDelegate;
- (void)_unmarkAllMisspellings;
// Note that callers should not perform any ops on these views that could change the set of frames
- (NSArray *)_documentViews;

- (NSURLRequest *)_requestFromDelegateForRequest:(NSURLRequest *)request identifier:(id *)identifier error:(NSError **)error;
- (void)_sendRemainingDelegateMessagesWithIdentifier:(id)identifier response:(NSURLResponse *)response length:(unsigned)length error:(NSError *)error;
- (void)_safeLoadURL:(NSURL *)URL;

- (BOOL)_hasSelection;
- (void)_clearSelection;
- (WebFrame *)_findFrameWithSelection;
- (void)_clearSelectionInOtherFrames;
- (BOOL)_subframeIsLoading;
- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v bridge:(WebFrameBridge *)bridge;

- (void)_addPlugInView:(NSView *)plugInView;
- (void)_removeAllPlugInViews;

// This should be called when leaving a page or closing the WebView
- (void)_willCloseURL;

- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request mainResource:(BOOL)mainResource alwaysFromRequest:(BOOL)f;
- (BOOL)_isMainFrame;

- (void)_addInspector:(WebInspector *)inspector;
- (void)_removeInspector:(WebInspector *)inspector;

- (WebFrameLoader *)_frameLoader;
- (void)_prepareForDataSourceReplacement;
- (void)_frameLoadCompleted;
- (WebDataSource *)_dataSourceForDocumentLoader:(WebDocumentLoader *)loader;
- (WebDocumentLoader *)_createDocumentLoaderWithRequest:(NSURLRequest *)request;
- (void)_didReceiveServerRedirectForProvisionalLoadForFrame;

- (NSURLRequest *)_webDataRequestForData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;

- (void)_detachFromParent;
- (void)_detachChildren;
- (void)_checkLoadCompleteForThisFrame;
- (void)_handledOnloadEvents;
- (void)_checkLoadComplete;
- (WebFrameBridge *)_bridge;

- (void)_goToItem:(WebHistoryItem *)item withLoadType:(WebFrameLoadType)type;
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame;
- (void)_postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target data:(NSArray *)postData contentType:(NSString *)contentType triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values;

- (void)_loadRequest:(NSURLRequest *)request inFrameNamed:(NSString *)frameName;


- (void)_defersCallbacksChanged;

- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow;
- (void)_viewDidMoveToHostWindow;

- (void)_addChild:(WebFrame *)child;

- (NSDictionary *)_actionInformationForNavigationType:(WebNavigationType)navigationType event:(NSEvent *)event originalURL:(NSURL *)URL;

- (WebHistoryItem *)_itemForSavingDocState;
- (WebHistoryItem *)_itemForRestoringDocState;

- (void)_saveDocumentAndScrollState;

- (void)_setTitle:(NSString *)title;

+ (CFAbsoluteTime)_timeOfLastCompletedLoad;
- (BOOL)_canCachePage;
- (void)_purgePageCache;

- (void)_opened;
// used to decide to use loadType=Same
- (BOOL)_shouldTreatURLAsSameAsCurrent:(NSURL *)URL;

- (WebFrame *)_nextFrameWithWrap:(BOOL)wrapFlag;
- (WebFrame *)_previousFrameWithWrap:(BOOL)wrapFlag;


- (BOOL)_shouldCreateRenderers;

- (int)_numPendingOrLoadingRequests:(BOOL)recurse;

- (void)_reloadForPluginChanges;

- (void)_attachScriptDebugger;
- (void)_detachScriptDebugger;

- (void)_recursive_pauseNullEventsForAllNetscapePlugins;
- (void)_recursive_resumeNullEventsForAllNetscapePlugins;

- (void)_restoreScrollPositionAndViewState;
- (void)_transitionToCommitted:(NSDictionary *)pageCache;

- (void)_provisionalLoadStarted;
-(NSDictionary *)_actionInformationForLoadType:(WebFrameLoadType)loadType isFormSubmission:(BOOL)isFormSubmission event:(NSEvent *)event originalURL:(NSURL *)URL;
- (void)_addHistoryItemForFragmentScroll;
- (void)_didFinishLoad;

@end

@interface NSObject (WebInternalFrameLoadDelegate)
- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error;
@end
