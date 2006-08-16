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
#import <WebKit/WebFramePrivate.h>
#import <WebKitSystemInterface.h>

@class WebDataSource;
@class WebMainResourceLoader;
@class WebIconLoader;
@class WebLoader;
@class WebResource;

@interface WebFrameLoader : NSObject
{
@public
    WebMainResourceLoader *mainResourceLoader;
    
    NSMutableArray *subresourceLoaders;
    NSMutableArray *plugInStreamLoaders;
    WebIconLoader *iconLoader;
    
    WebFrame *webFrame;
    WebDataSource *dataSource;
    WebDataSource *provisionalDataSource;
    WebFrameState state;
    
    NSMutableDictionary *pendingArchivedResources;
}

- (id)initWithWebFrame:(WebFrame *)wf;
// FIXME: should really split isLoadingIcon from hasLoadedIcon, no?
- (BOOL)hasIconLoader;
- (void)loadIconWithRequest:(NSURLRequest *)request;
- (void)stopLoadingIcon;
- (void)addPlugInStreamLoader:(WebLoader *)loader;
- (void)removePlugInStreamLoader:(WebLoader *)loader;
- (void)setDefersCallbacks:(BOOL)defers;
- (void)stopLoadingPlugIns;
- (BOOL)isLoadingMainResource;
- (BOOL)isLoadingSubresources;
- (BOOL)isLoading;
- (void)stopLoadingSubresources;
- (void)addSubresourceLoader:(WebLoader *)loader;
- (void)removeSubresourceLoader:(WebLoader *)loader;
- (NSData *)mainResourceData;
- (void)releaseMainResourceLoader;
- (void)cancelMainResourceLoad;
- (BOOL)startLoadingMainResourceWithRequest:(NSMutableURLRequest *)request identifier:(id)identifier;
- (void)stopLoadingWithError:(NSError *)error;
- (void)clearProvisionalLoad;
- (void)stopLoading;
- (void)markLoadComplete;
- (void)commitProvisionalLoad;
- (void)startLoading;
- (void)startProvisionalLoad:(WebDataSource *)dataSource;
- (WebDataSource *)dataSource;
- (WebDataSource *)provisionalDataSource;
- (WebDataSource *)activeDataSource;
- (WebFrameState)state;
- (void)clearDataSource;
- (void)setupForReplace;
+ (CFAbsoluteTime)timeOfLastCompletedLoad;

- (WebResource *)_archivedSubresourceForURL:(NSURL *)URL;
- (BOOL)_defersCallbacks;
- (id)_identifierForInitialRequest:(NSURLRequest *)clientRequest;
- (NSURLRequest *)_willSendRequest:(NSMutableURLRequest *)clientRequest forResource:(id)identifier redirectResponse:(NSURLResponse *)redirectResponse;
- (void)_didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier;
- (void)_didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier;
- (void)_didReceiveResponse:(NSURLResponse *)r forResource:(id)identifier;
- (void)_didReceiveData:(NSData *)data contentLength:(int)lengthReceived forResource:(id)identifier;
- (void)_didFinishLoadingForResource:(id)identifier;
- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier;
- (BOOL)_privateBrowsingEnabled;
- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier;
- (void)_addPlugInStreamLoader:(WebLoader *)loader;
- (void)_removePlugInStreamLoader:(WebLoader *)loader;
- (void)_finishedLoadingResource;
- (void)_receivedError:(NSError *)error;
- (void)_addSubresourceLoader:(WebLoader *)loader;
- (void)_removeSubresourceLoader:(WebLoader *)loader;
- (NSURLRequest *)_originalRequest;
- (WebFrame *)webFrame;
- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete;
- (NSURLRequest *)initialRequest;
- (void)_receivedData:(NSData *)data;
- (void)_setRequest:(NSURLRequest *)request;
- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)r proxy:(WKNSURLConnectionDelegateProxyPtr)proxy;
- (void)_handleFallbackContent;
- (BOOL)_isStopping;
- (void)_decidePolicyForMIMEType:(NSString *)MIMEType decisionListener:(WebPolicyDecisionListener *)listener;
- (void)_setupForReplaceByMIMEType:(NSString *)newMIMEType;
- (void)_setResponse:(NSURLResponse *)response;
- (void)_mainReceivedError:(NSError *)error complete:(BOOL)isComplete;
- (void)_finishedLoading;
- (void)_mainReceivedBytesSoFar:(unsigned)bytesSoFar complete:(BOOL)isComplete;
- (void)_iconLoaderReceivedPageIcon:(WebIconLoader *)iconLoader;
- (NSURL *)_URL;

- (NSError *)cancelledErrorWithRequest:(NSURLRequest *)request;
- (NSError *)fileDoesNotExistErrorWithResponse:(NSURLResponse *)response;
- (BOOL)willUseArchiveForRequest:(NSURLRequest *)r originalURL:(NSURL *)originalURL loader:(WebLoader *)loader;
- (BOOL)archiveLoadPendingForLoader:(WebLoader *)loader;
- (void)deliverArchivedResourcesAfterDelay;
- (void)cancelPendingArchiveLoadForLoader:(WebLoader *)loader;
- (void)clearArchivedResources;
- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request mainResource:(BOOL)mainResource alwaysFromRequest:(BOOL)f;
- (void)cannotShowMIMETypeForURL:(NSURL *)URL;
- (NSError *)interruptForPolicyChangeErrorWithRequest:(NSURLRequest *)request;
- (BOOL)isHostedByObjectElement;
- (BOOL)isLoadingMainFrame;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme;
+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme;
                                                                                                      
@end
