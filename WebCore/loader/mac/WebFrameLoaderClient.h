/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import <Cocoa/Cocoa.h>

namespace WebCore {
    class DocumentLoader;
    class WebResourceLoader;
}

typedef struct LoadErrorResetToken LoadErrorResetToken;

@class DOMElement;
@class WebCoreFrameBridge;
@class WebPolicyDecider;
@class WebResource;

@protocol WebFrameLoaderClient <NSObject>

- (BOOL)_hasWebView; // mainly for assertions
- (BOOL)_hasFrameView; // ditto

- (BOOL)_hasBackForwardList;
- (void)_resetBackForwardList;

- (BOOL)_provisionalItemIsTarget;
- (BOOL)_loadProvisionalItemFromPageCache;
- (void)_invalidateCurrentItemPageCache;

- (BOOL)_privateBrowsingEnabled;

- (void)_makeDocumentView;
- (void)_makeRepresentationForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_setDocumentViewFromPageCache:(NSDictionary *)dictionary;
- (void)_forceLayout;
- (void)_forceLayoutForNonHTML;

- (void)_updateHistoryForCommit;

- (void)_updateHistoryForBackForwardNavigation;
- (void)_updateHistoryForReload;
- (void)_updateHistoryForStandardLoad;
- (void)_updateHistoryForInternalLoad;

- (void)_updateHistoryAfterClientRedirect;

- (void)_setCopiesOnScroll;

- (LoadErrorResetToken *)_tokenForLoadErrorReset;
- (void)_resetAfterLoadError:(LoadErrorResetToken *)token;
- (void)_doNotResetAfterLoadError:(LoadErrorResetToken *)token;

- (void)_detachedFromParent1;
- (void)_detachedFromParent2;
- (void)_detachedFromParent3;
- (void)_detachedFromParent4;

- (void)_loadedFromPageCache;

- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)response proxy:(id)proxy;

- (id)_dispatchIdentifierForInitialRequest:(NSURLRequest *)request fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (NSURLRequest *)_dispatchResource:(id)identifier willSendRequest:(NSURLRequest *)clientRequest redirectResponse:(NSURLResponse *)redirectResponse fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchDidReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchDidCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didReceiveResponse:(NSURLResponse *)r fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didReceiveContentLength:(int)lengthReceived fromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didFinishLoadingFromDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_dispatchResource:(id)identifier didFailLoadingWithError:error fromDocumentLoader:(WebCore::DocumentLoader*)loader;

- (void)_dispatchDidHandleOnloadEventsForFrame;
- (void)_dispatchDidReceiveServerRedirectForProvisionalLoadForFrame;
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

- (WebCoreFrameBridge *)_dispatchCreateWebViewWithRequest:(NSURLRequest *)request;
- (void)_dispatchShow;

- (void)_dispatchDecidePolicyForMIMEType:(NSString *)MIMEType request:(NSURLRequest *)request decider:(WebPolicyDecider *)decider;
- (void)_dispatchDecidePolicyForNewWindowAction:(NSDictionary *)action request:(NSURLRequest *)request newFrameName:(NSString *)frameName decider:(WebPolicyDecider *)decider;
- (void)_dispatchDecidePolicyForNavigationAction:(NSDictionary *)action request:(NSURLRequest *)request decider:(WebPolicyDecider *)decider;
- (void)_dispatchUnableToImplementPolicyWithError:(NSError *)error;

- (void)_dispatchSourceFrame:(WebCoreFrameBridge *)sourceFrame willSubmitForm:(DOMElement *)form withValues:(NSDictionary *)values submissionDecider:(WebPolicyDecider *)decider;

- (void)_dispatchDidLoadMainResourceForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_clearLoadingFromPageCacheForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (BOOL)_isDocumentLoaderLoadingFromPageCache:(WebCore::DocumentLoader*)loader;
- (void)_revertToProvisionalStateForDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_setMainDocumentError:(NSError *)error forDocumentLoader:(WebCore::DocumentLoader*)loader;
- (void)_clearUnarchivingStateForLoader:(WebCore::DocumentLoader*)loader;

- (void)_progressStarted;
- (void)_progressCompleted;

- (void)_incrementProgressForIdentifier:(id)identifier response:(NSURLResponse *)response;
- (void)_incrementProgressForIdentifier:(id)identifier data:(NSData *)data;
- (void)_completeProgressForIdentifier:(id)identifier;

- (void)_setMainFrameDocumentReady:(BOOL)ready;

- (void)_startDownloadWithRequest:(NSURLRequest *)request;

- (void)_willChangeTitleForDocument:(WebCore::DocumentLoader*)loader;
- (void)_didChangeTitleForDocument:(WebCore::DocumentLoader*)loader;

- (void)_committedLoadWithDocumentLoader:(WebCore::DocumentLoader*)loader data:(NSData *)data;
- (void)_finishedLoadingDocument:(WebCore::DocumentLoader*)loader;
- (void)_documentLoader:(WebCore::DocumentLoader*)loader setMainDocumentError:(NSError *)error;
- (void)_finalSetupForReplaceWithDocumentLoader:(WebCore::DocumentLoader*)loader;

- (NSError *)_cancelledErrorWithRequest:(NSURLRequest *)request;
- (NSError *)_cannotShowURLErrorWithRequest:(NSURLRequest *)request;
- (NSError *)_interruptForPolicyChangeErrorWithRequest:(NSURLRequest *)request;

- (NSError *)_cannotShowMIMETypeErrorWithResponse:(NSURLResponse *)response;
- (NSError *)_fileDoesNotExistErrorWithResponse:(NSURLResponse *)response;

- (BOOL)_shouldFallBackForError:(NSError *)error;

- (NSURL *)_mainFrameURL;

- (void)_setDefersCallbacks:(BOOL)defers;

- (BOOL)_willUseArchiveForRequest:(NSURLRequest *)request originalURL:(NSURL *)originalURL loader:(WebCore::WebResourceLoader *)loader;
- (BOOL)_archiveLoadPendingForLoader:(WebCore::WebResourceLoader *)loader;
- (void)_cancelPendingArchiveLoadForLoader:(WebCore::WebResourceLoader *)loader;
- (void)_clearArchivedResources;

- (BOOL)_canHandleRequest:(NSURLRequest *)request;
- (BOOL)_canShowMIMEType:(NSString *)MIMEType;
- (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme;
- (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme;

- (NSDictionary *)_elementForEvent:(NSEvent *)event;

- (WebPolicyDecider *)_createPolicyDeciderWithTarget:(id)obj action:(SEL)selector;

- (void)_frameLoadCompleted;
- (void)_restoreScrollPositionAndViewState;
- (void)_provisionalLoadStarted;
    // used to decide to use loadType=Same
- (BOOL)_shouldTreatURLAsSameAsCurrent:(NSURL *)URL;
- (void)_addHistoryItemForFragmentScroll;
- (void)_didFinishLoad;
- (void)_prepareForDataSourceReplacement;
- (PassRefPtr<WebCore::DocumentLoader>)_createDocumentLoaderWithRequest:(NSURLRequest *)request;
- (void)_setTitle:(NSString *)title forURL:(NSURL *)URL;

@end
