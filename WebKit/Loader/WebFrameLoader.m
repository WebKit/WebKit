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
#import "WebFrameLoaderClient.h"
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

- (void)provisionalLoadStarted
{
    firstLayoutDone = NO;
    [[client _bridge] provisionalLoadStarted];

    [client _provisionalLoadStarted];
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
        [self provisionalLoadStarted];
    else if (state == WebFrameStateComplete) {
        [client _frameLoadCompleted];
        [self frameLoadCompleted];
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

- (void)stopLoadingSubframes
{
    for (WebCoreFrameBridge *child = [[client _bridge] firstChild]; child; child = [child nextSibling])
        [[(WebFrameBridge *)child loader] stopLoading];
}

- (void)stopLoading
{
    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (isStoppingLoad)
        return;
    
    isStoppingLoad = YES;
    
    [self invalidatePendingPolicyDecisionCallingDefaultAction:YES];
    
    [self stopLoadingSubframes];
    [provisionalDocumentLoader stopLoading];
    [documentLoader stopLoading];
    [self _clearProvisionalDataSource];
    [self clearArchivedResources];

    isStoppingLoad = NO;    
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
    
    if (![self startLoadingMainResourceWithRequest:[provisionalDocumentLoader actualRequest] identifier:identifier])
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
    
    if ([self state] == WebFrameStateProvisional) {
        NSURL *failedURL = [[provisionalDocumentLoader originalRequestCopy] URL];
        [bridge didNotOpenURL:failedURL];
        [client _invalidateCurrentItemPageCache];
        
        // Call -_clientRedirectCancelledOrFinished: here so that the frame load delegate is notified that the redirect's
        // status has changed, if there was a redirect.  The frame load delegate may have saved some state about
        // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:.  Since we are definitely
        // not going to use this provisional resource, as it was cancelled, notify the frame load delegate that the redirect
        // has ended.
        if (sentRedirectNotification)
            [self clientRedirectCancelledOrFinished:NO];
    }
    
    
    [loader mainReceivedError:error complete:isComplete];

    [bridge release];
    [cli release];
    [loader release];
}

- (void)clientRedirectCancelledOrFinished:(BOOL)cancelWithLoadInProgress
{
    // Note that -webView:didCancelClientRedirectForFrame: is called on the frame load delegate even if
    // the redirect succeeded.  We should either rename this API, or add a new method, like
    // -webView:didFinishClientRedirectForFrame:
    [[[client webView] _frameLoadDelegateForwarder] webView:[client webView]
                            didCancelClientRedirectForFrame:client];
    if (!cancelWithLoadInProgress)
        quickRedirectComing = NO;
    
    sentRedirectNotification = NO;
    
    LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [client name], self, (int)quickRedirectComing);
}

- (void)clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory isJavaScriptFormAction:(BOOL)isJavaScriptFormAction
{
    LOG(Redirect, "%@(%p) Client redirect to: %@, [self dataSource] = %p, lockHistory = %d, isJavaScriptFormAction = %d", [client name], self, URL, [self dataSource], (int)lockHistory, (int)isJavaScriptFormAction);
    
    [[[client webView] _frameLoadDelegateForwarder] webView:[client webView]
                             willPerformClientRedirectToURL:URL
                                                      delay:seconds
                                                   fireDate:date
                                                   forFrame:client];
    
    // Remember that we sent a redirect notification to the frame load delegate so that when we commit
    // the next provisional load, we can send a corresponding -webView:didCancelClientRedirectForFrame:
    sentRedirectNotification = YES;
    
    // If a "quick" redirect comes in an, we set a special mode so we treat the next
    // load as part of the same navigation.
    
    if (!documentLoader || isJavaScriptFormAction) {
        // If we don't have a dataSource, we have no "original" load on which to base a redirect,
        // so we better just treat the redirect as a normal load.
        quickRedirectComing = NO;
        LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [client name], self, (int)quickRedirectComing);
    } else {
        quickRedirectComing = lockHistory;
        LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [client name], self, (int)quickRedirectComing);
    }
}

- (BOOL)shouldReloadForCurrent:(NSURL *)currentURL andDestination:(NSURL *)destinationURL
{
    return !(([currentURL fragment] || [destinationURL fragment]) &&
             [[currentURL _webkit_URLByRemovingFragment] isEqual:[destinationURL _webkit_URLByRemovingFragment]]);
}

// main funnel for navigating via callback from WebCore (e.g., clicking a link, redirect)
- (void)loadURL:(NSURL *)URL referrer:(NSString *)referrer loadType:(FrameLoadType)_loadType target:(NSString *)target triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values
{
    BOOL isFormSubmission = (values != nil);
    
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [request setValue:[[client webView] userAgentForURL:[request URL]] forHTTPHeaderField:@"Referer"];
    [self _addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:(event != nil || isFormSubmission)];
    if (_loadType == FrameLoadTypeReload) {
        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    }
    
    // I believe this is never called with LoadSame.  If it is, we probably want to set the cache
    // policy of LoadFromOrigin, but I didn't test that.
    ASSERT(_loadType != FrameLoadTypeSame);
    
    NSDictionary *action = [client _actionInformationForLoadType:_loadType isFormSubmission:isFormSubmission event:event originalURL:URL];
    WebFormState *formState = nil;
    if (form && values)
        formState = [[WebFormState alloc] initWithForm:form values:values sourceFrame:client];
    
    if (target != nil) {
        WebFrame *targetFrame = [client findFrameNamed:target];
        if (targetFrame != nil) {
            [[targetFrame _frameLoader] loadURL:URL referrer:referrer loadType:_loadType target:nil triggeringEvent:event form:form formValues:values];
        } else {
            [self checkNewWindowPolicyForRequest:request
                                          action:action
                                       frameName:target
                                       formState:formState
                                         andCall:self
                                    withSelector:@selector(_continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
        }
        [request release];
        [formState release];
        return;
    }
    
    WebDataSource *oldDataSource = [[self dataSource] retain];
    
    BOOL sameURL = [client _shouldTreatURLAsSameAsCurrent:URL];
    
    // Make sure to do scroll to anchor processing even if the URL is
    // exactly the same so pages with '#' links and DHTML side effects
    // work properly.
    if (!isFormSubmission
        && _loadType != FrameLoadTypeReload
        && _loadType != FrameLoadTypeSame
        && ![self shouldReloadForCurrent:URL andDestination:[[client _bridge] URL]]
        
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && ![[client _bridge] isFrameSet]) {
        
        // Just do anchor navigation within the existing content.
        
        // We don't do this if we are submitting a form, explicitly reloading,
        // currently displaying a frameset, or if the new URL does not have a fragment.
        // These rules are based on what KHTML was doing in KHTMLPart::openURL.
        
        // FIXME: What about load types other than Standard and Reload?
        
        [[oldDataSource _documentLoader] setTriggeringAction:action];
        [self invalidatePendingPolicyDecisionCallingDefaultAction:YES];
        [self checkNavigationPolicyForRequest:request
                                                    dataSource:oldDataSource formState:formState
                                                       andCall:self withSelector:@selector(continueFragmentScrollAfterNavigationPolicy:formState:)];
    } else {
        // must grab this now, since this load may stop the previous load and clear this flag
        BOOL isRedirect = quickRedirectComing;
        [self _loadRequest:request triggeringAction:action loadType:_loadType formState:formState];
        if (isRedirect) {
            LOG(Redirect, "%@(%p) _private->quickRedirectComing was %d", [client name], self, (int)isRedirect);
            quickRedirectComing = NO;
            [provisionalDocumentLoader setIsClientRedirect:YES];
        } else if (sameURL) {
            // Example of this case are sites that reload the same URL with a different cookie
            // driving the generated content, or a master frame with links that drive a target
            // frame, where the user has clicked on the same link repeatedly.
            [self setLoadType:FrameLoadTypeSame];
        }            
    }
    
    [request release];
    [oldDataSource release];
    [formState release];
}

-(void)continueFragmentScrollAfterNavigationPolicy:(NSURLRequest *)request formState:(WebFormState *)formState
{
    if (!request)
        return;
    
    NSURL *URL = [request URL];
    
    BOOL isRedirect = quickRedirectComing;
    LOG(Redirect, "%@(%p) _private->quickRedirectComing = %d", [client name], self, (int)quickRedirectComing);
    quickRedirectComing = NO;
    
    [documentLoader replaceRequestURLForAnchorScrollWithURL:URL];
    if (!isRedirect && ![client _shouldTreatURLAsSameAsCurrent:URL]) {
        // NB: must happen after _setURL, since we add based on the current request.
        // Must also happen before we openURL and displace the scroll position, since
        // adding the BF item will save away scroll state.
        
        // NB2:  If we were loading a long, slow doc, and the user anchor nav'ed before
        // it was done, currItem is now set the that slow doc, and prevItem is whatever was
        // before it.  Adding the b/f item will bump the slow doc down to prevItem, even
        // though its load is not yet done.  I think this all works out OK, for one because
        // we have already saved away the scroll and doc state for the long slow load,
        // but it's not an obvious case.

        [client _addHistoryItemForFragmentScroll];
    }
    
    [[client _bridge] scrollToAnchorWithURL:URL];
    
    if (!isRedirect) {
        // This will clear previousItem from the rest of the frame tree tree that didn't
        // doing any loading.  We need to make a pass on this now, since for anchor nav
        // we'll not go through a real load and reach Completed state
        [client _checkLoadComplete];
    }
    
    [[[client webView] _frameLoadDelegateForwarder] webView:[client webView]
                        didChangeLocationWithinPageForFrame:client];

    [client _didFinishLoad];
}

- (void)closeOldDataSources
{
    // FIXME: is it important for this traversal to be postorder instead of preorder?
    // FIXME: add helpers for postorder traversal?
    for (WebCoreFrameBridge *child = [[client _bridge] firstChild]; child; child = [child nextSibling])
        [[(WebFrameBridge *)child loader] closeOldDataSources];
    
    if (documentLoader)
        [[[client webView] _frameLoadDelegateForwarder] webView:[client webView] willCloseFrame:client];
    [[client webView] setMainFrameDocumentReady:NO];  // stop giving out the actual DOMDocument to observers
}

- (void)commitProvisionalLoad:(NSDictionary *)pageCache
{
    bool reload = loadType == FrameLoadTypeReload || loadType == FrameLoadTypeReloadAllowingStaleData;
    
    WebDataSource *provisionalDataSource = [[self provisionalDataSource] retain];
    NSURLResponse *response = [provisionalDataSource response];
    
    NSDictionary *headers = [response isKindOfClass:[NSHTTPURLResponse class]]
        ? [(NSHTTPURLResponse *)response allHeaderFields] : nil;
    
    if (loadType != FrameLoadTypeReplace)
        [self closeOldDataSources];
    
    if (!pageCache)
        [provisionalDataSource _makeRepresentation];
    
    [client _transitionToCommitted:pageCache];
    
    // Call -_clientRedirectCancelledOrFinished: here so that the frame load delegate is notified that the redirect's
    // status has changed, if there was a redirect.  The frame load delegate may have saved some state about
    // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:.  Since we are
    // just about to commit a new page, there cannot possibly be a pending redirect at this point.
    if (sentRedirectNotification)
        [self clientRedirectCancelledOrFinished:NO];
    
    NSURL *baseURL = [[provisionalDataSource request] _webDataRequestBaseURL];        
    NSURL *URL = baseURL ? baseURL : [response URL];
    
    if (!URL || [URL _web_isEmpty])
        URL = [NSURL URLWithString:@"about:blank"];    
    
    [[client _bridge] openURL:URL
                     reload:reload 
                contentType:[response MIMEType]
                    refresh:[headers objectForKey:@"Refresh"]
               lastModified:(pageCache ? nil : WKGetNSURLResponseLastModifiedDate(response))
                  pageCache:pageCache];
    
    [client _opened];
    
    [provisionalDataSource release];
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

static inline BOOL isBackForwardLoadType(FrameLoadType type)
{
    switch (type) {
        case FrameLoadTypeStandard:
        case FrameLoadTypeReload:
        case FrameLoadTypeReloadAllowingStaleData:
        case FrameLoadTypeSame:
        case FrameLoadTypeInternal:
        case FrameLoadTypeReplace:
            return false;
        case FrameLoadTypeBack:
        case FrameLoadTypeForward:
        case FrameLoadTypeIndexedBackForward:
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
    FrameLoadType type;
    
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoader:policyDocumentLoader];

    NSMutableURLRequest *r = [newDataSource request];
    [client _addExtraFieldsToRequest:r mainResource:YES alwaysFromRequest:NO];
    if ([client _shouldTreatURLAsSameAsCurrent:[request URL]]) {
        [r setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        type = FrameLoadTypeSame;
    } else
        type = FrameLoadTypeStandard;
    
    [policyDocumentLoader setOverrideEncoding:[[self documentLoader] overrideEncoding]];
    [newDataSource _addToUnarchiveState:archive];
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the b/f list, we treat it as a reload so the b/f list 
    // is appropriately maintained.
    if ([self shouldReloadToHandleUnreachableURLFromRequest:request]) {
        ASSERT(type == FrameLoadTypeStandard);
        type = FrameLoadTypeReload;
    }
    
    [self loadDataSource:newDataSource withLoadType:type formState:nil];
}

- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(FrameLoadType)type formState:(WebFormState *)formState
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

    [self loadDataSource:newDataSource withLoadType:FrameLoadTypeReloadAllowingStaleData formState:nil];
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
    
    [self loadDataSource:newDataSource withLoadType:FrameLoadTypeReload formState:nil];
}

- (void)didReceiveServerRedirectForProvisionalLoadForFrame
{
    [client _didReceiveServerRedirectForProvisionalLoadForFrame];
}

- (void)finishedLoadingDocument:(WebDocumentLoader *)loader
{
    [[client _dataSourceForDocumentLoader:loader] _finishedLoading];
}

- (void)committedLoadWithDocumentLoader:(WebDocumentLoader *)loader data:(NSData *)data
{
    [[client _dataSourceForDocumentLoader:loader] _receivedData:data];
}

- (BOOL)isReplacing
{
    return loadType == FrameLoadTypeReplace;
}

- (void)setReplacing
{
    loadType = FrameLoadTypeReplace;
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

- (FrameLoadType)loadType
{
    return loadType;
}

- (void)setLoadType:(FrameLoadType)type
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
            policyLoadType = FrameLoadTypeReload;
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
        if (quickRedirectComing)
            [self clientRedirectCancelledOrFinished:NO];

        [self setPolicyDocumentLoader:nil];

        // If the navigation request came from the back/forward menu, and we punt on it, we have the 
        // problem that we have optimistically moved the b/f cursor already, so move it back.  For sanity, 
        // we only do this when punting a navigation for the target frame or top-level frame.  
        if ((isTargetItem || [[client webView] mainFrame] == client) && isBackForwardLoadType(policyLoadType))
            [(WebFrame <WebFrameLoaderClient> *)[[client webView] mainFrame] _resetBackForwardList];

        return;
    }
    
    FrameLoadType type = policyLoadType;
    WebDataSource *dataSource = [[self policyDataSource] retain];
    
    [self stopLoading];
    loadType = type;

    [self startProvisionalLoad:dataSource];

    [dataSource release];
    [self setPolicyDocumentLoader:nil];
    
    if (client == [[client webView] mainFrame])
        LOG(DocumentLoad, "loading %@", [[[self provisionalDataSource] request] URL]);

    if (type == FrameLoadTypeForward || type == FrameLoadTypeBack || type == FrameLoadTypeIndexedBackForward) {
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

- (void)loadDataSource:(WebDataSource *)newDataSource withLoadType:(FrameLoadType)type formState:(WebFormState *)formState
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
        if (loadType == FrameLoadTypeForward || loadType == FrameLoadTypeBack || loadType == FrameLoadTypeIndexedBackForward)
            [client _restoreScrollPositionAndViewState];
    }
    
    firstLayoutDone = YES;

    WebView *wv = [client webView];
    [[wv _frameLoadDelegateForwarder] webView:wv didFirstLayoutInFrame:client];
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

- (BOOL)isQuickRedirectComing
{
    return quickRedirectComing;
}

@end
