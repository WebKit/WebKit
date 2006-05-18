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

#import <WebKit/WebDataSourcePrivate.h>

@class NSURL;

@class DOMDocumentFragment;
@class DOMElement;
@class DOMRange;
@class NSError;
@class NSURLRequest;
@class NSURLResponse;
@class WebArchive;
@class WebLoader;
@class WebFrameBridge;
@class WebHistoryItem;
@class WebIconLoader;
@class WebMainResourceLoader;
@class WebResource;
@class WebUnarchivingState;
@class WebView;
@protocol WebDocumentRepresentation;

@interface WebDataSource (WebInternal)
- (void)_addPlugInStreamLoader:(WebLoader *)loader;
- (void)_removePlugInStreamLoader:(WebLoader *)loader;
- (void)_stopLoadingWithError:(NSError *)error;
- (void)_setTitle:(NSString *)title;
- (NSString *)_overrideEncoding;
- (void)_setIconURL:(NSURL *)URL;
- (void)_setTitle:(NSString *)title;
- (NSString *)_overrideEncoding;
- (void)_setIconURL:(NSURL *)URL;
- (void)_setIconURL:(NSURL *)URL withType:(NSString *)iconType;
- (void)_addSubresourceLoader:(WebLoader *)loader;
- (void)_removeSubresourceLoader:(WebLoader *)loader;
- (NSURLRequest *)_originalRequest;
- (WebView *)_webView;
- (WebResource *)_archivedSubresourceForURL:(NSURL *)URL;
- (void)_addResponse:(NSURLResponse *)r;
- (WebFrameBridge *)_bridge;
- (void)_setOverrideEncoding:(NSString *)overrideEncoding;
- (void)_replaceSelectionWithArchive:(WebArchive *)archive selectReplacement:(BOOL)selectReplacement;
- (NSURL *)_URL;
+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission;
- (void)_setPrimaryLoadComplete:(BOOL)flag;
- (void)_setMainDocumentError:(NSError *)error;
- (void)_addToUnarchiveState:(WebArchive *)archive;
- (NSURL *)_URLForHistory;
- (void)_setWebFrame:(WebFrame *)frame;
- (BOOL)_isClientRedirect;
- (void)_makeRepresentation;
- (double)_loadingStartedTime;
- (BOOL)_isStopping;
- (void)_finishedLoading;
- (void)_setResponse:(NSURLResponse *)response;
- (void)_setupForReplaceByMIMEType:(NSString *)mimeType;
- (void)_setRequest:(NSURLRequest *)request;
- (void)_receivedData:(NSData *)data;
- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete;
- (BOOL)_isDocumentHTML;
- (DOMDocumentFragment *)_documentFragmentWithImageResource:(WebResource *)resource;
- (DOMDocumentFragment *)_documentFragmentWithArchive:(WebArchive *)archive;
- (NSString *)_title;
- (void)_stopRecordingResponses;
- (BOOL)_loadingFromPageCache;
- (NSArray *)_responses;
- (void)_stopLoading;
- (void)__adoptRequest:(NSMutableURLRequest *)request;
- (void)_setTriggeringAction:(NSDictionary *)action;
- (NSDictionary *)_triggeringAction;
- (NSURLRequest *)_lastCheckedRequest;
- (void)_setLastCheckedRequest:(NSURLRequest *)request;
- (void)_setURL:(NSURL *)URL;
- (void)_setIsClientRedirect:(BOOL)flag;
- (WebArchive *)_popSubframeArchiveWithName:(NSString *)frameName;
- (void)_defersCallbacksChanged;
- (void)_startLoading;
- (void)_loadFromPageCache:(NSDictionary *)pageCache;
- (DOMElement *)_imageElementWithImageResource:(WebResource *)resource;
- (void)_finishedLoadingResource;
- (void)_mainReceivedBytesSoFar:(unsigned)bytesSoFar complete:(BOOL)isComplete;
- (void)_receivedError:(NSError *)error;
- (void)_mainReceivedError:(NSError *)error complete:(BOOL)isComplete;

@end
