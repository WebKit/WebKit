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
#import "WebFrameBridge.h"
#import "WebFrameInternal.h"
#import "WebFrameLoadDelegate.h"
#import "WebIconDatabasePrivate.h"
#import "WebKitErrorsPrivate.h"
#import "WebKitLogging.h"
#import "WebKitNSStringExtras.h"
#import "WebMainResourceLoader.h"
#import "WebNSDictionaryExtras.h"
#import "WebNSURLExtras.h"
#import "WebPreferences.h"
#import "WebResourcePrivate.h"
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
    [documentLoadState release];
    [provisionalDocumentLoadState release];
 
    ASSERT(!policyDocumentLoadState);
    
    [super dealloc];
}

- (WebDocumentLoadState *)activeDocumentLoadState
{
    if (state == WebFrameStateProvisional)
        return provisionalDocumentLoadState;
    
    return documentLoadState;    
}

- (WebDataSource *)activeDataSource
{
    return [client _dataSourceForDocumentLoadState:[self activeDocumentLoadState]];
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
    [[self activeDocumentLoadState] setLoading:YES];
}

- (void)removePlugInStreamLoader:(WebLoader *)loader
{
    [plugInStreamLoaders removeObject:loader];
    [[self activeDocumentLoadState] updateLoading];
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
    ASSERT(!provisionalDocumentLoadState);
    if (subresourceLoaders == nil)
        subresourceLoaders = [[NSMutableArray alloc] init];
    [subresourceLoaders addObject:loader];
    [[self activeDocumentLoadState] setLoading:YES];
}

- (void)removeSubresourceLoader:(WebLoader *)loader
{
    [subresourceLoaders removeObject:loader];
    [[self activeDocumentLoadState] updateLoading];
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
    return [client _dataSourceForDocumentLoadState:documentLoadState]; 
}

- (void)_setDocumentLoadState:(WebDocumentLoadState *)loadState
{
    if (loadState == nil && documentLoadState == nil)
        return;
    
    ASSERT(loadState != documentLoadState);
    
    [client _prepareForDataSourceReplacement];
    [documentLoadState detachFromFrameLoader];
    
    [loadState retain];
    [documentLoadState release];
    documentLoadState = loadState;
}

- (WebDocumentLoadState *)documentLoadState
{
    return documentLoadState;
}

- (WebDataSource *)policyDataSource
{
    return [client _dataSourceForDocumentLoadState:policyDocumentLoadState];     
}

- (void)_setPolicyDocumentLoadState:(WebDocumentLoadState *)loadState
{
    [loadState retain];
    [policyDocumentLoadState release];
    policyDocumentLoadState = loadState;
}
   
- (void)clearDataSource
{
    [self _setDocumentLoadState:nil];
}

- (WebDataSource *)provisionalDataSource 
{
    return [client _dataSourceForDocumentLoadState:provisionalDocumentLoadState]; 
}

- (WebDocumentLoadState *)provisionalDocumentLoadState
{
    return provisionalDocumentLoadState;
}

- (void)_setProvisionalDocumentLoadState:(WebDocumentLoadState *)loadState
{
    ASSERT(!loadState || !provisionalDocumentLoadState);

    if (provisionalDocumentLoadState != documentLoadState)
        [provisionalDocumentLoadState detachFromFrameLoader];

    [loadState retain];
    [provisionalDocumentLoadState release];
    provisionalDocumentLoadState = loadState;
}

- (void)_clearProvisionalDataSource
{
    [self _setProvisionalDocumentLoadState:nil];
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
        LOG(Timing, "%@:  transition from %s to %s, %f seconds since start of document load", [client name], stateNames[state], stateNames[newState], CFAbsoluteTimeGetCurrent() - [[[[[client webView] mainFrame] dataSource] _documentLoadState] loadingStartedTime]);
    
    if (newState == WebFrameStateComplete && client == [[client webView] mainFrame])
        LOG(DocumentLoad, "completed %@ (%f seconds)", [[[self dataSource] request] URL], CFAbsoluteTimeGetCurrent() - [[[self dataSource] _documentLoadState] loadingStartedTime]);
    
    state = newState;
    
    if (state == WebFrameStateProvisional)
        [client _provisionalLoadStarted];
    else if (state == WebFrameStateComplete) {
        [client _frameLoadCompleted];
        _timeOfLastCompletedLoad = CFAbsoluteTimeGetCurrent();
        [[self documentLoadState] stopRecordingResponses];
    }
}

- (void)clearProvisionalLoad
{
    [self _setProvisionalDocumentLoadState:nil];
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

    [self _setDocumentLoadState:provisionalDocumentLoadState];
    [self _setProvisionalDocumentLoadState:nil];
    [self _setState:WebFrameStateCommittedPage];
}

- (void)stopLoading
{
    [[self provisionalDocumentLoadState] stopLoading];
    [[self documentLoadState] stopLoading];
    [self _clearProvisionalDataSource];
    [self clearArchivedResources];
}

// FIXME: poor method name; also why is this not part of startProvisionalLoad:?
- (void)startLoading
{
    [[self provisionalDataSource] _startLoading];
}

- (void)startProvisionalLoad:(WebDataSource *)ds
{
    [self _setProvisionalDocumentLoadState:[ds _documentLoadState]];
    [self _setState:WebFrameStateProvisional];
}

- (void)setupForReplace
{
    [self _setState:WebFrameStateProvisional];
    WebDocumentLoadState *old = provisionalDocumentLoadState;
    provisionalDocumentLoadState = documentLoadState;
    documentLoadState = nil;
    [old release];
    
    [client _detachChildren];
}

- (id)_identifierForInitialRequest:(NSURLRequest *)clientRequest
{
    return [[self activeDataSource] _identifierForInitialRequest:clientRequest];
}

- (NSURLRequest *)_willSendRequest:(NSMutableURLRequest *)clientRequest forResource:(id)identifier redirectResponse:(NSURLResponse *)redirectResponse
{
    return [[self activeDataSource] _willSendRequest:clientRequest forResource:identifier redirectResponse:redirectResponse];
}

- (void)_didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    [[self activeDataSource] _didReceiveAuthenticationChallenge:currentWebChallenge forResource:identifier];
}

- (void)_didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    [[self activeDataSource] _didCancelAuthenticationChallenge:currentWebChallenge forResource:identifier];
}

- (void)_didReceiveResponse:(NSURLResponse *)r forResource:(id)identifier
{
    [[self activeDataSource] _didReceiveResponse:r forResource:identifier];
}

- (void)_didReceiveData:(NSData *)data contentLength:(int)lengthReceived forResource:(id)identifier
{
    [[self activeDataSource] _didReceiveData:data contentLength:lengthReceived forResource:identifier];
}

- (void)_didFinishLoadingForResource:(id)identifier
{
    [[self activeDataSource] _didFinishLoadingForResource:identifier];
}

- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier
{
    [[self activeDataSource] _didFailLoadingWithError:error forResource:identifier];
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
    return [[self activeDocumentLoadState] originalRequestCopy];
}

- (WebFrame *)webFrame
{
    return client;
}

- (void)_receivedMainResourceError:(NSError *)error complete:(BOOL)isComplete
{
    WebDataSource *ds = [self activeDataSource];
    [ds retain];
    [ds _receivedMainResourceError:error complete:isComplete];
    [ds release];
}

- (NSURLRequest *)initialRequest
{
    return [[self activeDataSource] initialRequest];
}

- (void)_receivedData:(NSData *)data
{
    [[self activeDocumentLoadState] receivedData:data];
}

- (void)_setRequest:(NSURLRequest *)request
{
    [[self activeDocumentLoadState] setRequest:request];
}

- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection request:(NSURLRequest *)request response:(NSURLResponse *)r proxy:(id)proxy
{
    [[self activeDataSource] _downloadWithLoadingConnection:connection request:request response:r proxy:proxy];
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
    return [[self activeDocumentLoadState] isStopping];
}

- (void)_setupForReplaceByMIMEType:(NSString *)newMIMEType
{
    [[self activeDocumentLoadState] setupForReplaceByMIMEType:newMIMEType];
}

- (void)_setResponse:(NSURLResponse *)response
{
    [[self activeDocumentLoadState] setResponse:response];
}

- (void)_mainReceivedError:(NSError *)error complete:(BOOL)isComplete
{
    [[self activeDocumentLoadState] mainReceivedError:error complete:isComplete];
}

- (void)_finishedLoading
{
    WebDataSource *ds = [self activeDataSource];
    
    [self retain];
    [[self activeDocumentLoadState] finishedLoading];

    if ([ds _mainDocumentError] || ![ds webFrame]) {
        [self release];
        return;
    }

    [[self activeDocumentLoadState] setPrimaryLoadComplete:YES];
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
    [client _handleUnimplementablePolicyWithErrorCode:WebKitErrorCannotShowMIMEType forURL:URL];    
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
    [client _checkNavigationPolicyForRequest:newRequest
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
    [[self activeDataSource] _decidePolicyForMIMEType:MIMEType decisionListener:l];
    [l release];
}

- (void)cancelContentPolicy
{
    [listener _invalidate];
    [listener release];
    listener = nil;
}

- (void)_loadRequest:(NSURLRequest *)request archive:(WebArchive *)archive
{
    WebFrameLoadType type;
    
    policyDocumentLoadState = [client _createDocumentLoadStateWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoadState:policyDocumentLoadState];

    NSMutableURLRequest *r = [newDataSource request];
    [client _addExtraFieldsToRequest:r mainResource:YES alwaysFromRequest:NO];
    if ([client _shouldTreatURLAsSameAsCurrent:[request URL]]) {
        [r setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        type = WebFrameLoadTypeSame;
    } else
        type = WebFrameLoadTypeStandard;
    
    [policyDocumentLoadState setOverrideEncoding:[[self documentLoadState] overrideEncoding]];
    [newDataSource _addToUnarchiveState:archive];
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the b/f list, we treat it as a reload so the b/f list 
    // is appropriately maintained.
    if ([client _shouldReloadToHandleUnreachableURLFromRequest:request]) {
        ASSERT(type == WebFrameLoadTypeStandard);
        type = WebFrameLoadTypeReload;
    }
    
    [client _loadDataSource:newDataSource withLoadType:type formState:nil];
}

- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(WebFrameLoadType)type formState:(WebFormState *)formState
{
    policyDocumentLoadState = [client _createDocumentLoadStateWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoadState:policyDocumentLoadState];

    [policyDocumentLoadState setTriggeringAction:action];

    [policyDocumentLoadState setOverrideEncoding:[[self documentLoadState] overrideEncoding]];

    [client _loadDataSource:newDataSource withLoadType:type formState:formState];
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
    policyDocumentLoadState = [client _createDocumentLoadStateWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoadState:policyDocumentLoadState];
    [request release];
    
    [policyDocumentLoadState setOverrideEncoding:encoding];

    [client _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReloadAllowingStaleData formState:nil];
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
    
    policyDocumentLoadState = [client _createDocumentLoadStateWithRequest:initialRequest];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoadState:policyDocumentLoadState];
    NSMutableURLRequest *request = [newDataSource request];

    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];

    // If we're about to rePOST, set up action so the app can warn the user
    if ([[request HTTPMethod] _webkit_isCaseInsensitiveEqualToString:@"POST"]) {
        NSDictionary *action = [client _actionInformationForNavigationType:WebNavigationTypeFormResubmitted event:nil originalURL:[request URL]];
        [policyDocumentLoadState setTriggeringAction:action];
    }

    [policyDocumentLoadState setOverrideEncoding:[[ds _documentLoadState] overrideEncoding]];
    
    [client _loadDataSource:newDataSource withLoadType:WebFrameLoadTypeReload formState:nil];
}

- (void)didReceiveServerRedirectForProvisionalLoadForFrame
{
    [client _didReceiveServerRedirectForProvisionalLoadForFrame];
}

- (void)finishedLoadingDocumentLoadState:(WebDocumentLoadState *)loadState
{
    [[client _dataSourceForDocumentLoadState:loadState] _finishedLoading];
}

- (void)commitProvisitionalLoad
{
    [client _commitProvisionalLoad:nil];
}

- (void)committedLoadWithDocumentLoadState:(WebDocumentLoadState *)loadState data:(NSData *)data
{
    [[client _dataSourceForDocumentLoadState:loadState] _receivedData:data];
}

- (BOOL)isReplacing
{
    return loadType == WebFrameLoadTypeReplace;
}

- (void)setReplacing
{
    loadType = WebFrameLoadTypeReplace;
}

- (void)revertToProvisionalWithDocumentLoadState:(WebDocumentLoadState *)loadState
{
    [[client _dataSourceForDocumentLoadState:loadState] _revertToProvisionalState];
}

- (void)documentLoadState:(WebDocumentLoadState *)loadState setMainDocumentError:(NSError *)error
{
    [[client _dataSourceForDocumentLoadState:loadState] _setMainDocumentError:error];
}

- (void)documentLoadState:(WebDocumentLoadState *)loadState mainReceivedCompleteError:(NSError *)error
{
    [loadState setPrimaryLoadComplete:YES];
    if ([WebScriptDebugServer listenerCount])
        [[WebScriptDebugServer sharedScriptDebugServer] webView:[client webView] didLoadMainResourceForDataSource:[self activeDataSource]];
    [client _checkLoadComplete];
}

- (void)finalSetupForReplaceWithDocumentLoadState:(WebDocumentLoadState *)loadState
{
    [[client _dataSourceForDocumentLoadState:loadState] _clearUnarchivingState];
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

<<<<<<< .mine
- (void)willChangeTitleForDocumentLoadState:(WebDocumentLoadState *)loadState
{
    // FIXME: should do this only in main frame case, right?
    [[webFrame webView] _willChangeValueForKey:_WebMainFrameTitleKey];
}

- (void)didChangeTitleForDocumentLoadState:(WebDocumentLoadState *)loadState
{
    // FIXME: should do this only in main frame case, right?
    [[webFrame webView] _didChangeValueForKey:_WebMainFrameTitleKey];

    // The title doesn't get communicated to the WebView until we are committed.
    if ([loadState isCommitted]) {
        NSURL *URLForHistory = [[webFrame _dataSourceForDocumentLoadState:loadState] _URLForHistory];
        if (URLForHistory != nil) {
            WebHistoryItem *entry = [[WebHistory optionalSharedHistory] itemForURL:URLForHistory];
            [entry setTitle:[loadState title]];
        
            // Must update the entries in the back-forward list too.  This must go through the WebFrame because
            // it has the right notion of the current b/f item.
            [webFrame _setTitle:[loadState title]];
        
            [[webFrame webView] setMainFrameDocumentReady:YES];    // update observers with new DOMDocument
            [[[webFrame webView] _frameLoadDelegateForwarder] webView:[webFrame webView]
                                                      didReceiveTitle:[loadState title]
                                                             forFrame:webFrame];
        }
    }
}

=======
- (WebFrameLoadType)loadType
{
    return loadType;
}

- (void)setLoadType:(WebFrameLoadType)type
{
    loadType = type;
}

>>>>>>> .r16864
@end
