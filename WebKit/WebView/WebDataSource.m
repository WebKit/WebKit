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
#import "WebDocumentLoadState.h"

@interface WebDataSourcePrivate : NSObject
{
    @public
    
    WebDocumentLoadState *loadState;
    
    id <WebDocumentRepresentation> representation;
    
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
    
    // The time when the data source was told to start loading.
    double loadingStartedTime;
    
    BOOL primaryLoadComplete;
    
    BOOL stopping;
    
    BOOL isClientRedirect;
    
    NSString *pageTitle;
    
    NSString *encoding;
    NSString *overrideEncoding;
    
    // Error associated with main document.
    NSError *mainDocumentError;
    
    BOOL loading; // self and webView are retained while loading
    
    BOOL gotFirstByte; // got first byte
    BOOL committed; // This data source has been committed
    BOOL representationFinishedLoading;
    
    BOOL defersCallbacks;
        
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
    
    BOOL loadingFromPageCache;
    
    WebFrame *webFrame;
    
    NSMutableDictionary *subresources;
    
    WebUnarchivingState *unarchivingState;
    
    BOOL supportsMultipartContent;
}

@end

@implementation WebDataSourcePrivate 

- (void)dealloc
{
    ASSERT(!loading);

    [loadState release];
    
    [representation release];
    [request release];
    [originalRequest release];
    [originalRequestCopy release];
    [pageTitle release];
    [response release];
    [mainDocumentError release];
    [triggeringAction release];
    [lastCheckedRequest release];
    [responses release];
    [webFrame release];
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

- (NSError *)_cancelledError
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                      code:NSURLErrorCancelled
                                       URL:[self _URL]];
}

- (void)_setMainDocumentError: (NSError *)error
{
    [error retain];
    [_private->mainDocumentError release];
    _private->mainDocumentError = error;
    
    if (!_private->representationFinishedLoading) {
        _private->representationFinishedLoading = YES;
        [[self representation] receivedError:error withDataSource:self];
    }
}

- (void)_clearErrors
{
    [_private->mainDocumentError release];
    _private->mainDocumentError = nil;
}

- (void)_prepareForLoadStart
{
    ASSERT(![self _isStopping]);
    [self _setPrimaryLoadComplete:NO];
    ASSERT([self webFrame] != nil);
    [self _clearErrors];
    
    // Mark the start loading time.
    _private->loadingStartedTime = CFAbsoluteTimeGetCurrent();
    
    [self _setLoading:YES];
    [[self _webView] _progressStarted:[self webFrame]];
    [[self _webView] _didStartProvisionalLoadForFrame:[self webFrame]];
    [[[self _webView] _frameLoadDelegateForwarder] webView:[self _webView]
                                     didStartProvisionalLoadForFrame:[self webFrame]];
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

- (void)_commitIfReady
{
    if (_private->gotFirstByte && !_private->committed) {
        _private->committed = TRUE;
        [[self webFrame] _commitProvisionalLoad:nil];
    }
}

- (void)_commitLoadWithData:(NSData *)data
{
    // Both unloading the old page and parsing the new page may execute JavaScript which destroys the datasource
    // by starting a new load, so retain temporarily.
    [self retain];
    [self _commitIfReady];
    [[self representation] receivedData:data withDataSource:self];
    [[[[self webFrame] frameView] documentView] dataSourceUpdated:self];
    [self release];
}

- (BOOL)_doesProgressiveLoadWithMIMEType:(NSString *)MIMEType
{
    return [[self webFrame] _loadType] != WebFrameLoadTypeReplace || [MIMEType isEqualToString:@"text/html"];
}

- (void)_addResponse:(NSURLResponse *)r
{
    if (!_private->stopRecordingResponses) {
        if (!_private->responses)
            _private->responses = [[NSMutableArray alloc] init];
        [_private->responses addObject: r];
    }
}

- (void)_revertToProvisionalState
{
    [self _setRepresentation:nil];
    [[_private->webFrame _frameLoader] setupForReplace];
    _private->committed = NO;
}

@end

@implementation WebDataSource (WebPrivate)

- (NSError *)_mainDocumentError
{
    return _private->mainDocumentError;
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
    if (resource) {
        return [resource _fileWrapperRepresentation];
    }
    
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

- (void)_finishedLoading
{
    _private->gotFirstByte = YES;
    [self _commitIfReady];
    
    _private->representationFinishedLoading = YES;
    [[self representation] finishedLoadingWithDataSource:self];
    [[self _bridge] end];
}

- (void)_setResponse:(NSURLResponse *)response
{
    [_private->response release];
    _private->response = [response retain];
}

- (void)_setRequest:(NSURLRequest *)request
{
    ASSERT_ARG(request, request != _private->request);
    
    // Replacing an unreachable URL with alternate content looks like a server-side
    // redirect at this point, but we can replace a committed dataSource.
    BOOL handlingUnreachableURL = [request _webDataRequestUnreachableURL] != nil;
    if (handlingUnreachableURL) {
        _private->committed = NO;
    }
    
    // We should never be getting a redirect callback after the data
    // source is committed, except in the unreachable URL case. It 
    // would be a WebFoundation bug if it sent a redirect callback after commit.
    ASSERT(!_private->committed);
    
    NSURLRequest *oldRequest = _private->request;
    
    _private->request = [request mutableCopy];
    
    // Only send webView:didReceiveServerRedirectForProvisionalLoadForFrame: if URL changed.
    // Also, don't send it when replacing unreachable URLs with alternate content.
    if (!handlingUnreachableURL && ![[oldRequest URL] isEqual: [request URL]]) {
        LOG(Redirect, "Server redirect to: %@", [request URL]);
        [[[self _webView] _frameLoadDelegateForwarder] webView:[self _webView]
            didReceiveServerRedirectForProvisionalLoadForFrame:[self webFrame]];
    }
    
    [oldRequest release];
}

- (void)_setupForReplaceByMIMEType:(NSString *)newMIMEType
{
    if (!_private->gotFirstByte)
        return;
    
    WebFrame *frame = [self webFrame];
    NSString *oldMIMEType = [[self response] MIMEType];
    
    if (![self _doesProgressiveLoadWithMIMEType:oldMIMEType]) {
        [self _revertToProvisionalState];
        [self _commitLoadWithData:[self data]];
    }
    
    _private->representationFinishedLoading = YES;
    [[self representation] finishedLoadingWithDataSource:self];
    [[self _bridge] end];
    
    [frame _setLoadType:WebFrameLoadTypeReplace];
    _private->gotFirstByte = NO;
    
    if ([self _doesProgressiveLoadWithMIMEType:newMIMEType])
        [self _revertToProvisionalState];
    
    [[_private->webFrame _frameLoader] stopLoadingSubresources];
    [[_private->webFrame _frameLoader] stopLoadingPlugIns];
    [_private->unarchivingState release];
}

-(void)_receivedData:(NSData *)data
{    
    _private->gotFirstByte = YES;
    
    if ([self _doesProgressiveLoadWithMIMEType:[[self response] MIMEType]])
        [self _commitLoadWithData:data];
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
    [self _mainReceivedError:error complete:isComplete];
}

- (void)_mainReceivedError:(NSError *)error complete:(BOOL)isComplete
{
    if (![self webFrame])
        return;
    
    [self _setMainDocumentError:error];
    
    if (isComplete) {
        [self _setPrimaryLoadComplete:YES];
        [[self webFrame] _checkLoadComplete];
    }
}

- (BOOL)_defersCallbacks
{
    return _private->defersCallbacks;
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
    
    [self _addResponse:r];
    
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

- (void)_setLoading:(BOOL)loading
{
    _private->loading = loading;
}

- (void)_updateLoading
{
    WebFrameLoader *frameLoader = [_private->webFrame _frameLoader];
    ASSERT(self == [frameLoader activeDataSource]);

    [self _setLoading:[frameLoader isLoading]];
}

- (void)_startLoading
{
    [self _prepareForLoadStart];
    
    if ([[_private->webFrame _frameLoader] isLoadingMainResource])
        return;
    
    _private->loadingFromPageCache = NO;
    
    id identifier;
    id resourceLoadDelegate = [[self _webView] resourceLoadDelegate];
    if ([resourceLoadDelegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)])
        identifier = [resourceLoadDelegate webView:[self _webView] identifierForInitialRequest:_private->originalRequest fromDataSource:self];
    else
        identifier = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:[self _webView] identifierForInitialRequest:_private->originalRequest fromDataSource:self];
    
    if (![[_private->webFrame _frameLoader] startLoadingMainResourceWithRequest:_private->request identifier:identifier])
        [self _updateLoading];
}

- (void)_stopRecordingResponses
{
    _private->stopRecordingResponses = YES;
}

- (double)_loadingStartedTime
{
    return _private->loadingStartedTime;
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

- (NSString *)_title
{
    return _private->pageTitle;
}

- (BOOL)_isStopping
{
    return _private->stopping;
}

- (void)_setWebFrame:(WebFrame *)frame
{
    [frame retain];
    [_private->webFrame release];
    _private->webFrame = frame;

    [_private->loadState setFrameLoader:[frame _frameLoader]];
    
    [self _defersCallbacksChanged];
    // no need to do _defersCallbacksChanged for subframes since they too
    // will be or have been told of their WebFrame
}

// May return nil if not initialized with a URL.
- (NSURL *)_URL
{
    return [[self request] URL];
}

- (void)_loadFromPageCache:(NSDictionary *)pageCache
{
    [self _prepareForLoadStart];
    _private->loadingFromPageCache = YES;
    _private->committed = TRUE;
    [[self webFrame] _commitProvisionalLoad:pageCache];
}

- (WebArchive *)_popSubframeArchiveWithName:(NSString *)frameName
{
    return [_private->unarchivingState popSubframeArchiveWithFrameName:frameName];
}

- (void)_setIsClientRedirect:(BOOL)flag
{
    _private->isClientRedirect = flag;
}

- (void)_setURL:(NSURL *)URL
{
    NSMutableURLRequest *newOriginalRequest = [_private->originalRequestCopy mutableCopy];
    [_private->originalRequestCopy release];
    [newOriginalRequest setURL:URL];
    _private->originalRequestCopy = newOriginalRequest;
    
    NSMutableURLRequest *newRequest = [_private->request mutableCopy];
    [_private->request release];
    [newRequest setURL:URL];
    _private->request = newRequest;
}

- (void)_setLastCheckedRequest:(NSURLRequest *)request
{
    NSURLRequest *oldRequest = _private->lastCheckedRequest;
    _private->lastCheckedRequest = [request copy];
    [oldRequest release];
}

- (void)_defersCallbacksChanged
{
    BOOL defers = [[self _webView] defersCallbacks];
    
    if (defers == _private->defersCallbacks) {
        return;
    }
    
    _private->defersCallbacks = defers;
    [[_private->webFrame _frameLoader] setDefersCallbacks:defers];
}

- (NSURLRequest *)_lastCheckedRequest
{
    // It's OK not to make a copy here because we know the caller
    // isn't going to modify this request
    return [[_private->lastCheckedRequest retain] autorelease];
}

// Cancels the data source's pending loads.  Conceptually, a data source only loads
// one document at a time, but one document may have many related resources. 
// _stopLoading will stop all loads initiated by the data source, 
// but not loads initiated by child frames' data sources -- that's the WebFrame's job.
- (void)_stopLoading
{
    // Always attempt to stop the bridge/part because it may still be loading/parsing after the data source
    // is done loading and not stopping it can cause a world leak.
    if (_private->committed)
        [[self _bridge] stopLoading];
    
    if (!_private->loading)
        return;
    
    [self retain];
    
    _private->stopping = YES;
    
    if ([[_private->webFrame _frameLoader] isLoadingMainResource]) {
        // Stop the main resource loader and let it send the cancelled message.
        [[_private->webFrame _frameLoader] cancelMainResourceLoad];
    } else if ([[_private->webFrame _frameLoader] isLoadingSubresources]) {
        // The main resource loader already finished loading. Set the cancelled error on the 
        // document and let the subresourceLoaders send individual cancelled messages below.
        [self _setMainDocumentError:[self _cancelledError]];
    } else {
        // If there are no resource loaders, we need to manufacture a cancelled message.
        // (A back/forward navigation has no resource loaders because its resources are cached.)
        [self _mainReceivedError:[self _cancelledError] complete:YES];
    }
    
    [[_private->webFrame _frameLoader] stopLoadingSubresources];
    [[_private->webFrame _frameLoader] stopLoadingPlugIns];
    
    _private->stopping = NO;
    
    [self release];
}

- (WebFrameBridge *)_bridge
{
    ASSERT(_private->committed);
    return [[self webFrame] _bridge];
}

- (WebView *)_webView
{
    return [_private->webFrame webView];
}

- (NSDictionary *)_triggeringAction
{
    return [[_private->triggeringAction retain] autorelease];
}

- (void)_setTriggeringAction:(NSDictionary *)action
{
    [action retain];
    [_private->triggeringAction release];
    _private->triggeringAction = action;
}

- (void)__adoptRequest:(NSMutableURLRequest *)request
{
    if (request != _private->request){
        [_private->request release];
        _private->request = [request retain];
    }
}

- (BOOL)_isDocumentHTML
{
    NSString *MIMEType = [[self response] MIMEType];
    return [WebView canShowMIMETypeAsHTML:MIMEType];
}

- (void)_stopLoadingWithError:(NSError *)error
{
    [[_private->webFrame _frameLoader] stopLoadingWithError:error];
}

- (void)_setPrimaryLoadComplete:(BOOL)flag
{
    _private->primaryLoadComplete = flag;
    
    if (flag) {
        if ([[_private->webFrame _frameLoader] isLoadingMainResource]) {
            [_private->loadState setMainResourceData:[[_private->webFrame _frameLoader] mainResourceData]];
            [[_private->webFrame _frameLoader] releaseMainResourceLoader];
        }
        
        [self _updateLoading];

        if ([WebScriptDebugServer listenerCount])
            [[WebScriptDebugServer sharedScriptDebugServer] webView:[[self webFrame] webView] didLoadMainResourceForDataSource:self];
    }
}

- (NSArray *)_responses
{
    return _private->responses;
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

- (BOOL)_isClientRedirect
{
    return _private->isClientRedirect;
}


- (NSURLRequest *)_originalRequest
{
    return _private->originalRequestCopy;
}

- (NSURL *)_URLForHistory
{
    // Return the URL to be used for history and B/F list.
    // Returns nil for WebDataProtocol URLs that aren't alternates 
    // for unreachable URLs, because these can't be stored in history.
    NSURL *URL = [_private->originalRequestCopy URL];
    if ([WebDataProtocol _webIsDataProtocolURL:URL]) {
        URL = [_private->originalRequestCopy _webDataRequestUnreachableURL];
    }
    
    return [URL _webkit_canonicalize];
}

- (void)_addToUnarchiveState:(WebArchive *)archive
{
    if (!_private->unarchivingState)
        _private->unarchivingState = [[WebUnarchivingState alloc] init];
    [_private->unarchivingState addArchive:archive];
}

- (void)_setOverrideEncoding:(NSString *)overrideEncoding
{
    NSString *copy = [overrideEncoding copy];
    [_private->overrideEncoding release];
    _private->overrideEncoding = copy;
}

- (NSString *)_overrideEncoding
{
    return [[_private->overrideEncoding copy] autorelease];
}

- (void)_setTitle:(NSString *)title
{
    NSString *trimmed;
    if (title == nil) {
        trimmed = nil;
    } else {
        trimmed = [title _webkit_stringByTrimmingWhitespace];
        if ([trimmed length] == 0)
            trimmed = nil;
    }
    if (trimmed == nil) {
        if (_private->pageTitle == nil)
            return;
    } else {
        if ([_private->pageTitle isEqualToString:trimmed])
            return;
    }
    
    if (!trimmed || [trimmed length] == 0)
        return;
    
    [[self _webView] _willChangeValueForKey:_WebMainFrameTitleKey];
    [_private->pageTitle release];
    _private->pageTitle = [trimmed copy];
    [[self _webView] _didChangeValueForKey:_WebMainFrameTitleKey];
    
    // The title doesn't get communicated to the WebView until we are committed.
    if (_private->committed) {
        NSURL *URLForHistory = [self _URLForHistory];
        if (URLForHistory != nil) {
            WebHistoryItem *entry = [[WebHistory optionalSharedHistory] itemForURL:URLForHistory];
            [entry setTitle: _private->pageTitle];
            
            // Must update the entries in the back-forward list too.  This must go through the WebFrame because
            // it has the right notion of the current b/f item.
            [[self webFrame] _setTitle:_private->pageTitle];
            
            [[self _webView] setMainFrameDocumentReady:YES];    // update observers with new DOMDocument
            [[[self _webView] _frameLoadDelegateForwarder] webView:[self _webView]
                                                   didReceiveTitle:_private->pageTitle
                                                          forFrame:[self webFrame]];
        }
    }
}

@end

@implementation WebDataSource

-(id)initWithRequest:(NSURLRequest *)request
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _private = [[WebDataSourcePrivate alloc] init];
    
    _private->loadState = [[WebDocumentLoadState alloc] initWithRequest:request];
    
    _private->originalRequest = [request retain];
    _private->originalRequestCopy = [request copy];
    
    LOG(Loading, "creating datasource for %@", [request URL]);
    _private->request = [_private->originalRequest mutableCopy];
    _private->supportsMultipartContent = WKSupportsMultipartXMixedReplace(_private->request);

    ++WebDataSourceCount;
    
    return self;
}

- (void)dealloc
{
    ASSERT([[_private->webFrame _frameLoader] activeDataSource] != self || ![[_private->webFrame _frameLoader] isLoading]);

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
    return _private->webFrame;
}

-(NSURLRequest *)initialRequest
{
    NSURLRequest *clientRequest = [_private->originalRequest _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = _private->originalRequest;
    return clientRequest;
}

-(NSMutableURLRequest *)request
{
    NSMutableURLRequest *clientRequest = [_private->request _webDataRequestExternalRequest];
    if (!clientRequest)
        clientRequest = _private->request;
    return clientRequest;
}

- (NSURLResponse *)response
{
    return _private->response;
}

- (NSString *)textEncodingName
{
    NSString *textEncodingName = [self _overrideEncoding];

    if (!textEncodingName)
        textEncodingName = [[self response] textEncodingName];

    return textEncodingName;
}

// Returns YES if there are any pending loads.
- (BOOL)isLoading
{
    // Once a frame has loaded, we no longer need to consider subresources,
    // but we still need to consider subframes.
    if ([[[self webFrame] _frameLoader] state] != WebFrameStateComplete) {
        if (!_private->primaryLoadComplete && _private->loading)
            return YES;
        if ([[_private->webFrame _frameLoader] isLoadingSubresources])
            return YES;
        if (![[[self webFrame] _bridge] doneProcessingData])
            return YES;
    }

    return [[self webFrame] _subframeIsLoading];
}

// Returns nil or the page title.
- (NSString *)pageTitle
{
    return [[self representation] title];
}

- (NSURL *)unreachableURL
{
    return [_private->originalRequest _webDataRequestUnreachableURL];
}

- (WebArchive *)webArchive
{
    // it makes no sense to grab a WebArchive from an uncommitted document.
    if (!_private->committed)
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
