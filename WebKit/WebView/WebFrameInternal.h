/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "WebFramePrivate.h"
#import "WebPolicyDelegatePrivate.h"

#ifdef __cplusplus
#import <WebCore/FrameLoaderTypes.h>
#import <wtf/Forward.h>
#endif

@class DOMRange;
@class WebInspector;
@class WebFrameView;
@class WebFrameBridge;
@class WebPolicyDecider;

#ifdef __cplusplus

namespace WebCore {
    class Document;
    class DocumentLoader;
    class Element;
    class Frame;
    class FrameMac;
    class FrameLoader;
    class HTMLElement;
    class Range;
    class WebResourceLoader;
    struct LoadErrorResetToken;
}

WebCore::FrameMac* core(WebFrame *);
WebFrame *kit(WebCore::Frame *);

WebCore::Element* core(DOMElement *);
DOMElement *kit(WebCore::Element*);

WebCore::Document* core(DOMDocument *);
DOMDocument *kit(WebCore::Document*);

WebCore::HTMLElement* core(DOMHTMLElement *);
DOMHTMLElement *kit(WebCore::HTMLElement*);

WebCore::Range* core(DOMRange *);
DOMRange *kit(WebCore::Range*);

#endif

@interface WebFrame (WebInternal)

- (void)_updateBackground;
- (void)_setInternalLoadDelegate:(id)internalLoadDelegate;
- (id)_internalLoadDelegate;
#if !BUILDING_ON_TIGER
- (void)_unmarkAllBadGrammar;
#endif
- (void)_unmarkAllMisspellings;
// Note that callers should not perform any ops on these views that could change the set of frames
- (NSArray *)_documentViews;

- (BOOL)_hasSelection;
- (void)_clearSelection;
- (WebFrame *)_findFrameWithSelection;
- (void)_clearSelectionInOtherFrames;
#ifdef __cplusplus
- (id)_initWithWebFrameView:(WebFrameView *)fv webView:(WebView *)v coreFrame:(WebCore::Frame*)coreFrame;
#endif

- (void)_addPlugInView:(NSView *)plugInView;
- (void)_removeAllPlugInViews;

// This should be called when leaving a page or closing the WebView
- (void)_willCloseURL;

- (BOOL)_isMainFrame;

- (void)_addInspector:(WebInspector *)inspector;
- (void)_removeInspector:(WebInspector *)inspector;

#ifdef __cplusplus

- (WebCore::FrameLoader*)_frameLoader;
- (WebDataSource *)_dataSourceForDocumentLoader:(WebCore::DocumentLoader*)loader;

- (void)_addDocumentLoader:(WebCore::DocumentLoader*)loader toUnarchiveState:(WebArchive *)archive;

#endif

- (NSURLRequest *)_webDataRequestForData:(NSData *)data MIMEType:(NSString *)MIMEType textEncodingName:(NSString *)encodingName baseURL:(NSURL *)URL unreachableURL:(NSURL *)unreachableURL;

- (WebFrameBridge *)_bridge;

#ifdef __cplusplus
- (void)_goToItem:(WebHistoryItem *)item withLoadType:(WebCore::FrameLoadType)type;
#endif
- (void)_loadURL:(NSURL *)URL referrer:(NSString *)referrer intoChild:(WebFrame *)childFrame;

- (void)_viewWillMoveToHostWindow:(NSWindow *)hostWindow;
- (void)_viewDidMoveToHostWindow;

- (void)_addChild:(WebFrame *)child;

- (WebHistoryItem *)_itemForSavingDocState;
- (WebHistoryItem *)_itemForRestoringDocState;

- (void)_saveDocumentAndScrollState;

+ (CFAbsoluteTime)_timeOfLastCompletedLoad;
- (BOOL)_canCachePage;
- (void)_purgePageCache;

- (int)_numPendingOrLoadingRequests:(BOOL)recurse;

- (void)_reloadForPluginChanges;

- (void)_attachScriptDebugger;
- (void)_detachScriptDebugger;

- (void)_recursive_pauseNullEventsForAllNetscapePlugins;
- (void)_recursive_resumeNullEventsForAllNetscapePlugins;

@end

#ifdef __cplusplus

@interface WebFrame (WebFrameLoaderClient)

- (BOOL)_hasBackForwardList;
- (void)_resetBackForwardList;
- (void)_invalidateCurrentItemPageCache;
- (BOOL)_provisionalItemIsTarget;
- (BOOL)_loadProvisionalItemFromPageCache;
- (BOOL)_privateBrowsingEnabled;
- (void)_makeDocumentView;
- (void)_forceLayout;
- (void)_updateHistoryForCommit;
- (void)_updateHistoryForReload;
- (void)_updateHistoryForStandardLoad;
- (void)_updateHistoryForBackForwardNavigation;
- (void)_updateHistoryForInternalLoad;
- (WebCore::LoadErrorResetToken*)_tokenForLoadErrorReset;
- (void)_resetAfterLoadError:(WebCore::LoadErrorResetToken*)token;
- (void)_doNotResetAfterLoadError:(WebCore::LoadErrorResetToken*)token;
- (void)_dispatchDidHandleOnloadEventsForFrame;
- (void)_dispatchDidReceiveServerRedirectForProvisionalLoadForFrame;
- (id)_dispatchIdentifierForInitialRequest:(NSURLRequest *)clientRequest fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (NSURLRequest *)_dispatchResource:(id)identifier willSendRequest:(NSURLRequest *)clientRequest redirectResponse:(NSURLResponse *)redirectResponse fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchDidReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchDidCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didReceiveResponse:(NSURLResponse *)r fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didReceiveContentLength:(int)lengthReceived fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didFinishLoadingFromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didFailLoadingWithError:error fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchDidCancelClientRedirectForFrame;
- (void)_dispatchWillPerformClientRedirectToURL:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date;
- (void)_dispatchDidChangeLocationWithinPageForFrame;
- (void)_dispatchWillCloseFrame;
- (void)_dispatchDidReceiveIcon:(NSImage *)icon;
- (void)_dispatchDidStartProvisionalLoadForFrame;
- (void)_dispatchDidReceiveTitle:(NSString *)title;
- (void)_dispatchDidCommitLoadForFrame;
- (void)_dispatchDidFailProvisionalLoadWithError:(NSError *)error;
- (void)_dispatchDidFailLoadWithError:(NSError *)error;
- (void)_dispatchDidFinishLoadForFrame;
- (void)_dispatchDidFirstLayoutInFrame;
- (WebCore::Frame*)_dispatchCreateWebViewWithRequest:(NSURLRequest *)request;
- (void)_dispatchShow;
- (WebPolicyDecider *)_createPolicyDeciderWithTarget:(id)target action:(SEL)action;
- (void)_dispatchDecidePolicyForMIMEType:(NSString *)MIMEType request:(NSURLRequest *)request decider:(WebPolicyDecider *)decider;
- (void)_dispatchDecidePolicyForNewWindowAction:(NSDictionary *)action request:(NSURLRequest *)request newFrameName:(NSString *)frameName decider:(WebPolicyDecider *)decider;
- (void)_dispatchDecidePolicyForNavigationAction:(NSDictionary *)action request:(NSURLRequest *)request decider:(WebPolicyDecider *)decider;
- (void)_dispatchUnableToImplementPolicyWithError:(NSError *)error;
- (void)_dispatchSourceFrame:(WebCore::Frame*)sourceFrame willSubmitForm:(WebCore::Element*)form withValues:(NSDictionary *)values submissionDecider:(WebPolicyDecider *)decider;
- (void)_detachedFromParent1;
- (void)_detachedFromParent2;
- (void)_detachedFromParent3;
- (void)_detachedFromParent4;
- (void)_updateHistoryAfterClientRedirect;
- (void)_loadedFromPageCache;
- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)response proxy:(id)proxy;
- (void)_setDocumentViewFromPageCache:(NSDictionary *)pageCache;
- (void)_setCopiesOnScroll;
- (void)_dispatchDidLoadMainResourceForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_forceLayoutForNonHTML;
- (void)_clearLoadingFromPageCacheForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (BOOL)_isDocumentLoaderLoadingFromPageCache:(WebCore::DocumentLoader*)loader;
- (void)_makeRepresentationForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_revertToProvisionalStateForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_setMainDocumentError:(NSError *)error forDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_clearUnarchivingStateForLoader:(WebCore::DocumentLoader*)loader;
- (void)_progressStarted;
- (void)_progressCompleted;
- (void)_incrementProgressForIdentifier:(id)identifier response:(NSURLResponse *)response;
- (void)_incrementProgressForIdentifier:(id)identifier data:(NSData *)data;
- (void)_completeProgressForIdentifier:(id)identifier;
- (void)_setMainFrameDocumentReady:(BOOL)ready;
- (void)_willChangeTitleForDocument:(WebCore::DocumentLoader*)loader;
- (void)_didChangeTitleForDocument:(WebCore::DocumentLoader*)loader;
- (void)_startDownloadWithRequest:(NSURLRequest *)request;
- (void)_finishedLoadingDocument:(WebCore::DocumentLoader*)loader;
- (void)_committedLoadWithDocumentLoader:(WebCore::DocumentLoader*)loader data:(NSData *)data;
- (void)_documentLoader:(WebCore::DocumentLoader*)loader setMainDocumentError:(NSError *)error;
- (void)_finalSetupForReplaceWithDocumentLoader:(WebCore::DocumentLoader*)loader;
- (NSError *)_cancelledErrorWithRequest:(NSURLRequest *)request;
- (NSError *)_cannotShowURLErrorWithRequest:(NSURLRequest *)request;
- (NSError *)_interruptForPolicyChangeErrorWithRequest:(NSURLRequest *)request;
- (NSError *)_cannotShowMIMETypeErrorWithResponse:(NSURLResponse *)response;
- (NSError *)_fileDoesNotExistErrorWithResponse:(NSURLResponse *)response;
- (BOOL)_shouldFallBackForError:(NSError *)error;
- (BOOL)_hasWebView;
- (BOOL)_hasFrameView;
- (NSURL *)_mainFrameURL;
- (BOOL)_canUseResourceForRequest:(NSURLRequest *)request;
- (BOOL)_canUseResourceWithResponse:(NSURLResponse *)response;
- (void)_deliverArchivedResourcesAfterDelay;
- (BOOL)_willUseArchiveForRequest:(NSURLRequest *)r originalURL:(NSURL *)originalURL loader:(WebCore::WebResourceLoader*)loader;
- (BOOL)_archiveLoadPendingForLoader:(WebCore::WebResourceLoader*)loader;
- (void)_cancelPendingArchiveLoadForLoader:(WebCore::WebResourceLoader*)loader;
- (void)_clearArchivedResources;
- (void)_deliverArchivedResources;
- (void)_setDefersCallbacks:(BOOL)defers;
- (BOOL)_canHandleRequest:(NSURLRequest *)request;
- (BOOL)_canShowMIMEType:(NSString *)MIMEType;
- (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme;
- (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme;
- (NSDictionary *)_elementForEvent:(NSEvent *)event;
- (void)_frameLoadCompleted;
- (void)_restoreScrollPositionAndViewState;
- (void)_setTitle:(NSString *)title forURL:(NSURL *)URL;
- (PassRefPtr<WebCore::DocumentLoader>)_createDocumentLoaderWithRequest:(NSURLRequest *)request;
- (void)_prepareForDataSourceReplacement;
- (void)_didFinishLoad;
- (void)_addHistoryItemForFragmentScroll;
- (BOOL)_shouldTreatURLAsSameAsCurrent:(NSURL *)URL;
- (void)_provisionalLoadStarted;

@end

#endif

@interface NSObject (WebInternalFrameLoadDelegate)
- (void)webFrame:(WebFrame *)webFrame didFinishLoadWithError:(NSError *)error;
@end
