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

#import "WebFrameLoader.h"

#import "WebDataProtocol.h"
#import "WebDataSourceInternal.h"
#import "WebDocumentLoaderMac.h"
#import "WebDownloadInternal.h"
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegate.h"
#import "WebHistory.h"
#import "WebIconDatabasePrivate.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebMainResourceLoader.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSURLExtras.h"
#import "WebPreferences.h"
#import "WebResourcePrivate.h"
#import "WebResourceLoadDelegate.h"
#import "WebDefaultResourceLoadDelegate.h"
#import "WebScriptDebugServerPrivate.h"
#import "WebViewInternal.h"
#import <JavaScriptCore/Assertions.h>
#import <WebKit/DOMHTML.h>

@implementation WebFrameLoader

- (id)initWithClient:(WebFrame <WebFrameLoaderClient> *)c
{
    self = [super init];
    if (self) {
        client = c;
        state = WebFrameStateCommittedPage;
    }
    return self;    
}

- (void)dealloc
{
    // FIXME: should these even exist?
    [mainResourceLoader release];
    [subresourceLoaders release];
    [plugInStreamLoaders release];
    [documentLoader release];
    [provisionalDocumentLoader release];
 
    ASSERT(!policyDocumentLoader);
    
    [super dealloc];
}

- (WebDocumentLoader *)activeDocumentLoader
{
    if (state == WebFrameStateProvisional)
        return provisionalDocumentLoader;
    
    return documentLoader;    
}

- (WebDataSource *)activeDataSource
{
    return [client _dataSourceForDocumentLoader:[self activeDocumentLoader]];
}

- (WebResource *)_archivedSubresourceForURL:(NSURL *)URL
{
    return [[self activeDataSource] _archivedSubresourceForURL:URL];
}

- (void)addPlugInStreamLoader:(WebLoader *)loader
{
    if (!plugInStreamLoaders)
        plugInStreamLoaders = [[NSMutableArray alloc] init];
    [plugInStreamLoaders addObject:loader];
    [[self activeDocumentLoader] setLoading:YES];
}

- (void)removePlugInStreamLoader:(WebLoader *)loader
{
    [plugInStreamLoaders removeObject:loader];
    [[self activeDocumentLoader] updateLoading];
}    

- (void)defersCallbacksChanged
{
    [self setDefersCallbacks:[[client webView] defersCallbacks]];
}

- (BOOL)defersCallbacks
{
    return [[client webView] defersCallbacks];
}

- (void)setDefersCallbacks:(BOOL)defers
{
    [mainResourceLoader setDefersCallbacks:defers];
    
    NSEnumerator *e = [subresourceLoaders objectEnumerator];
    WebLoader *loader;
    while ((loader = [e nextObject]))
        [loader setDefersCallbacks:defers];
    
    e = [plugInStreamLoaders objectEnumerator];
    while ((loader = [e nextObject]))
        [loader setDefersCallbacks:defers];

    [self deliverArchivedResourcesAfterDelay];
}

- (void)stopLoadingPlugIns
{
    [plugInStreamLoaders makeObjectsPerformSelector:@selector(cancel)];
    [plugInStreamLoaders removeAllObjects];   
}

- (BOOL)isLoadingMainResource
{
    return mainResourceLoader != nil;
}

- (BOOL)isLoadingSubresources
{
    return [subresourceLoaders count];
}

- (BOOL)isLoadingPlugIns
{
    return [plugInStreamLoaders count];
}

- (BOOL)isLoading
{
    return [self isLoadingMainResource] || [self isLoadingSubresources] || [self isLoadingPlugIns];
}

- (void)stopLoadingSubresources
{
    NSArray *loaders = [subresourceLoaders copy];
    [loaders makeObjectsPerformSelector:@selector(cancel)];
    [loaders release];
    [subresourceLoaders removeAllObjects];
}

- (void)addSubresourceLoader:(WebLoader *)loader
{
    ASSERT(!provisionalDocumentLoader);
    if (subresourceLoaders == nil)
        subresourceLoaders = [[NSMutableArray alloc] init];
    [subresourceLoaders addObject:loader];
    [[self activeDocumentLoader] setLoading:YES];
}

- (void)removeSubresourceLoader:(WebLoader *)loader
{
    [subresourceLoaders removeObject:loader];
    [[self activeDocumentLoader] updateLoading];
}

- (NSData *)mainResourceData
{
    return [mainResourceLoader resourceData];
}

- (void)releaseMainResourceLoader
{
    [mainResourceLoader release];
    mainResourceLoader = nil;
}

- (void)cancelMainResourceLoad
{
    [mainResourceLoader cancel];
}

- (BOOL)startLoadingMainResourceWithRequest:(NSMutableURLRequest *)request identifier:(id)identifier
{
    mainResourceLoader = [[WebMainResourceLoader alloc] initWithFrameLoader:self];
    
    [mainResourceLoader setIdentifier:identifier];
    [client _addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:NO];
    if (![mainResourceLoader loadWithRequest:request]) {
        // FIXME: if this should really be caught, we should just ASSERT this doesn't happen;
        // should it be caught by other parts of WebKit or other parts of the app?
        LOG_ERROR("could not create WebResourceHandle for URL %@ -- should be caught by policy handler level", [request URL]);
        [mainResourceLoader release];
        mainResourceLoader = nil;
        return NO;
    }
    
    return YES;
}

- (void)stopLoadingWithError:(NSError *)error
{
    [mainResourceLoader cancelWithError:error];
}

- (WebDataSource *)dataSource
{
    return [client _dataSourceForDocumentLoader:documentLoader]; 
}

- (void)setDocumentLoader:(WebDocumentLoader *)loader
{
    if (loader == nil && documentLoader == nil)
        return;
    
    ASSERT(loader != documentLoader);
    
    [client _prepareForDataSourceReplacement];
    [documentLoader detachFromFrameLoader];
    
    [loader retain];
    [documentLoader release];
    documentLoader = loader;
}

- (WebDocumentLoader *)documentLoader
{
    return documentLoader;
}

- (WebDataSource *)policyDataSource
{
    return [client _dataSourceForDocumentLoader:policyDocumentLoader];     
}

- (void)setPolicyDocumentLoader:(WebDocumentLoader *)loader
{
    if (policyDocumentLoader == loader)
        return;

    if (policyDocumentLoader != provisionalDocumentLoader && policyDocumentLoader != documentLoader)
        [policyDocumentLoader detachFromFrameLoader];

    [policyDocumentLoader release];
    [loader retain];
    policyDocumentLoader = loader;
}
   
- (void)clearDataSource
{
    [self setDocumentLoader:nil];
}

- (WebDataSource *)provisionalDataSource 
{
    return [client _dataSourceForDocumentLoader:provisionalDocumentLoader]; 
}

- (WebDocumentLoader *)provisionalDocumentLoader
{
    return provisionalDocumentLoader;
}

- (void)setProvisionalDocumentLoader:(WebDocumentLoader *)loader
{
    ASSERT(!loader || !provisionalDocumentLoader);

    if (provisionalDocumentLoader != documentLoader)
        [provisionalDocumentLoader detachFromFrameLoader];

    [loader retain];
    [provisionalDocumentLoader release];
    provisionalDocumentLoader = loader;
}

- (void)_clearProvisionalDataSource
{
    [self setProvisionalDocumentLoader:nil];
}

- (WebFrameState)state
{
    return state;
}

#ifndef NDEBUG
static const char * const stateNames[] = {
    "WebFrameStateProvisional",
    "WebFrameStateCommittedPage",
    "WebFrameStateComplete"
};
#endif

static CFAbsoluteTime _timeOfLastCompletedLoad;

+ (CFAbsoluteTime)timeOfLastCompletedLoad
{
    return _timeOfLastCompletedLoad;
}

- (void)_setState:(WebFrameState)newState
{
    LOG(Loading, "%@:  transition from %s to %s", [client name], stateNames[state], stateNames[newState]);
    if ([client webView])
        LOG(Timing, "%@:  transition from %s to %s, %f seconds since start of document load", [client name], stateNames[state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[[client webView] mainFrame] dataSource] _documentLoader] loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && client == [[client webView] mainFrame])
        LOG(DocumentLoad, "completed %@ (%f seconds)", [[[self dataSource] request] URL], CFAbsoluteTimeGetCurrent() - [[[self dataSource] _documentLoader] loadingStartedTime]);
    
    state = newState;
    
    if (state == WebFrameStateProvisional)
        [client _provisionalLoadStarted];
    else if (state == WebFrameStateComplete) {
        [client _frameLoadCompleted];
        _timeOfLastCompletedLoad = CFAbsoluteTimeGetCurrent();
        [[self documentLoader] stopRecordingResponses];
    }
}

- (void)clearProvisionalLoad
{
    [self setProvisionalDocumentLoader:nil];
    [[client webView] _progressCompleted:client];
    [self _setState:WebFrameStateComplete];
}

- (void)markLoadComplete
{
    [self _setState:WebFrameStateComplete];
}

- (void)commitProvisionalLoad
{
    [self stopLoadingSubresources];
    [self stopLoadingPlugIns];

    [self setDocumentLoader:provisionalDocumentLoader];
    [self setProvisionalDocumentLoader:nil];
    [self _setState:WebFrameStateCommittedPage];
}

- (void)stopLoading
{
    [[self provisionalDocumentLoader] stopLoading];
    [[self documentLoader] stopLoading];
    [self _clearProvisionalDataSource];
    [self clearArchivedResources];
}

// FIXME: poor method name; also why is this not part of startProvisionalLoad:?
- (void)startLoading
{
    [provisionalDocumentLoader prepareForLoadStart];
        
    if ([self isLoadingMainResource])
        return;
        
    [[self provisionalDataSource] _setLoadingFromPageCache:NO];
        
    id identifier;
    id resourceLoadDelegate = [[client webView] resourceLoadDelegate];
    if ([resourceLoadDelegate respondsToSelector:@selector(webView:identifierForInitialRequest:fromDataSource:)])
        identifier = [resourceLoadDelegate webView:[client webView] identifierForInitialRequest:[provisionalDocumentLoader originalRequest] fromDataSource:[self provisionalDataSource]];
    else
        identifier = [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:[client webView] identifierForInitialRequest:[provisionalDocumentLoader originalRequest] fromDataSource:[self provisionalDataSource]];
    
    if (![[provisionalDocumentLoader frameLoader] startLoadingMainResourceWithRequest:[provisionalDocumentLoader actualRequest] identifier:identifier])
        [provisionalDocumentLoader updateLoading];
}

- (void)startProvisionalLoad:(WebDataSource *)ds
{
    [self setProvisionalDocumentLoader:[ds _documentLoader]];
    [self _setState:WebFrameStateProvisional];
}

- (void)setupForReplace
{
    [self _setState:WebFrameStateProvisional];
    WebDocumentLoader *old = provisionalDocumentLoader;
    provisionalDocumentLoader = documentLoader;
    documentLoader = nil;
    [old release];
    
    [client _detachChildren];
}

- (id)_identifierForInitialRequest:(NSURLRequest *)clientRequest
{
    WebView *webView = [client webView];
        
    // The identifier is released after the last callback, rather than in dealloc
    // to avoid potential cycles.
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsIdentifierForRequest)
        return [[[webView resourceLoadDelegate] webView:webView identifierForInitialRequest:clientRequest fromDataSource:[self activeDataSource]] retain];
    else
        return [[[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView identifierForInitialRequest:clientRequest fromDataSource:[self activeDataSource]] retain];
}

- (NSURLRequest *)_willSendRequest:(NSMutableURLRequest *)clientRequest forResource:(id)identifier redirectResponse:(NSURLResponse *)redirectResponse
{
    WebView *webView = [client webView];
    
    [clientRequest setValue:[webView userAgentForURL:[clientRequest URL]] forHTTPHeaderField:@"User-Agent"];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsWillSendRequest)
        return [[webView resourceLoadDelegate] webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:[self activeDataSource]];
    else
        return [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDataSource:[self activeDataSource]];
}

- (void)_didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    WebView *webView = [client webView];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveAuthenticationChallenge)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:[self activeDataSource]];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveAuthenticationChallenge:currentWebChallenge fromDataSource:[self activeDataSource]];
}

- (void)_didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    WebView *webView = [client webView];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidCancelAuthenticationChallenge)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:[self activeDataSource]];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didCancelAuthenticationChallenge:currentWebChallenge fromDataSource:[self activeDataSource]];
    
}

- (void)_didReceiveResponse:(NSURLResponse *)r forResource:(id)identifier
{
    WebView *webView = [client webView];
    
    [[self activeDocumentLoader] addResponse:r];
    
    [webView _incrementProgressForIdentifier:identifier response:r];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveResponse)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveResponse:r fromDataSource:[self activeDataSource]];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveResponse:r fromDataSource:[self activeDataSource]];
}

- (void)_didReceiveData:(NSData *)data contentLength:(int)lengthReceived forResource:(id)identifier
{
    WebView *webView = [client webView];
    
    [webView _incrementProgressForIdentifier:identifier data:data];
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidReceiveContentLength)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didReceiveContentLength:(WebNSUInteger)lengthReceived fromDataSource:[self activeDataSource]];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didReceiveContentLength:(WebNSUInteger)lengthReceived fromDataSource:[self activeDataSource]];
}

- (void)_didFinishLoadingForResource:(id)identifier
{    
    WebView *webView = [client webView];
    
    [webView _completeProgressForIdentifier:identifier];    
    
    if ([webView _resourceLoadDelegateImplementations].delegateImplementsDidFinishLoadingFromDataSource)
        [[webView resourceLoadDelegate] webView:webView resource:identifier didFinishLoadingFromDataSource:[self activeDataSource]];
    else
        [[WebDefaultResourceLoadDelegate sharedResourceLoadDelegate] webView:webView resource:identifier didFinishLoadingFromDataSource:[self activeDataSource]];
}

- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier
{
    WebView *webView = [client webView];
        
    [webView _completeProgressForIdentifier:identifier];
        
    if (error)
        [[webView _resourceLoadDelegateForwarder] webView:webView resource:identifier didFailLoadingWithError:error fromDataSource:[self activeDataSource]];
}

- (BOOL)_privateBrowsingEnabled
{
    return [[[client webView] preferences] privateBrowsingEnabled];
}

- (void)_finishedLoadingResource
{
    [client _checkLoadComplete];
}

- (void)_receivedError:(NSError *)error
{
    [client _checkLoadComplete];
}

- (NSURLRequest *)_originalRequest
{
    return [[self activeDocumentLoader] originalRequestCopy];
}

- (WebFrame *)webFrame
{
    return client;
}

- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete
{
    WebDocumentLoader *loader = [self activeDocumentLoader];
    [loader retain];
    
    WebFrameBridge *bridge = [client _bridge];
    
    // Retain the bridge because the stop may release the last reference to it.
    [bridge retain];
 
    WebFrame *cli = [client retain];
   
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
    
    [cli _receivedMainResourceError:error];
    [loader mainReceivedError:error complete:isComplete];

    [cli release];

    [loader release];
}

- (NSURLRequest *)initialRequest
{
    return [[self activeDataSource] initialRequest];
}

- (void)_receivedData:(NSData *)data
{
    [[self activeDocumentLoader] receivedData:data];
}

- (void)_setRequest:(NSURLRequest *)request
{
    [[self activeDocumentLoader] setRequest:request];
}

- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)r proxy:(id)proxy
{
    [WebDownload _downloadWithLoadingConnection:connection
                                        request:request
                                       response:r
                                       delegate:[[client webView] downloadDelegate]
                                          proxy:proxy];
}

- (WebFrameBridge *)bridge
{
    return [client _bridge];
}

- (void)_handleFallbackContent
{
    [[self bridge] handleFallbackContent];
}

- (BOOL)_isStopping
{
    return [[self activeDocumentLoader] isStopping];
}

- (void)_setupForReplaceByMIMEType:(NSString *)newMIMEType
{
    [[self activeDocumentLoader] setupForReplaceByMIMEType:newMIMEType];
}

- (void)_setResponse:(NSURLResponse *)response
{
    [[self activeDocumentLoader] setResponse:response];
}

- (void)_mainReceivedError:(NSError *)error complete:(BOOL)isComplete
{
    [[self activeDocumentLoader] mainReceivedError:error complete:isComplete];
}

- (void)_finishedLoading
{
    WebDataSource *ds = [self activeDataSource];
    
    [self retain];
    [[self activeDocumentLoader] finishedLoading];

    if ([ds _mainDocumentError] || ![ds webFrame]) {
        [self release];
        return;
    }

    [[self activeDocumentLoader] setPrimaryLoadComplete:YES];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:[client webView] didLoadMainResourceForDataSource:[self activeDataSource]];
    [client _checkLoadComplete];

    [self release];
}

- (void)_notifyIconChanged:(NSURL *)iconURL
{
    ASSERT([[WebIconDatabase sharedIconDatabase] _isEnabled]);
    ASSERT(client == [[client webView] mainFrame]);

    [[client webView] _willChangeValueForKey:_WebMainFrameIconKey];
    
    NSImage *icon = [[WebIconDatabase sharedIconDatabase] iconForURL:[[[self activeDataSource] _URL] _web_originalDataAsString] withSize:WebIconSmallSize];
    
    [[[client webView] _frameLoadDelegateForwarder] webView:[client webView]
                                               didReceiveIcon:icon
                                                     forFrame:client];
    
    [[client webView] _didChangeValueForKey:_WebMainFrameIconKey];
}

- (NSURL *)_URL
{
    return [[self activeDataSource] _URL];
}

- (NSError *)cancelledErrorWithRequest:(NSURLRequest *)request
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                      code:NSURLErrorCancelled
                                       URL:[request URL]];
}

- (NSError *)fileDoesNotExistErrorWithResponse:(NSURLResponse *)response
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                                code:NSURLErrorFileDoesNotExist
                                                 URL:[response URL]];    
}

- (void)clearArchivedResources
{
    [pendingArchivedResources removeAllObjects];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(deliverArchivedResources) object:nil];
}

- (void)deliverArchivedResources
{
    if (![pendingArchivedResources count] || [self defersCallbacks])
        return;
        
    NSEnumerator *keyEnum = [pendingArchivedResources keyEnumerator];
    WebLoader *loader;
    while ((loader = [keyEnum nextObject])) {
        WebResource *resource = [pendingArchivedResources objectForKey:loader];
        [loader didReceiveResponse:[resource _response]];
        NSData *data = [resource data];
        [loader didReceiveData:data lengthReceived:[data length] allAtOnce:YES];
        [loader didFinishLoading];
    }
    
    [pendingArchivedResources removeAllObjects];
}

- (void)deliverArchivedResourcesAfterDelay
{
    if (![pendingArchivedResources count] || [self defersCallbacks])
        return;
    
    [self performSelector:@selector(deliverArchivedResources) withObject:nil afterDelay:0];
}

static BOOL isCaseInsensitiveEqual(NSString *a, NSString *b)
{
    return [a caseInsensitiveCompare:b] == NSOrderedSame;
}

// The following 2 methods are copied from [NSHTTPURLProtocol _cachedResponsePassesValidityChecks] and modified for our needs.
// FIXME: It would be nice to eventually to share this code somehow.
- (BOOL)_canUseResourceForRequest:(NSURLRequest *)theRequest
{
    NSURLRequestCachePolicy policy = [theRequest cachePolicy];
    
    if (policy == NSURLRequestReturnCacheDataElseLoad) {
        return YES;
    } else if (policy == NSURLRequestReturnCacheDataDontLoad) {
        return YES;
    } else if (policy == NSURLRequestReloadIgnoringCacheData) {
        return NO;
    } else if ([theRequest valueForHTTPHeaderField:@"must-revalidate"] != nil) {
        return NO;
    } else if ([theRequest valueForHTTPHeaderField:@"proxy-revalidate"] != nil) {
        return NO;
    } else if ([theRequest valueForHTTPHeaderField:@"If-Modified-Since"] != nil) {
        return NO;
    } else if ([theRequest valueForHTTPHeaderField:@"Cache-Control"] != nil) {
        return NO;
    } else if (isCaseInsensitiveEqual(@"POST", [theRequest HTTPMethod])) {
        return NO;
    } else {
        return YES;
    }
}

- (BOOL)_canUseResourceWithResponse:(NSURLResponse *)theResponse
{
    if (WKGetNSURLResponseMustRevalidate(theResponse)) {
        return NO;
    } else if (WKGetNSURLResponseCalculatedExpiration(theResponse) - CFAbsoluteTimeGetCurrent() < 1) {
        return NO;
    } else {
        return YES;
    }
}

- (NSMutableDictionary *)pendingArchivedResources
{
    if (!pendingArchivedResources)
        pendingArchivedResources = [[NSMutableDictionary alloc] init];
    
    return pendingArchivedResources;
}

- (BOOL)willUseArchiveForRequest:(NSURLRequest *)r originalURL:(NSURL *)originalURL loader:(WebLoader *)loader
{
    if ([[r URL] isEqual:originalURL] && [self _canUseResourceForRequest:r]) {
        WebResource *resource = [self _archivedSubresourceForURL:originalURL];
        if (resource && [self _canUseResourceWithResponse:[resource _response]]) {
            [[self pendingArchivedResources] _webkit_setObject:resource forUncopiedKey:loader];
            // Deliver the resource after a delay because callers don't expect to receive callbacks while calling this method.
            [self deliverArchivedResourcesAfterDelay];
            return YES;
        }
    }
    return NO;
}

- (BOOL)archiveLoadPendingForLoader:(WebLoader *)loader
{
    return [pendingArchivedResources objectForKey:loader] != nil;
}

- (void)cancelPendingArchiveLoadForLoader:(WebLoader *)loader
{
    [pendingArchivedResources removeObjectForKey:loader];
    
    if (![pendingArchivedResources count])
        [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(deliverArchivedResources) object:nil];
}

- (void)_addExtraFieldsToRequest:(NSMutableURLRequest *)request mainResource:(BOOL)mainResource alwaysFromRequest:(BOOL)f
{
    [client _addExtraFieldsToRequest:request mainResource:mainResource alwaysFromRequest:f];
}

- (void)cannotShowMIMETypeForURL:(NSURL *)URL
{
    [self handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowMIMEType forURL:URL];    
}

- (NSError *)interruptForPolicyChangeErrorWithRequest:(NSURLRequest *)request
{
    return [NSError _webKitErrorWithDomain:WebKitErrorDomain code:WebKitErrorFrameLoadInterruptedByPolicyChange URL:[request URL]];
}

- (BOOL)isHostedByObjectElement
{
    // Handle <object> fallback for error cases.            
    DOMHTMLElement *hostElement = [client frameElement];
    return hostElement && [hostElement isKindOfClass:[DOMHTMLObjectElement class]];
}

- (BOOL)isLoadingMainFrame
{
    return [client _isMainFrame];
}

+ (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    return [WebView canShowMIMEType:MIMEType];
}

+ (BOOL)_representationExistsForURLScheme:(NSString *)URLScheme
{
    return [WebView _representationExistsForURLScheme:URLScheme];
}

+ (NSString *)_generatedMIMETypeForURLScheme:(NSString *)URLScheme
{
    return [WebView _generatedMIMETypeForURLScheme:URLScheme];
}

- (void)_checkNavigationPolicyForRequest:(NSURLRequest *)newRequest andCall:(id)obj withSelector:(SEL)sel
{
    [self checkNavigationPolicyForRequest:newRequest
                              dataSource:[self activeDataSource]
                                formState:nil
                                  andCall:obj
                             withSelector:sel];
}

- (void)_checkContentPolicyForMIMEType:(NSString *)MIMEType andCall:(id)obj withSelector:(SEL)sel
{
    WebPolicyDecisionListener *l = [[WebPolicyDecisionListener alloc] _initWithTarget:obj action:sel];
    listener = l;
    
    [l retain];

    [[[client webView] _policyDelegateForwarder] webView:[client webView] decidePolicyForMIMEType:MIMEType
                                                 request:[[self activeDocumentLoader] request]
                                                   frame:client
                                        decisionListener:listener];
    [l release];
}

- (void)cancelContentPolicy
{
    [listener _invalidate];
    [listener release];
    listener = nil;
}

static inline BOOL isBackForwardLoadType(WebFrameLoadType type)
{
    switch (type) {
        case WebFrameLoadTypeStandard:
        case WebFrameLoadTypeReload:
        case WebFrameLoadTypeReloadAllowingStaleData:
        case WebFrameLoadTypeSame:
        case WebFrameLoadTypeInternal:
        case WebFrameLoadTypeReplace:
            return false;
        case WebFrameLoadTypeBack:
        case WebFrameLoadTypeForward:
        case WebFrameLoadTypeIndexedBackForward:
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

- (BOOL)shouldReloadToHandleUnreachableURLFromRequest:(NSURLRequest *)request
{
    NSURL *unreachableURL = [request _webDataRequestUnreachableURL];
    if (unreachableURL == nil)
        return NO;
    
    if (!isBackForwardLoadType(policyLoadType))
        return NO;
    
    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    WebDataSource *compareDataSource = nil;
    if (delegateIsDecidingNavigationPolicy || delegateIsHandlingUnimplementablePolicy)
        compareDataSource = [self policyDataSource];
    else if (delegateIsHandlingProvisionalLoadError)
        compareDataSource = [self provisionalDataSource];
    
    return compareDataSource != nil && [unreachableURL isEqual:[[compareDataSource request] URL]];
}

- (void)_loadRequest:(NSURLRequest *)request archive:(WebArchive *)archive
{
    WebFrameLoadType type;
    
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoader:policyDocumentLoader];

    NSMutableURLRequest *r = [newDataSource request];
    [client _addExtraFieldsToRequest:r mainResource:YES alwaysFromRequest:NO];
    if ([client _shouldTreatURLAsSameAsCurrent:[request URL]]) {
        [r setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        type = WebFrameLoadTypeSame;
    } else
        type = WebFrameLoadTypeStandard;
    
    [policyDocumentLoader setOverrideEncoding:[[self documentLoader] overrideEncoding]];
    [newDataSource _addToUnarchiveState:archive];
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the b/f list, we treat it as a reload so the b/f list 
    // is appropriately maintained.
    if ([self shouldReloadToHandleUnreachableURLFromRequest:request]) {
        ASSERT(type == WebFrameLoadTypeStandard);
        type = WebFrameLoadTypeReload;
    }
    
    [self loadDataSource:newDataSource withLoadType:type formState:nil];
}

- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(WebFrameLoadType)type formState:(WebFormState *)formState
{
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoader:policyDocumentLoader];

    [policyDocumentLoader setTriggeringAction:action];
    [policyDocumentLoader setOverrideEncoding:[[self documentLoader] overrideEncoding]];

    [self loadDataSource:newDataSource withLoadType:type formState:formState];
}

- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding
{
    WebDataSource *ds = [self dataSource];
    if (ds == nil)
        return;

    NSMutableURLRequest *request = [[ds request] mutableCopy];
    NSURL *unreachableURL = [ds unreachableURL];
    if (unreachableURL != nil)
        [request setURL:unreachableURL];

    [request setCachePolicy:NSURLRequestReturnCacheDataElseLoad];
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoader:policyDocumentLoader];
    [request release];
    
    [policyDocumentLoader setOverrideEncoding:encoding];

    [self loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReloadAllowingStaleData formState:nil];
}

- (void)reload
{
    WebDataSource *ds = [self dataSource];
    if (ds == nil)
        return;

    NSMutableURLRequest *initialRequest = [ds request];
    
    // If a window is created by javascript, its main frame can have an empty but non-nil URL.
    // Reloading in this case will lose the current contents (see 4151001).
    if ([[[[ds request] URL] absoluteString] length] == 0)
        return;

    // Replace error-page URL with the URL we were trying to reach.
    NSURL *unreachableURL = [initialRequest _webDataRequestUnreachableURL];
    if (unreachableURL != nil)
        initialRequest = [NSURLRequest requestWithURL:unreachableURL];
    
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:initialRequest];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoader:policyDocumentLoader];
    NSMutableURLRequest *request = [newDataSource request];

    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];

    // If we're about to rePOST, set up action so the app can warn the user
    if ([[request HTTPMethod] _webkit_isCaseInsensitiveEqualToString:@"POST"]) {
        NSDictionary *action = [client _actionInformationForNavigationType:WebNavigationTypeFormResubmitted event:nil originalURL:[request URL]];
        [policyDocumentLoader setTriggeringAction:action];
    }

    [policyDocumentLoader setOverrideEncoding:[[ds _documentLoader] overrideEncoding]];
    
    [self loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReload formState:nil];
}

- (void)didReceiveServerRedirectForProvisionalLoadForFrame
{
    [client _didReceiveServerRedirectForProvisionalLoadForFrame];
}

- (void)finishedLoadingDocument:(WebDocumentLoader *)loader
{
    [[client _dataSourceForDocumentLoader:loader] _finishedLoading];
}

- (void)commitProvisitionalLoad
{
    [client _commitProvisionalLoad:nil];
}

- (void)committedLoadWithDocumentLoader:(WebDocumentLoader *)loader data:(NSData *)data
{
    [[client _dataSourceForDocumentLoader:loader] _receivedData:data];
}

- (BOOL)isReplacing
{
    return loadType == WebFrameLoadTypeReplace;
}

- (void)setReplacing
{
    loadType = WebFrameLoadTypeReplace;
}

- (void)revertToProvisionalWithDocumentLoader:(WebDocumentLoader *)loader
{
    [[client _dataSourceForDocumentLoader:loader] _revertToProvisionalState];
}

- (void)documentLoader:(WebDocumentLoader *)loader setMainDocumentError:(NSError *)error
{
    [[client _dataSourceForDocumentLoader:loader] _setMainDocumentError:error];
}

- (void)documentLoader:(WebDocumentLoader *)loader mainReceivedCompleteError:(NSError *)error
{
    [loader setPrimaryLoadComplete:YES];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:[client webView] didLoadMainResourceForDataSource:[self activeDataSource]];
    [client _checkLoadComplete];
}

- (void)finalSetupForReplaceWithDocumentLoader:(WebDocumentLoader *)loader
{
    [[client _dataSourceForDocumentLoader:loader] _clearUnarchivingState];
}

- (void)prepareForLoadStart
{
    [[client webView] _progressStarted:client];
    [[client webView] _didStartProvisionalLoadForFrame:client];
    [[[client webView] _frameLoadDelegateForwarder] webView:[client webView]
                               didStartProvisionalLoadForFrame:client];    
}

- (BOOL)subframeIsLoading
{
    return [client _subframeIsLoading];
}

- (void)willChangeTitleForDocument:(WebDocumentLoader *)loader
{
    // FIXME: should do this only in main frame case, right?
    [[client webView] _willChangeValueForKey:_WebMainFrameTitleKey];
}

- (void)didChangeTitleForDocument:(WebDocumentLoader *)loader
{
    // FIXME: should do this only in main frame case, right?
    [[client webView] _didChangeValueForKey:_WebMainFrameTitleKey];

    // The title doesn't get communicated to the WebView until we are committed.
    if ([loader isCommitted]) {
        NSURL *URLForHistory = [[client _dataSourceForDocumentLoader:loader] _URLForHistory];
        if (URLForHistory != nil) {
            WebHistoryItem *entry = [[WebHistory optionalSharedHistory] itemForURL:URLForHistory];
            [entry setTitle:[loader title]];
        
            // Must update the entries in the back-forward list too.  This must go through the WebFrame because
            // it has the right notion of the current b/f item.
            [client _setTitle:[loader title]];
        
            [[client webView] setMainFrameDocumentReady:YES];    // update observers with new DOMDocument
            [[[client webView] _frameLoadDelegateForwarder] webView:[client webView]
                                                      didReceiveTitle:[loader title]
                                                             forFrame:client];
        }
    }
}

- (WebFrameLoadType)loadType
{
    return loadType;
}

- (void)setLoadType:(WebFrameLoadType)type
{
    loadType = type;
}

- (void)invalidatePendingPolicyDecisionCallingDefaultAction:(BOOL)call
{
    [listener _invalidate];
    [listener release];
    listener = nil;

    NSURLRequest *request = policyRequest;
    NSString *frameName = policyFrameName;
    id target = policyTarget;
    SEL selector = policySelector;
    WebFormState *formState = policyFormState;

    policyRequest = nil;
    policyFrameName = nil;
    policyTarget = nil;
    policySelector = nil;
    policyFormState = nil;

    if (call) {
        if (frameName)
            objc_msgSend(target, selector, nil, nil, nil);
        else
            objc_msgSend(target, selector, nil, nil);
    }

    [request release];
    [frameName release];
    [target release];
    [formState release];
}

- (void)checkNewWindowPolicyForRequest:(NSURLRequest *)request action:(NSDictionary *)action frameName:(NSString *)frameName formState:(WebFormState *)formState andCall:(id)target withSelector:(SEL)selector
{
    WebPolicyDecisionListener *decisionListener = [[WebPolicyDecisionListener alloc]
        _initWithTarget:self action:@selector(_continueAfterNewWindowPolicy:)];

    policyRequest = [request retain];
    policyTarget = [target retain];
    policyFrameName = [frameName retain];
    policySelector = selector;
    listener = [decisionListener retain];
    policyFormState = [formState retain];

    WebView *wv = [client webView];
    [[wv _policyDelegateForwarder] webView:wv
            decidePolicyForNewWindowAction:action
                                   request:request
                              newFrameName:frameName
                          decisionListener:decisionListener];
    
    [decisionListener release];
}

- (void)_continueAfterNewWindowPolicy:(WebPolicyAction)policy
{
    NSURLRequest *request = [[policyRequest retain] autorelease];
    NSString *frameName = [[policyFrameName retain] autorelease];
    id target = [[policyTarget retain] autorelease];
    SEL selector = policySelector;
    WebFormState *formState = [[policyFormState retain] autorelease];

    // will release policy* objects, hence the above retains
    [self invalidatePendingPolicyDecisionCallingDefaultAction:NO];

    BOOL shouldContinue = NO;

    switch (policy) {
    case WebPolicyIgnore:
        break;
    case WebPolicyDownload:
        // FIXME: should download full request
        [[client webView] _downloadURL:[request URL]];
        break;
    case WebPolicyUse:
        shouldContinue = YES;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    objc_msgSend(target, selector, shouldContinue ? request : nil, frameName, formState);
}

- (void)checkNavigationPolicyForRequest:(NSURLRequest *)request
                             dataSource:(WebDataSource *)dataSource
                              formState:(WebFormState *)formState
                                andCall:(id)target
                           withSelector:(SEL)selector
{
    NSDictionary *action = [[dataSource _documentLoader] triggeringAction];
    if (action == nil) {
        action = [client _actionInformationForNavigationType:WebNavigationTypeOther event:nil originalURL:[request URL]];
        [[dataSource _documentLoader]  setTriggeringAction:action];
    }
        
    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if ([request isEqual:[[dataSource _documentLoader] lastCheckedRequest]] || [[request URL] _web_isEmpty]) {
        [target performSelector:selector withObject:request withObject:nil];
        return;
    }
    
    // We are always willing to show alternate content for unreachable URLs;
    // treat it like a reload so it maintains the right state for b/f list.
    if ([request _webDataRequestUnreachableURL] != nil) {
        if (isBackForwardLoadType(policyLoadType))
            policyLoadType = WebFrameLoadTypeReload;
        [target performSelector:selector withObject:request withObject:nil];
        return;
    }
    
    [[dataSource _documentLoader] setLastCheckedRequest:request];

    WebPolicyDecisionListener *decisionListener = [[WebPolicyDecisionListener alloc] _initWithTarget:self action:@selector(continueAfterNavigationPolicy:)];
    
    ASSERT(policyRequest == nil);
    policyRequest = [request retain];
    ASSERT(policyTarget == nil);
    policyTarget = [target retain];
    policySelector = selector;
    ASSERT(listener == nil);
    listener = [decisionListener retain];
    ASSERT(policyFormState == nil);
    policyFormState = [formState retain];

    WebView *wv = [client webView];
    delegateIsDecidingNavigationPolicy = YES;
    [[wv _policyDelegateForwarder] webView:wv
           decidePolicyForNavigationAction:action
                                   request:request
                                     frame:client
                          decisionListener:decisionListener];
    delegateIsDecidingNavigationPolicy = NO;
    
    [decisionListener release];
}

- (void)continueAfterNavigationPolicy:(WebPolicyAction)policy
{
    NSURLRequest *request = [[policyRequest retain] autorelease];
    id target = [[policyTarget retain] autorelease];
    SEL selector = policySelector;
    WebFormState *formState = [[policyFormState retain] autorelease];
    
    // will release policy* objects, hence the above retains
    [self invalidatePendingPolicyDecisionCallingDefaultAction:NO];

    BOOL shouldContinue = NO;

    switch (policy) {
    case WebPolicyIgnore:
        break;
    case WebPolicyDownload:
        // FIXME: should download full request
        [[client webView] _downloadURL:[request URL]];
        break;
    case WebPolicyUse:
        if (![WebView _canHandleRequest:request]) {
            [self handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowURL forURL:[request URL]];
        } else {
            shouldContinue = YES;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    [target performSelector:selector withObject:(shouldContinue ? request : nil) withObject:formState];
}

// Called after the FormsDelegate is done processing willSubmitForm:
- (void)continueAfterWillSubmitForm:(WebPolicyAction)policy
{
    if (listener) {
        [listener _invalidate];
        [listener release];
        listener = nil;
    }
    [self startLoading];
}

- (void)continueLoadRequestAfterNavigationPolicy:(NSURLRequest *)request formState:(WebFormState *)formState
{
    // If we loaded an alternate page to replace an unreachableURL, we'll get in here with a
    // nil policyDataSource because loading the alternate page will have passed
    // through this method already, nested; otherwise, policyDataSource should still be set.
    ASSERT([self policyDataSource] || [[self provisionalDataSource] unreachableURL] != nil);

    BOOL isTargetItem = [client _provisionalItemIsTarget];

    // Two reasons we can't continue:
    //    1) Navigation policy delegate said we can't so request is nil. A primary case of this 
    //       is the user responding Cancel to the form repost nag sheet.
    //    2) User responded Cancel to an alert popped up by the before unload event handler.
    // The "before unload" event handler runs only for the main frame.
    BOOL canContinue = request && ([[client webView] mainFrame] != client || [[self bridge] shouldClose]);

    if (!canContinue) {
        // If we were waiting for a quick redirect, but the policy delegate decided to ignore it, then we 
        // need to report that the client redirect was cancelled.
        if ([client _quickRedirectComing])
            [client _clientRedirectCancelledOrFinished:NO];

        [self setPolicyDocumentLoader:nil];

        // If the navigation request came from the back/forward menu, and we punt on it, we have the 
        // problem that we have optimistically moved the b/f cursor already, so move it back.  For sanity, 
        // we only do this when punting a navigation for the target frame or top-level frame.  
        if ((isTargetItem || [[client webView] mainFrame] == client) && isBackForwardLoadType(policyLoadType))
            [(WebFrame <WebFrameLoaderClient> *)[[client webView] mainFrame] _resetBackForwardList];

        return;
    }
    
    WebFrameLoadType type = policyLoadType;
    WebDataSource *dataSource = [[self policyDataSource] retain];
    
    [self stopLoading];
    loadType = type;

    [self startProvisionalLoad:dataSource];

    [dataSource release];
    [self setPolicyDocumentLoader:nil];
    
    if (client == [[client webView] mainFrame])
        LOG(DocumentLoad, "loading %@", [[[self provisionalDataSource] request] URL]);

    if (type == WebFrameLoadTypeForward || type == WebFrameLoadTypeBack || type == WebFrameLoadTypeIndexedBackForward) {
        if ([client _loadProvisionalItemFromPageCache])
            return;
    }

    if (formState) {
        // It's a bit of a hack to reuse the WebPolicyDecisionListener for the continuation
        // mechanism across the willSubmitForm callout.
        listener = [[WebPolicyDecisionListener alloc] _initWithTarget:self action:@selector(continueAfterWillSubmitForm:)];
        [[[client webView] _formDelegate] frame:client sourceFrame:[formState sourceFrame] willSubmitForm:[formState form] withValues:[formState values] submissionListener:listener];
    } 
    else {
        [self continueAfterWillSubmitForm:WebPolicyUse];
    }
}

- (void)loadDataSource:(WebDataSource *)newDataSource withLoadType:(WebFrameLoadType)type formState:(WebFormState *)formState
{
    ASSERT([client webView] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT([client frameView] != nil);

    policyLoadType = type;

    WebDocumentLoaderMac *loader = (WebDocumentLoaderMac *)[newDataSource _documentLoader];

    WebFrame *parentFrame = [client parentFrame];
    if (parentFrame)
        [loader setOverrideEncoding:[[[parentFrame dataSource] _documentLoader] overrideEncoding]];

    [loader setFrameLoader:self];
    [loader setDataSource:newDataSource];

    [self invalidatePendingPolicyDecisionCallingDefaultAction:YES];

    [self setPolicyDocumentLoader:loader];

    [self checkNavigationPolicyForRequest:[newDataSource request]
                               dataSource:newDataSource
                                formState:formState
                                  andCall:self
                             withSelector:@selector(continueLoadRequestAfterNavigationPolicy:formState:)];
}

- (void)handleUnimplementablePolicyWithErrorCode:(int)code forURL:(NSURL *)URL
{
    NSError *error = [NSError _webKitErrorWithDomain:WebKitErrorDomain code:code URL:URL];
    WebView *wv = [client webView];
    delegateIsHandlingUnimplementablePolicy = YES;
    [[wv _policyDelegateForwarder] webView:wv unableToImplementPolicyWithError:error frame:client];    
    delegateIsHandlingUnimplementablePolicy = NO;
}

- (BOOL)delegateIsHandlingProvisionalLoadError
{
    return delegateIsHandlingProvisionalLoadError;
}

- (void)setDelegateIsHandlingProvisionalLoadError:(BOOL)is
{
    delegateIsHandlingProvisionalLoadError = is;
}

- (void)didFirstLayout
{
    if ([[client webView] backForwardList]) {
        if (loadType == WebFrameLoadTypeForward || loadType == WebFrameLoadTypeBack || loadType == WebFrameLoadTypeIndexedBackForward)
            [client _restoreScrollPositionAndViewState];
    }
    
    firstLayoutDone = YES;

    WebView *wv = [client webView];
    [[wv _frameLoadDelegateForwarder] webView:wv didFirstLayoutInFrame:client];
}

- (void)provisionalLoadStarted
{
    firstLayoutDone = NO;
}

- (void)frameLoadCompleted
{
    // After a canceled provisional load, firstLayoutDone is NO. Reset it to YES if we're displaying a page.
    if ([self dataSource])
        firstLayoutDone = YES;
}

- (BOOL)firstLayoutDone
{
    return firstLayoutDone;
}

@end
