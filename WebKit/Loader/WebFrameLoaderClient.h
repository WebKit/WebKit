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

typedef struct LoadErrorResetToken LoadErrorResetToken;

@class WebDocumentLoader;
@class WebPolicyDecisionListener;

@protocol WebFrameLoaderClient

- (void)_resetBackForwardList;

- (BOOL)_provisionalItemIsTarget;
- (BOOL)_loadProvisionalItemFromPageCache;
- (void)_invalidateCurrentItemPageCache;

- (BOOL)_privateBrowsingEnabled;

- (void)_makeDocumentView;
- (void)_setDocumentViewFromPageCache:(NSDictionary *)dictionary;
- (void)_forceLayout;

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

- (id)_dispatchIdentifierForInitialRequest:(NSURLRequest *)request fromDocumentLoader:(WebDocumentLoader *)loader;
- (NSURLRequest *)_dispatchResource:(id)identifier willSendRequest:(NSURLRequest *)clientRequest redirectResponse:(NSURLResponse *)redirectResponse fromDocumentLoader:(WebDocumentLoader *)loader;
- (void)_dispatchDidReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(WebDocumentLoader *)loader;
- (void)_dispatchDidCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier fromDocumentLoader:(WebDocumentLoader *)loader;
- (void)_dispatchResource:(id)identifier didReceiveResponse:(NSURLResponse *)r fromDocumentLoader:(WebDocumentLoader *)loader;
- (void)_dispatchResource:(id)identifier didReceiveContentLength:(int)lengthReceived fromDocumentLoader:(WebDocumentLoader *)loader;
- (void)_dispatchResource:(id)identifier didFinishLoadingFromDocumentLoader:(WebDocumentLoader *)loader;
- (void)_dispatchResource:(id)identifier didFailLoadingWithError:error fromDocumentLoader:(WebDocumentLoader *)loader;

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

- (WebFrame *)_dispatchCreateWebViewWithRequest:(NSURLRequest *)request;
- (void)_dispatchShow;

- (void)_dispatchDecidePolicyForMIMEType:(NSString *)MIMEType request:(NSURLRequest *)request decisionListener:(WebPolicyDecisionListener *)decisionListener;
- (void)_dispatchDecidePolicyForNewWindowAction:(NSDictionary *)action request:(NSURLRequest *)request newFrameName:(NSString *)frameName decisionListener:(WebPolicyDecisionListener *)decisionListener;
- (void)_dispatchDecidePolicyForNavigationAction:(NSDictionary *)action request:(NSURLRequest *)request decisionListener:(WebPolicyDecisionListener *)decisionListener;
- (void)_dispatchUnableToImplementPolicyWithError:(NSError *)error;

- (void)_dispatchDidLoadMainResourceForDocumentLoader:(WebDocumentLoader *)loader;

@end
