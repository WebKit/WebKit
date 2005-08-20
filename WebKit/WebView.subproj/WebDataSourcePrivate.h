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

#import <WebKit/WebDataSource.h>

@class DOMDocumentFragment;
@class DOMElement;
@class DOMRange;
@class NSError;
@class NSURLRequest;
@class NSURLResponse;
@class WebArchive;
@class WebLoader;
@class WebBridge;
@class WebHistoryItem;
@class WebIconLoader;
@class WebMainResourceLoader;
@class WebResource;
@class WebView;

@protocol WebDocumentRepresentation;

@interface WebDataSourcePrivate : NSObject
{
@public
    NSData *resourceData;

    id <WebDocumentRepresentation> representation;
    
    WebView *webView;
    
    // A reference to actual request used to create the data source.
    // This should only be used by the resourceLoadDelegate's
    // identifierForInitialRequest:fromDatasource: method.  It is
    // not guaranteed to remain unchanged, as requests are mutable.
    NSURLRequest *originalRequest;
    
    // A copy of the original request used to create the data source.
    // We have to copy the request because requests are mutable.
    NSURLRequest *originalRequestCopy;
    
    // The 'working' request for this datasource.  It may be mutated
    // several times from the original request to include additional
    // headers, cookie information, canonicalization and redirects.
    NSMutableURLRequest *request;
    
    NSURLResponse *response;

    // Client for main resource.
    WebMainResourceLoader *mainResourceLoader;
    
    // Clients for other resources.
    NSMutableArray *subresourceLoaders;
    NSMutableArray *plugInStreamLoaders;

    // The time when the data source was told to start loading.
    double loadingStartedTime;
    
    BOOL primaryLoadComplete;

    BOOL stopping;

    BOOL isClientRedirect;

    NSString *pageTitle;
    
    NSString *encoding;
    NSString *overrideEncoding;

    // Errors associated with resources.
    NSMutableDictionary *errors;

    // Error associated with main document.
    NSError *mainDocumentError;

    BOOL loading; // self and webView are retained while loading

    BOOL gotFirstByte; // got first byte
    BOOL committed; // This data source has been committed

    BOOL defersCallbacks;

    NSURL *iconURL;
    WebIconLoader *iconLoader;

    // The action that triggered loading of this data source -
    // we keep this around for the benefit of the various policy
    // handlers.
    NSDictionary *triggeringAction;

    // The last request that we checked click policy for - kept around
    // so we can avoid asking again needlessly.
    NSURLRequest *lastCheckedRequest;

    // We retain all the received responses so we can play back the
    // WebResourceLoadDelegate messages if the item is loaded from the
    // page cache.
    NSMutableArray *responses;
    BOOL stopRecordingResponses;

    BOOL storedInPageCache;
    BOOL loadingFromPageCache;

    WebFrame *webFrame;
    
    NSMutableDictionary *subresources;
    NSMutableDictionary *pendingSubframeArchives;
}

@end

@interface WebDataSource (WebPrivate)

// Other private methods
- (void)_addSubresources:(NSArray *)subresources;
- (NSFileWrapper *)_fileWrapperForURL:(NSURL *)URL;

- (WebArchive *)_archiveWithCurrentState:(BOOL)currentState;
- (WebArchive *)_archiveWithMarkupString:(NSString *)markupString nodes:(NSArray *)nodes;
- (void)_addSubframeArchives:(NSArray *)subframeArchives;
- (WebArchive *)_popSubframeArchiveWithName:(NSString *)frameName;

- (DOMElement *)_imageElementWithImageResource:(WebResource *)resource;
- (DOMDocumentFragment *)_documentFragmentWithImageResource:(WebResource *)resource;
- (DOMDocumentFragment *)_documentFragmentWithArchive:(WebArchive *)archive;
- (void)_replaceSelectionWithArchive:(WebArchive *)archive selectReplacement:(BOOL)selectReplacement;

- (NSError *)_mainDocumentError;
- (NSString *)_stringWithData:(NSData *)data;
- (void)_startLoading;
- (void)_stopLoading;
- (NSURL *)_URL;
- (NSURL *)_URLForHistory;
- (WebView *)_webView;
- (void)_setRepresentation:(id<WebDocumentRepresentation>)representation;
- (void)_setWebView:(WebView *)webView;
- (void)_startLoading:(NSDictionary *)pageCache;
- (void)_stopLoadingInternal;
- (BOOL)_isStopping;
- (void)_recursiveStopLoading;
- (void)_addSubresourceLoader:(WebLoader *)loader;
- (void)_removeSubresourceLoader:(WebLoader *)loader;
- (void)_addPlugInStreamLoader:(WebLoader *)loader;
- (void)_removePlugInStreamLoader:(WebLoader *)loader;
- (void)_setPrimaryLoadComplete:(BOOL)flag;
- (double)_loadingStartedTime;
- (void)_setTitle:(NSString *)title;
- (void)_setURL:(NSURL *)URL;
- (void)__adoptRequest:(NSMutableURLRequest *)request;
- (void)_setRequest:(NSURLRequest *)request;
- (void)_setResponse:(NSURLResponse *)response;
- (void)_clearErrors;
- (void)_setMainDocumentError:(NSError *)error;
+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission;
+ (Class)_representationClassForMIMEType:(NSString *)MIMEType;
- (void)_loadIcon;
- (void)_setIconURL:(NSURL *)URL;
- (void)_setIconURL:(NSURL *)URL withType:(NSString *)iconType;
- (void)_setOverrideEncoding:(NSString *)overrideEncoding;
- (NSString *)_overrideEncoding;
- (void)_setIsClientRedirect:(BOOL)flag;
- (BOOL)_isClientRedirect;

// Convenience interface for getting here from an WebDataSource.
// This returns nil if the representation is not an WebHTMLRepresentation.
- (WebBridge *)_bridge;

- (BOOL)_isCommitted;
- (void)_commitIfReady:(NSDictionary *)pageCache;
- (void)_makeRepresentation;
- (void)_receivedData:(NSData *)data;
- (void)_finishedLoading;
- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete;
- (void)_defersCallbacksChanged;
- (NSURLRequest *)_originalRequest;
- (NSDictionary *)_triggeringAction;
- (void)_setTriggeringAction:(NSDictionary *)action;
- (NSURLRequest *)_lastCheckedRequest;
- (void)_setLastCheckedRequest:(NSURLRequest *)request;
- (void)_setStoredInPageCache:(BOOL)f;
- (BOOL)_storedInPageCache;
- (BOOL)_loadingFromPageCache;

- (void)_addResponse:(NSURLResponse *)r;
- (NSArray *)_responses;
- (void)_stopRecordingResponses;

- (void)_stopLoadingWithError:(NSError *)error;

- (void)_setWebFrame:(WebFrame *)frame;

- (BOOL)_isDocumentHTML;
- (NSString *)_title;

@end
