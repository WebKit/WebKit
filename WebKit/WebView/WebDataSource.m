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

#import "WebDataSource.h"

#import "WebArchive.h"
#import "WebArchiver.h"
#import "WebDOMOperationsPrivate.h"
#import "WebDataProtocol.h"
#import "WebDataSourceInternal.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebDocument.h"
#import "WebDownloadInternal.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFrameLoader.h"
#import "WebFrameLoadDelegate.h"
#import "WebFrameView.h"
#import "WebHTMLRepresentation.h"
#import "WebHTMLViewPrivate.h"
#import "WebHistory.h"
#import "WebHistoryItemPrivate.h"
#import "WebIconDatabasePrivate.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebKitStatisticsPrivate.h"
#import "WebNSObjectExtras.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebPDFRepresentation.h"
#import "WebPreferences.h"
#import "WebResourceLoadDelegate.h"
#import "WebResourcePrivate.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebUnarchivingState.h"
#import "WebViewInternal.h"
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLRequest.h>
#import <JavaScriptCore/Assertions.h>
#import <WebKit/DOMHTML.h>
#import <WebKit/DOMPrivate.h>
#import <WebKitSystemInterface.h>
#import "WebDocumentLoadStateMac.h"

@interface WebDataSourcePrivate : NSObject
{
    @public
    
    WebDocumentLoadStateMac *loadState;
   
    id <WebDocumentRepresentation> representation;
    
    BOOL loadingFromPageCache;
    WebUnarchivingState *unarchivingState;
    NSMutableDictionary *subresources;
    BOOL representationFinishedLoading;
}

@end

@implementation WebDataSourcePrivate 

- (void)dealloc
{
    ASSERT(![loadState isLoading]);

    [loadState release];
    
    [representation release];
    [unarchivingState release];

    [super dealloc];
}

@end

@interface WebDataSource (WebFileInternal)
@end

@implementation WebDataSource (WebFileInternal)

- (void)_setRepresentation: (id<WebDocumentRepresentation>)representation
{
    [_private->representation release];
    _private->representation = [representation retain];
    _private->representationFinishedLoading = NO;
}

static inline void addTypesFromClass(NSMutableDictionary *allTypes, Class class, NSArray *supportTypes)
{
    NSEnumerator *enumerator = [supportTypes objectEnumerator];
    ASSERT(enumerator != nil);
    NSString *mime = nil;
    while ((mime = [enumerator nextObject]) != nil) {
        // Don't clobber previously-registered classes.
        if ([allTypes objectForKey:mime] == nil)
            [allTypes setObject:class forKey:mime];
    }
}

+ (Class)_representationClassForMIMEType:(NSString *)MIMEType
{
    Class repClass;
    return [WebView _viewClass:nil andRepresentationClass:&repClass forMIMEType:MIMEType] ? repClass : nil;
}

@end

@implementation WebDataSource (WebPrivate)

- (NSError *)_mainDocumentError
{
    return [_private->loadState mainDocumentError];
}

- (void)_addSubframeArchives:(NSArray *)subframeArchives
{
    NSEnumerator *enumerator = [subframeArchives objectEnumerator];
    WebArchive *archive;
    while ((archive = [enumerator nextObject]) != nil)
        [self _addToUnarchiveState:archive];
}

- (NSFileWrapper *)_fileWrapperForURL:(NSURL *)URL
{
    if ([URL isFileURL]) {
        NSString *path = [[URL path] stringByResolvingSymlinksInPath];
        return [[[NSFileWrapper alloc] initWithPath:path] autorelease];
    }
    
    WebResource *resource = [self subresourceForURL:URL];
    if (resource)
        return [resource _fileWrapperRepresentation];
    
    NSCachedURLResponse *cachedResponse = [[self _webView] _cachedResponseForURL:URL];
    if (cachedResponse) {
        NSFileWrapper *wrapper = [[[NSFileWrapper alloc] initRegularFileWithContents:[cachedResponse data]] autorelease];
        [wrapper setPreferredFilename:[[cachedResponse response] suggestedFilename]];
        return wrapper;
    }
    
    return nil;
}

@end

@implementation WebDataSource (WebInternal)

- (void)_finishedLoading
{
    _private->representationFinishedLoading = YES;
    [[self representation] finishedLoadingWithDataSource:self];
}

- (void)_receivedData:(NSData *)data
{
    [[self representation] receivedData:data withDataSource:self];
    [[[[self webFrame] frameView] documentView] dataSourceUpdated:self];
}

- (void)_setMainDocumentError:(NSError *)error
{
    if (!_private->representationFinishedLoading) {
        _private->representationFinishedLoading = YES;
        [[self representation] receivedError:error withDataSource:self];
    }
}

- (void)_clearUnarchivingState
{
    [_private->unarchivingState release];
    _private->unarchivingState = nil;
}

- (void)_revertToProvisionalState
{
    [self _setRepresentation:nil];
}

+ (NSMutableDictionary *)_repTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static NSMutableDictionary *repTypes = nil;
    static BOOL addedImageTypes = NO;
    
    if (!repTypes) {
        repTypes = [[NSMutableDictionary alloc] init];
        addTypesFromClass(repTypes, [WebHTMLRepresentation class], [WebHTMLRepresentation supportedNonImageMIMETypes]);
        
        // Since this is a "secret default" we don't both registering it.
        BOOL omitPDFSupport = [[NSUserDefaults standardUserDefaults] boolForKey:@"WebKitOmitPDFSupport"];
        if (!omitPDFSupport)
            addTypesFromClass(repTypes, [WebPDFRepresentation class], [WebPDFRepresentation supportedMIMETypes]);
    }
    
    if (!addedImageTypes && !allowImageTypeOmission) {
        addTypesFromClass(repTypes, [WebHTMLRepresentation class], [WebHTMLRepresentation supportedImageMIMETypes]);
        addedImageTypes = YES;
    }
    
    return repTypes;
}

- (void)_decidePolicyForMIMEType:(NSString *)MIMEType decisionListener:(WebPolicyDecisionListener *)listener
{
    WebView *wv = [self _webView];
    [[wv _policyDelegateForwarder] webView:wv decidePolicyForMIMEType:MIMEType
                                   request:[self request]
                                     frame:[self webFrame]
                          decisionListener:listener];
}

- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete
{
    // MOVABLE
    WebFrameBridge *bridge = [[self webFrame] _bridge];
    
    // Retain the bridge because the stop may release the last reference to it.
    [bridge retain];
    
    if (isComplete) {
        // FIXME: Don't want to do this if an entirely new load is going, so should check
        // that both data sources on the frame are either self or nil.
        // Can't call [self _bridge] because we might not have commited yet
        [bridge stop];
        // FIXME: WebKitErrorPlugInWillHandleLoad is a workaround for the cancel we do to prevent loading plugin content twice.  See <rdar://problem/4258008>
        if ([error code] != NSURLErrorCancelled && [error code] != WebKitErrorPlugInWillHandleLoad)
            [bridge handleFallbackContent];
    }
    
    [bridge release];
    
    [[self webFrame] _receivedMainResourceError:error];
    [_private->loadState mainReceivedError:error complete:isComplete];
}

- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)r proxy:(WKNSURLConnectionDelegateProxyPtr) proxy
{
    [WebDownload _downloadWithLoadingConnection:connection
                                        request:request
                                       response:r
                                       delegate:[[self _webView] downloadDelegate]
                                          proxy:proxy];
}

- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier
{
    WebView *webView = [self _webView];
    
    [webView _completeProgressForIdentifier:identifier];
    
    if (error)
        [[webView _resourceLoadDelegateForwarder] webView:webView resource:identifier didFailLoadingWithError:error fromDataSource:self];
}

- (void)_didFinishLoadingForResource:(id)identifier
{
    WebView *webView = [self _webView];
    
    [webView _completeProgressForIdentifier:identifier];    
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidFinishLoadingFromDataSource)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didFinishLoadingFromDataSource:self];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didFinishLoadingFromDataSource:self];
}

- (void)_didReceiveData:(NSData *)data contentLength:(int)lengthReceived forResource:(id)identifier
{
    WebView *webView = [self _webView];
    
    [webView _incrementProgressForIdentifier:identifier data:data];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveContentLength)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveContentLength:(WebNSUInteger)lengthReceived fromDataSource:self];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveContentLength:(WebNSUInteger)lengthReceived fromDataSource:self];
}

- (void)_didReceiveResponse:(NSURLResponse *)r forResource:(id)identifier
{
    WebView *webView = [self _webView];
    
    [_private->loadState addResponse:r];
    
    [webView _incrementProgressForIdentifier:identifier response:r];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveResponse)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveResponse:r fromDataSource:self];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveResponse:r fromDataSource:self];
}

- (void)_didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    WebView *webView = [self _webView];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidCancelAuthenticationChallenge)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:self];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:self];
}

- (void)_didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    WebView *webView = [self _webView];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveAuthenticationChallenge)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:self];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:self];
}

- (NSURLRequest *)_willSendRequest:(NSMutableURLRequest *)clientRequest forResource:(id)identifier redirectResponse:(NSURLResponse *)redirectResponse
{
    WebView *webView = [self _webView];
    
    [clientRequest _web_setHTTPUserAgent:[webView userAgentForURL:[clientRequest URL]]];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsWillSendRequest)
        return [[webView resourceLoadDelegate] webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:self];
    else
        return [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:self];
}        

- (id)_identifierForInitialRequest:(NSURLRequest *)clientRequest
{
    WebView *webView = [self _webView];
    
    // The identifier is released after the last callback, rather than in dealloc
    // to avoid potential cycles.
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsIdentifierForRequest)
        return [[[webView resourceLoadDelegate] webView:webView identifierForInitialRequest:clientRequest fromDataSource:self] retain];
    else
        return [[[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView identifierForInitialRequest:clientRequest fromDataSource:self] retain];
}

- (WebResource *)_archivedSubresourceForURL:(NSURL *)URL
{
    return [_private->unarchivingState archivedResourceForURL:URL];
}

- (void)_startLoading
{
    [_private->loadState prepareForLoadStart];
    
    if ([[_private->loadState frameLoader] isLoadingMainResource])
        return;
    
    _private->loadingFromPageCache = NO;
    
    id identifier;
    id resourceLoadDelegate = [[self _webView] resourceLoadDelegate];
    if ([resourceLoadDelegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)])
        identifier = [resourceLoadDelegate webView:[self _webView] identifierForInitialRequest:[_private->loadState originalRequest] fromDataSource:self];
    else
        identifier = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:[self _webView] identifierForInitialRequest:[_private->loadState originalRequest] fromDataSource:self];
    
    if (![[_private->loadState frameLoader] startLoadingMainResourceWithRequest:[_private->loadState actualRequest] identifier:identifier])
        [_private->loadState updateLoading];
}

- (void)_replaceSelectionWithArchive:(WebArchive *)archive selectReplacement:(BOOL)selectReplacement
{
    DOMDocumentFragment *fragment = [self _documentFragmentWithArchive:archive];
    if (fragment)
        [[self _bridge] replaceSelectionWithFragment:fragment selectReplacement:selectReplacement smartReplace:NO matchStyle:NO];
}

- (DOMDocumentFragment *)_documentFragmentWithArchive:(WebArchive *)archive
{
    ASSERT(archive);
    WebResource *mainResource = [archive mainResource];
    if (mainResource) {
        NSString *MIMEType = [mainResource MIMEType];
        if ([WebView canShowMIMETypeAsHTML:MIMEType]) {
            NSString *markupString = [[NSString alloc] initWithData:[mainResource data] encoding:NSUTF8StringEncoding];
            // FIXME: seems poor form to do this as a side effect of getting a document fragment
            [self _addToUnarchiveState:archive];
            DOMDocumentFragment *fragment = [[self _bridge] documentFragmentWithMarkupString:markupString baseURLString:[[mainResource URL] _web_originalDataAsString]];
            [markupString release];
            return fragment;
        } else if ([[WebFrameBridge supportedImageResourceMIMETypes] containsObject:MIMEType]) {
            return [self _documentFragmentWithImageResource:mainResource];
            
        }
    }
    return nil;
}

- (DOMDocumentFragment *)_documentFragmentWithImageResource:(WebResource *)resource
{
    DOMElement *imageElement = [self _imageElementWithImageResource:resource];
    if (!imageElement)
        return 0;
    DOMDocumentFragment *fragment = [[[self _bridge] DOMDocument] createDocumentFragment];
    [fragment appendChild:imageElement];
    return fragment;
}

- (DOMElement *)_imageElementWithImageResource:(WebResource *)resource
{
    if (!resource)
        return 0;
    
    [self addSubresource:resource];
    
    DOMElement *imageElement = [[[self _bridge] DOMDocument] createElement:@"img"];
    
    // FIXME: calling _web_originalDataAsString on a file URL returns an absolute path. Workaround this.
    NSURL *URL = [resource URL];
    [imageElement setAttribute:@"src" value:[URL isFileURL] ? [URL absoluteString] : [URL _web_originalDataAsString]];
    
    return imageElement;
}

// May return nil if not initialized with a URL.
- (NSURL *)_URL
{
    return [[self request] URL];
}

- (void)_loadFromPageCache:(NSDictionary *)pageCache
{
    [_private->loadState prepareForLoadStart];
    _private->loadingFromPageCache = YES;
    [_private->loadState setCommitted:YES];
    [[self webFrame] _commitProvisionalLoad:pageCache];
}

- (WebArchive *)_popSubframeArchiveWithName:(NSString *)frameName
{
    return [_private->unarchivingState popSubframeArchiveWithFrameName:frameName];
}

- (WebFrameBridge *)_bridge
{
    ASSERT([_private->loadState isCommitted]);
    return [[self webFrame] _bridge];
}

- (WebView *)_webView
{
    return [[[_private->loadState frameLoader] webFrame] webView];
}

- (BOOL)_isDocumentHTML
{
    NSString *MIMEType = [[self response] MIMEType];
    return [WebView canShowMIMETypeAsHTML:MIMEType];
}

- (void)_stopLoadingWithError:(NSError *)error
{
    [[_private->loadState frameLoader] stopLoadingWithError:error];
}

- (BOOL)_loadingFromPageCache
{
    return _private->loadingFromPageCache;
}

-(void)_makeRepresentation
{
    Class repClass = [[self class] _representationClassForMIMEType:[[self response] MIMEType]];
    
    // Check if the data source was already bound?
    if (![[self representation] isKindOfClass:repClass]) {
        id newRep = repClass != nil ? [[repClass alloc] init] : nil;
        [self _setRepresentation:(id <WebDocumentRepresentation>)newRep];
        [newRep release];
    }
    
    [_private->representation setDataSource:self];
}

- (NSURL *)_URLForHistory
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates 
    // for unreachable URLs, because these can't be stored in history.
    NSURL *URL = [[_private->loadState originalRequestCopy] URL];
    if ([WebDataProtocol _webIsDataProtocolURL:URL])
        URL = [[_private->loadState originalRequestCopy] _webDataRequestUnreachableURL];
    
    return [URL _webkit_canonicalize];
}

- (void)_addToUnarchiveState:(WebArchive *)archive
{
    if (!_private->unarchivingState)
        _private->unarchivingState = [[WebUnarchivingState alloc] init];
    [_private->unarchivingState addArchive:archive];
}

- (WebDocumentLoadState *)_documentLoadState
{
    return _private->loadState;
}

- (id)_initWithDocumentLoadState:(WebDocumentLoadStateMac *)loadState
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _private = [[WebDataSourcePrivate alloc] init];
    
    _private->loadState = [loadState retain];
        
    LOG(Loading, "creating datasource for %@", [[_private->loadState request] URL]);
    WKSupportsMultipartXMixedReplace([_private->loadState request]);
    
    ++WebDataSourceCount;
    
    return self;
    
}

@end

@implementation WebDataSource

-(id)initWithRequest:(NSURLRequest *)request
{
    return [self _initWithDocumentLoadState:[[WebDocumentLoadState alloc] initWithRequest:request]];
}

- (void)dealloc
{
    ASSERT([[_private->loadState frameLoader] activeDataSource] != self || ![[_private->loadState frameLoader] isLoading]);

    --WebDataSourceCount;
    
    [_private release];
    
    [super dealloc];
}

- (void)finalize
{
    --WebDataSourceCount;

    [super finalize];
}

- (NSData *)data
{
    return [_private->loadState mainResourceData];
}

- (id <WebDocumentRepresentation>)representation
{
    return _private->representation;
}

- (WebFrame *)webFrame
{
    return [[_private->loadState frameLoader] webFrame];
}

-(NSURLRequest *)initialRequest
{
    NSURLRequest *clientRequest = [[_private->loadState originalRequest] _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = [_private->loadState originalRequest];
    return clientRequest;
}

-(NSMutableURLRequest *)request
{
    return [_private->loadState request];
}

- (NSURLResponse *)response
{
    return [_private->loadState response];
}

- (NSString *)textEncodingName
{
    NSString *textEncodingName = [_private->loadState overrideEncoding];

    if (!textEncodingName)
        textEncodingName = [[self response] textEncodingName];

    return textEncodingName;
}

- (BOOL)isLoading
{
    return [_private->loadState isLoadingInAPISense];
}

// Returns nil or the page title.
- (NSString *)pageTitle
{
    return [[self representation] title];
}

- (NSURL *)unreachableURL
{
    return [[_private->loadState originalRequest] _webDataRequestUnreachableURL];
}

- (WebArchive *)webArchive
{
    // it makes no sense to grab a WebArchive from an uncommitted document.
    if (![_private->loadState isCommitted])
        return nil;

    return [WebArchiver archiveFrame:[self webFrame]];
}

- (WebResource *)mainResource
{
    NSURLResponse *response = [self response];
    return [[[WebResource alloc] initWithData:[self data]
                                          URL:[response URL] 
                                     MIMEType:[response MIMEType]
                             textEncodingName:[response textEncodingName]
                                    frameName:[[self webFrame] name]] autorelease];
}

- (NSArray *)subresources
{
    NSArray *datas;
    NSArray *responses;
    [[self _bridge] getAllResourceDatas:&datas andResponses:&responses];
    ASSERT([datas count] == [responses count]);

    NSMutableArray *subresources = [[NSMutableArray alloc] initWithCapacity:[datas count]];
    for (unsigned i = 0; i < [datas count]; ++i) {
        NSURLResponse *response = [responses objectAtIndex:i];
        [subresources addObject:[[[WebResource alloc] _initWithData:[datas objectAtIndex:i] URL:[response URL] response:response] autorelease]];
    }

    return [subresources autorelease];
}

- (WebResource *)subresourceForURL:(NSURL *)URL
{
    NSData *data;
    NSURLResponse *response;
    if (![[self _bridge] getData:&data andResponse:&response forURL:URL])
        return nil;

    return [[[WebResource alloc] _initWithData:data URL:URL response:response] autorelease];
}

- (void)addSubresource:(WebResource *)subresource
{
    if (subresource) {
        if (!_private->unarchivingState)
            _private->unarchivingState = [[WebUnarchivingState alloc] init];
        [_private->unarchivingState addResource:subresource];
    }
}

@end
