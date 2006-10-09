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
#import "WebDocumentLoader.h"
#import "WebFormDataStream.h"
#import "WebFrameBridge.h"
#import "WebFrameLoaderClient.h"
#import "WebMainResourceLoader.h"
#import <JavaScriptCore/Assertions.h>
#import <WebKit/DOMHTML.h>
#import <WebCore/WebCoreSystemInterface.h>

#import "WebDataSourceInternal.h"
#import "WebFrameInternal.h"
#import "WebIconDatabasePrivate.h"
#import "WebKitErrorsPrivate.h"
#import "WebNSURLExtras.h"
#import "WebNSURLRequestExtras.h"
#import "WebResourcePrivate.h"
#import "WebViewInternal.h"

static BOOL isCaseInsensitiveEqual(NSString *a, NSString *b)
{
    return [a caseInsensitiveCompare:b] == NSOrderedSame;
}

BOOL isBackForwardLoadType(FrameLoadType type)
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
    BOOL defers = [[client webView] defersCallbacks];
    for (WebFrame *frame = client; frame; frame = [frame _traverseNextFrameStayWithin:client])
        [[frame _frameLoader] setDefersCallbacks:defers];
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
    [self addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:NO];
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

- (WebFrameState)state
{
    return state;
}

#if !LOG_DISABLED
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

- (void)setState:(WebFrameState)newState
{    
    state = newState;
    
    if (state == WebFrameStateProvisional)
        [self provisionalLoadStarted];
    else if (state == WebFrameStateComplete) {
        [self frameLoadCompleted];
        _timeOfLastCompletedLoad = CFAbsoluteTimeGetCurrent();
        [[self documentLoader] stopRecordingResponses];
    }
}

- (void)clearProvisionalLoad
{
    [self setProvisionalDocumentLoader:nil];
    [[client webView] _progressCompleted:client];
    [self setState:WebFrameStateComplete];
}

- (void)markLoadComplete
{
    [self setState:WebFrameStateComplete];
}

- (void)commitProvisionalLoad
{
    [self stopLoadingSubresources];
    [self stopLoadingPlugIns];

    [self setDocumentLoader:provisionalDocumentLoader];
    [self setProvisionalDocumentLoader:nil];
    [self setState:WebFrameStateCommittedPage];
}

- (void)stopLoadingSubframes
{
    for (WebCoreFrameBridge *child = [[client _bridge] firstChild]; child; child = [child nextSibling])
        [[(WebFrameBridge *)child frameLoader] stopLoading];
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
    [self setProvisionalDocumentLoader:nil];
    [self clearArchivedResources];

    isStoppingLoad = NO;    
}

// FIXME: poor method name; also why is this not part of startProvisionalLoad:?
- (void)startLoading
{
    [provisionalDocumentLoader prepareForLoadStart];
        
    if ([self isLoadingMainResource])
        return;

    [client _clearLoadingFromPageCacheForDocumentLoader:provisionalDocumentLoader];

    id identifier = [client _dispatchIdentifierForInitialRequest:[provisionalDocumentLoader originalRequest] fromDocumentLoader:provisionalDocumentLoader];
        
    if (![self startLoadingMainResourceWithRequest:[provisionalDocumentLoader actualRequest] identifier:identifier])
        [provisionalDocumentLoader updateLoading];
}

- (void)startProvisionalLoad:(WebDocumentLoader *)loader
{
    [self setProvisionalDocumentLoader:loader];
    [self setState:WebFrameStateProvisional];
}

- (void)setupForReplace
{
    [self setState:WebFrameStateProvisional];
    WebDocumentLoader *old = provisionalDocumentLoader;
    provisionalDocumentLoader = documentLoader;
    documentLoader = nil;
    [old release];
    
    [self detachChildren];
}

- (id)_identifierForInitialRequest:(NSURLRequest *)clientRequest
{
    // FIXME: why retain here, but not in the other place this happens?

    // The identifier is released after the last callback, rather than in dealloc,
    // to avoid potential cycles.    
    return [[client _dispatchIdentifierForInitialRequest:clientRequest fromDocumentLoader:[self activeDocumentLoader]] retain];
}

- (NSURLRequest *)_willSendRequest:(NSMutableURLRequest *)clientRequest forResource:(id)identifier redirectResponse:(NSURLResponse *)redirectResponse
{
    WebView *webView = [client webView];
    [clientRequest setValue:[webView userAgentForURL:[clientRequest URL]] forHTTPHeaderField:@"User-Agent"];
    return [client _dispatchResource:identifier willSendRequest:clientRequest redirectResponse:redirectResponse fromDocumentLoader:[self activeDocumentLoader]];
}

- (void)_didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    [client _dispatchDidReceiveAuthenticationChallenge:currentWebChallenge forResource:identifier fromDocumentLoader:[self activeDocumentLoader]];
}

- (void)_didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)currentWebChallenge forResource:(id)identifier
{
    [client _dispatchDidCancelAuthenticationChallenge:currentWebChallenge forResource:identifier fromDocumentLoader:[self activeDocumentLoader]];
}

- (void)_didReceiveResponse:(NSURLResponse *)r forResource:(id)identifier
{
    [[self activeDocumentLoader] addResponse:r];
    
    [[client webView] _incrementProgressForIdentifier:identifier response:r];
    [client _dispatchResource:identifier didReceiveResponse:r fromDocumentLoader:[self activeDocumentLoader]];
}

- (void)_didReceiveData:(NSData *)data contentLength:(int)lengthReceived forResource:(id)identifier
{
    WebView *webView = [client webView];
    [webView _incrementProgressForIdentifier:identifier data:data];

    [client _dispatchResource:identifier didReceiveContentLength:lengthReceived fromDocumentLoader:[self activeDocumentLoader]];
}

- (void)_didFinishLoadingForResource:(id)identifier
{    
    WebView *webView = [client webView];
    [webView _completeProgressForIdentifier:identifier];
    [client _dispatchResource:identifier didFinishLoadingFromDocumentLoader:[self activeDocumentLoader]];
}

- (void)_didFailLoadingWithError:(NSError *)error forResource:(id)identifier
{
    WebView *webView = [client webView];
        
    [webView _completeProgressForIdentifier:identifier];
        
    if (error)
        [client _dispatchResource:identifier didFailLoadingWithError:error fromDocumentLoader:[self activeDocumentLoader]];
}

- (BOOL)_privateBrowsingEnabled
{
    return [client _privateBrowsingEnabled];
}

- (void)_finishedLoadingResource
{
    [self checkLoadComplete];
}

- (void)_receivedError:(NSError *)error
{
    [self checkLoadComplete];
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
        // Can't call _bridge because we might not have commited yet
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
    [client _dispatchDidCancelClientRedirectForFrame];

    if (!cancelWithLoadInProgress)
        quickRedirectComing = NO;
    
    sentRedirectNotification = NO;
}

- (void)clientRedirectedTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date lockHistory:(BOOL)lockHistory isJavaScriptFormAction:(BOOL)isJavaScriptFormAction
{
    [client _dispatchWillPerformClientRedirectToURL:URL delay:seconds fireDate:date];
    
    // Remember that we sent a redirect notification to the frame load delegate so that when we commit
    // the next provisional load, we can send a corresponding -webView:didCancelClientRedirectForFrame:
    sentRedirectNotification = YES;
    
    // If a "quick" redirect comes in an, we set a special mode so we treat the next
    // load as part of the same navigation.
    
    if (!documentLoader || isJavaScriptFormAction) {
        // If we don't have a dataSource, we have no "original" load on which to base a redirect,
        // so we better just treat the redirect as a normal load.
        quickRedirectComing = NO;
    } else {
        quickRedirectComing = lockHistory;
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
    [self addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:(event != nil || isFormSubmission)];
    if (_loadType == FrameLoadTypeReload)
        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    
    // I believe this is never called with LoadSame.  If it is, we probably want to set the cache
    // policy of LoadFromOrigin, but I didn't test that.
    ASSERT(_loadType != FrameLoadTypeSame);
    
    NSDictionary *action = [self actionInformationForLoadType:_loadType isFormSubmission:isFormSubmission event:event originalURL:URL];
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
                                    withSelector:@selector(continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
        }
        [request release];
        [formState release];
        return;
    }
    
    WebDocumentLoader *oldDocumentLoader = [documentLoader retain];
    
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
        
        [oldDocumentLoader setTriggeringAction:action];
        [self invalidatePendingPolicyDecisionCallingDefaultAction:YES];
        [self checkNavigationPolicyForRequest:request
                                   documentLoader:oldDocumentLoader formState:formState
                                      andCall:self withSelector:@selector(continueFragmentScrollAfterNavigationPolicy:formState:)];
    } else {
        // must grab this now, since this load may stop the previous load and clear this flag
        BOOL isRedirect = quickRedirectComing;
        [self _loadRequest:request triggeringAction:action loadType:_loadType formState:formState];
        if (isRedirect) {
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
    [oldDocumentLoader release];
    [formState release];
}

-(void)continueFragmentScrollAfterNavigationPolicy:(NSURLRequest *)request formState:(WebFormState *)formState
{
    if (!request)
        return;
    
    NSURL *URL = [request URL];
    
    BOOL isRedirect = quickRedirectComing;
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
        [self checkLoadComplete];
    }
 
    [client _dispatchDidChangeLocationWithinPageForFrame];
    [client _didFinishLoad];
}

- (void)closeOldDataSources
{
    // FIXME: is it important for this traversal to be postorder instead of preorder?
    // FIXME: add helpers for postorder traversal?
    for (WebCoreFrameBridge *child = [[client _bridge] firstChild]; child; child = [child nextSibling])
        [[(WebFrameBridge *)child frameLoader] closeOldDataSources];
    
    if (documentLoader)
        [client _dispatchWillCloseFrame];

    [[client webView] setMainFrameDocumentReady:NO];  // stop giving out the actual DOMDocument to observers
}

// Called after we send an openURL:... down to WebCore.
- (void)opened
{
    if ([self loadType] == FrameLoadTypeStandard && [documentLoader isClientRedirect])
        [client _updateHistoryAfterClientRedirect];

    if ([client _isDocumentLoaderLoadingFromPageCache:documentLoader]) {
        // Force a layout to update view size and thereby update scrollbars.
        [client _forceLayout];

        NSArray *responses = [[self documentLoader] responses];
        NSURLResponse *response;
        int i, count = [responses count];
        for (i = 0; i < count; i++) {
            response = [responses objectAtIndex: i];
            // FIXME: If the WebKit client changes or cancels the request, this is not respected.
            NSError *error;
            id identifier;
            NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[response URL]];
            [self requestFromDelegateForRequest:request identifier:&identifier error:&error];
            [self sendRemainingDelegateMessagesWithIdentifier:identifier response:response length:(unsigned)[response expectedContentLength] error:error];
            [request release];
        }
        
        [client _loadedFromPageCache];

        [[self documentLoader] setPrimaryLoadComplete:YES];

        // FIXME: Why only this frame and not parent frames?
        [self checkLoadCompleteForThisFrame];
    }
}

- (void)commitProvisionalLoad:(NSDictionary *)pageCache
{
    bool reload = loadType == FrameLoadTypeReload || loadType == FrameLoadTypeReloadAllowingStaleData;
    
    WebDocumentLoader *pdl = [provisionalDocumentLoader retain];
    
    NSURLResponse *response = [pdl response];
    
    NSDictionary *headers = [response isKindOfClass:[NSHTTPURLResponse class]]
        ? [(NSHTTPURLResponse *)response allHeaderFields] : nil;
    
    if (loadType != FrameLoadTypeReplace)
        [self closeOldDataSources];
    
    if (!pageCache)
        [client _makeRepresentationForDocumentLoader:pdl];
    
    [self transitionToCommitted:pageCache];
    
    // Call -_clientRedirectCancelledOrFinished: here so that the frame load delegate is notified that the redirect's
    // status has changed, if there was a redirect.  The frame load delegate may have saved some state about
    // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:.  Since we are
    // just about to commit a new page, there cannot possibly be a pending redirect at this point.
    if (sentRedirectNotification)
        [self clientRedirectCancelledOrFinished:NO];
    
    NSURL *baseURL = [[provisionalDocumentLoader request] _webDataRequestBaseURL];        
    NSURL *URL = baseURL ? baseURL : [response URL];
    
    if (!URL || [URL _web_isEmpty])
        URL = [NSURL URLWithString:@"about:blank"];    
    
    [[client _bridge] openURL:URL
                     reload:reload 
                contentType:[response MIMEType]
                    refresh:[headers objectForKey:@"Refresh"]
               lastModified:(pageCache ? nil : wkGetNSURLResponseLastModifiedDate(response))
                  pageCache:pageCache];
    
    [self opened];
    
    [pdl release];
}

- (NSURLRequest *)initialRequest
{
    return [[self activeDocumentLoader] initialRequest];
}

- (void)_receivedData:(NSData *)data
{
    [[self activeDocumentLoader] receivedData:data];
}

- (void)_setRequest:(NSURLRequest *)request
{
    [[self activeDocumentLoader] setRequest:request];
}

- (void)_downloadWithLoadingConnection:(NSURLConnection *)connection
    request:(NSURLRequest *)request response:(NSURLResponse *)response proxy:(id)proxy
{
    [client _downloadWithLoadingConnection:connection request:request response:response proxy:proxy];
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
    WebDocumentLoader *dl = [self activeDocumentLoader];
    
    WebFrameBridge *bridge = [self bridge];

    [bridge retain];
    [dl finishedLoading];

    if ([dl mainDocumentError] || ![dl frameLoader]) {
        [bridge release];
        return;
    }

    [dl setPrimaryLoadComplete:YES];
    [client _dispatchDidLoadMainResourceForDocumentLoader:dl];
    [self checkLoadComplete];

    [bridge release];
}

- (void)_notifyIconChanged:(NSURL *)iconURL
{
    ASSERT([[WebIconDatabase sharedIconDatabase] _isEnabled]);
    ASSERT(client == [[client webView] mainFrame]);

    [[client webView] _willChangeValueForKey:_WebMainFrameIconKey];
    
    NSImage *icon = [[WebIconDatabase sharedIconDatabase] iconForURL:[[[self activeDocumentLoader] URL] _web_originalDataAsString] withSize:WebIconSmallSize];
    
    [client _dispatchDidReceiveIcon:icon];
    
    [[client webView] _didChangeValueForKey:_WebMainFrameIconKey];
}

- (NSURL *)_URL
{
    return [[self activeDocumentLoader] URL];
}

- (NSError *)cancelledErrorWithRequest:(NSURLRequest *)request
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:[request URL]];
}

- (NSError *)fileDoesNotExistErrorWithResponse:(NSURLResponse *)response
{
    return [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist URL:[response URL]];    
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

- (BOOL)_canUseResourceWithResponse:(NSURLResponse *)response
{
    if (wkGetNSURLResponseMustRevalidate(response))
        return NO;
    if (wkGetNSURLResponseCalculatedExpiration(response) - CFAbsoluteTimeGetCurrent() < 1)
        return NO;
    return YES;
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
        WebResource *resource = [client _archivedSubresourceForURL:originalURL fromDocumentLoader:[self activeDocumentLoader]];
        if (resource && [self _canUseResourceWithResponse:[resource _response]]) {
            CFDictionarySetValue((CFMutableDictionaryRef)[self pendingArchivedResources], loader, resource);
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
                           documentLoader:[self activeDocumentLoader]
                                formState:nil
                                  andCall:obj
                             withSelector:sel];
}

- (void)_checkContentPolicyForMIMEType:(NSString *)MIMEType andCall:(id)obj withSelector:(SEL)sel
{
    WebPolicyDecisionListener *l = [[WebPolicyDecisionListener alloc] _initWithTarget:obj action:sel];
    listener = l;
    
    [l retain];

    [client _dispatchDecidePolicyForMIMEType:MIMEType request:[[self activeDocumentLoader] request] decisionListener:listener];

    [l release];
}

- (void)cancelContentPolicy
{
    [listener _invalidate];
    [listener release];
    listener = nil;
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
    WebDocumentLoader *compareDocumentLoader = nil;
    if (delegateIsDecidingNavigationPolicy || delegateIsHandlingUnimplementablePolicy)
        compareDocumentLoader = policyDocumentLoader;
    else if (delegateIsHandlingProvisionalLoadError)
        compareDocumentLoader = [self provisionalDocumentLoader];
    
    return compareDocumentLoader != nil && [unreachableURL isEqual:[[compareDocumentLoader request] URL]];
}

- (void)_loadRequest:(NSURLRequest *)request archive:(WebArchive *)archive
{
    FrameLoadType type;
    
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:request];
    WebDataSource *newDataSource = [client _dataSourceForDocumentLoader:policyDocumentLoader];

    NSMutableURLRequest *r = [newDataSource request];
    [self addExtraFieldsToRequest:r mainResource:YES alwaysFromRequest:NO];
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
    
    [self loadDocumentLoader:policyDocumentLoader withLoadType:type formState:nil];
}

- (void)_loadRequest:(NSURLRequest *)request triggeringAction:(NSDictionary *)action loadType:(FrameLoadType)type formState:(WebFormState *)formState
{
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:request];

    [policyDocumentLoader setTriggeringAction:action];
    [policyDocumentLoader setOverrideEncoding:[[self documentLoader] overrideEncoding]];

    [self loadDocumentLoader:policyDocumentLoader withLoadType:type formState:formState];
}

- (void)_reloadAllowingStaleDataWithOverrideEncoding:(NSString *)encoding
{
    if (documentLoader == nil)
        return;

    NSMutableURLRequest *request = [[documentLoader request] mutableCopy];
    NSURL *unreachableURL = [documentLoader unreachableURL];
    if (unreachableURL != nil)
        [request setURL:unreachableURL];

    [request setCachePolicy:NSURLRequestReturnCacheDataElseLoad];
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:request];
    [request release];
    
    [policyDocumentLoader setOverrideEncoding:encoding];

    [self loadDocumentLoader:policyDocumentLoader withLoadType:FrameLoadTypeReloadAllowingStaleData formState:nil];
}

- (void)reload
{
    if (documentLoader == nil)
        return;

    NSMutableURLRequest *initialRequest = [documentLoader request];
    
    // If a window is created by javascript, its main frame can have an empty but non-nil URL.
    // Reloading in this case will lose the current contents (see 4151001).
    if ([[[[documentLoader request] URL] absoluteString] length] == 0)
        return;

    // Replace error-page URL with the URL we were trying to reach.
    NSURL *unreachableURL = [initialRequest _webDataRequestUnreachableURL];
    if (unreachableURL != nil)
        initialRequest = [NSURLRequest requestWithURL:unreachableURL];
    
    ASSERT(!policyDocumentLoader);
    policyDocumentLoader = [client _createDocumentLoaderWithRequest:initialRequest];
    NSMutableURLRequest *request = [policyDocumentLoader request];

    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];

    // If we're about to rePOST, set up action so the app can warn the user
    if (isCaseInsensitiveEqual([request HTTPMethod], @"POST")) {
        NSDictionary *action = [self actionInformationForNavigationType:WebNavigationTypeFormResubmitted
            event:nil originalURL:[request URL]];
        [policyDocumentLoader setTriggeringAction:action];
    }

    [policyDocumentLoader setOverrideEncoding:[documentLoader overrideEncoding]];
    
    [self loadDocumentLoader:policyDocumentLoader withLoadType:FrameLoadTypeReload formState:nil];
}

- (void)didReceiveServerRedirectForProvisionalLoadForFrame
{
    [client _dispatchDidReceiveServerRedirectForProvisionalLoadForFrame];
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
    [client _dispatchDidLoadMainResourceForDocumentLoader:[self activeDocumentLoader]];
    [self checkLoadComplete];
}

- (void)finalSetupForReplaceWithDocumentLoader:(WebDocumentLoader *)loader
{
    [[client _dataSourceForDocumentLoader:loader] _clearUnarchivingState];
}

- (void)prepareForLoadStart
{
    [[client webView] _progressStarted:client];
    [[client webView] _didStartProvisionalLoadForFrame:client];
    [client _dispatchDidStartProvisionalLoadForFrame];
}

- (BOOL)subframeIsLoading
{
    // It's most likely that the last added frame is the last to load so we walk backwards.
    for (WebFrame *frame = [client _lastChildFrame]; frame; frame = [frame _previousSiblingFrame])
        if ([[frame dataSource] isLoading] || [[frame provisionalDataSource] isLoading])
            return YES;
    return NO;
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
            // Must update the entries in the back-forward list too.
            // This must go through the WebFrame because it has the right notion of the current b/f item.
            [client _setTitle:[loader title] forURL:URLForHistory];
            [[client webView] setMainFrameDocumentReady:YES]; // update observers with new DOMDocument

            [client _dispatchDidReceiveTitle:[loader title]];
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
        _initWithTarget:self action:@selector(continueAfterNewWindowPolicy:)];

    policyRequest = [request retain];
    policyTarget = [target retain];
    policyFrameName = [frameName retain];
    policySelector = selector;
    listener = [decisionListener retain];
    policyFormState = [formState retain];

    [client _dispatchDecidePolicyForNewWindowAction:action request:request newFrameName:frameName decisionListener:decisionListener];
    
    [decisionListener release];
}

- (void)continueAfterNewWindowPolicy:(WebPolicyAction)policy
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
                         documentLoader:(WebDocumentLoader *)loader
                              formState:(WebFormState *)formState
                                andCall:(id)target
                           withSelector:(SEL)selector
{
    NSDictionary *action = [loader triggeringAction];
    if (action == nil) {
        action = [self actionInformationForNavigationType:WebNavigationTypeOther
            event:nil originalURL:[request URL]];
        [loader setTriggeringAction:action];
    }
        
    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if ([request isEqual:[loader lastCheckedRequest]] || [[request URL] _web_isEmpty]) {
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
    
    [loader setLastCheckedRequest:request];

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

    delegateIsDecidingNavigationPolicy = YES;
    [client _dispatchDecidePolicyForNavigationAction:action request:request decisionListener:decisionListener];
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
    ASSERT(policyDocumentLoader || [provisionalDocumentLoader unreachableURL] != nil);

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
    WebDocumentLoader *dl = [policyDocumentLoader retain];
    
    [self stopLoading];
    loadType = type;

    [self startProvisionalLoad:dl];

    [dl release];
    [self setPolicyDocumentLoader:nil];
    
    if (isBackForwardLoadType(type)) {
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

- (void)loadDocumentLoader:(WebDocumentLoader *)loader withLoadType:(FrameLoadType)type formState:(WebFormState *)formState
{
    ASSERT([client webView] != nil);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT([client frameView] != nil);

    policyLoadType = type;

    WebFrame *parentFrame = [client parentFrame];
    if (parentFrame)
        [loader setOverrideEncoding:[[[parentFrame dataSource] _documentLoader] overrideEncoding]];

    [loader setFrameLoader:self];

    [self invalidatePendingPolicyDecisionCallingDefaultAction:YES];

    [self setPolicyDocumentLoader:loader];

    [self checkNavigationPolicyForRequest:[loader request]
                           documentLoader:loader
                                formState:formState
                                  andCall:self
                             withSelector:@selector(continueLoadRequestAfterNavigationPolicy:formState:)];
}

- (void)handleUnimplementablePolicyWithErrorCode:(int)code forURL:(NSURL *)URL
{
    NSError *error = [NSError _webKitErrorWithDomain:WebKitErrorDomain code:code URL:URL];
    delegateIsHandlingUnimplementablePolicy = YES;
    [client _dispatchUnableToImplementPolicyWithError:error];
    delegateIsHandlingUnimplementablePolicy = NO;
}

- (void)didFirstLayout
{
    if ([[client webView] backForwardList]) {
        if (isBackForwardLoadType(loadType))
            [client _restoreScrollPositionAndViewState];
    }

    firstLayoutDone = YES;
    [client _dispatchDidFirstLayoutInFrame];
}

- (void)frameLoadCompleted
{
    [client _frameLoadCompleted];

    // After a canceled provisional load, firstLayoutDone is NO. Reset it to YES if we're displaying a page.
    if (documentLoader)
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

- (void)transitionToCommitted:(NSDictionary *)pageCache
{
    ASSERT([client webView] != nil);
    
    switch ([self state]) {
        case WebFrameStateProvisional:
            goto keepGoing;
        
        case WebFrameStateCommittedPage:
        case WebFrameStateComplete:
            break;
    }

    ASSERT_NOT_REACHED();
    return;

keepGoing:

    [client _setCopiesOnScroll];
    [client _updateHistoryForCommit];

    // The call to closeURL invokes the unload event handler, which can execute arbitrary
    // JavaScript. If the script initiates a new load, we need to abandon the current load,
    // or the two will stomp each other.
    WebDocumentLoader *pdl = provisionalDocumentLoader;
    [[client _bridge] closeURL];
    if (pdl != provisionalDocumentLoader)
        return;

    [self commitProvisionalLoad];

    // Handle adding the URL to the back/forward list.
    WebDocumentLoader *dl = documentLoader;
    NSString *ptitle = [dl title];

    switch (loadType) {
    case WebFrameLoadTypeForward:
    case WebFrameLoadTypeBack:
    case WebFrameLoadTypeIndexedBackForward:
        if ([[client webView] backForwardList]) {
            [client _updateHistoryForBackForwardNavigation];

            // Create a document view for this document, or used the cached view.
            if (pageCache)
                [client _setDocumentViewFromPageCache:pageCache];
            else
                [client _makeDocumentView];
        }
        break;

    case WebFrameLoadTypeReload:
    case WebFrameLoadTypeSame:
    case WebFrameLoadTypeReplace:
        [client _updateHistoryForReload];
        [client _makeDocumentView];
        break;

    // FIXME - just get rid of this case, and merge WebFrameLoadTypeReloadAllowingStaleData with the above case
    case WebFrameLoadTypeReloadAllowingStaleData:
        [client _makeDocumentView];
        break;

    case WebFrameLoadTypeStandard:
        [client _updateHistoryForStandardLoad];
        [client _makeDocumentView];
        break;

    case WebFrameLoadTypeInternal:
        [client _updateHistoryForInternalLoad];
        [client _makeDocumentView];
        break;

    // FIXME Remove this check when dummy ds is removed (whatever "dummy ds" is).
    // An exception should be thrown if we're in the WebFrameLoadTypeUninitialized state.
    default:
        ASSERT_NOT_REACHED();
    }

    // Tell the client we've committed this URL.
    ASSERT([[client frameView] documentView] != nil);
    [[client webView] _didCommitLoadForFrame:client];
    [client _dispatchDidCommitLoadForFrame];
    
    // If we have a title let the WebView know about it.
    if (ptitle)
        [client _dispatchDidReceiveTitle:ptitle];
}

- (void)checkLoadCompleteForThisFrame
{
    ASSERT([client webView] != nil);

    switch ([self state]) {
        case WebFrameStateProvisional: {
            if (delegateIsHandlingProvisionalLoadError)
                return;

            WebDocumentLoader *pdl = [provisionalDocumentLoader retain];

            // If we've received any errors we may be stuck in the provisional state and actually complete.
            NSError *error = [pdl mainDocumentError];
            if (error != nil) {
                // Check all children first.
                LoadErrorResetToken *resetToken = [client _tokenForLoadErrorReset];
                BOOL shouldReset = YES;
                if (![pdl isLoadingInAPISense]) {
                    [[client webView] _didFailProvisionalLoadWithError:error forFrame:client];
                    delegateIsHandlingProvisionalLoadError = YES;
                    [client _dispatchDidFailProvisionalLoadWithError:error];

                    delegateIsHandlingProvisionalLoadError = NO;
                    [[client _internalLoadDelegate] webFrame:client didFinishLoadWithError:error];

                    // FIXME: can stopping loading here possibly have
                    // any effect, if isLoading is false, which it
                    // must be, to be in this branch of the if? And is it ok to just do 
                    // a full-on stopLoading?
                    [self stopLoadingSubframes];
                    [pdl stopLoading];

                    // Finish resetting the load state, but only if another load hasn't been started by the
                    // delegate callback.
                    if (pdl == provisionalDocumentLoader)
                        [self clearProvisionalLoad];
                    else {
                        NSURL *unreachableURL = [provisionalDocumentLoader unreachableURL];
                        if (unreachableURL != nil && [unreachableURL isEqual:[[pdl request] URL]])
                            shouldReset = NO;
                    }
                }
                if (shouldReset)
                    [client _resetAfterLoadError:resetToken];
                else
                    [client _doNotResetAfterLoadError:resetToken];
            }
            [pdl release];
            return;
        }
        
        case WebFrameStateCommittedPage: {
            WebDocumentLoader *dl = documentLoader;
            
            if (![dl isLoadingInAPISense]) {
                [self markLoadComplete];

                // FIXME: Is this subsequent work important if we already navigated away?
                // Maybe there are bugs because of that, or extra work we can skip because
                // the new page is ready.

                [client _forceLayoutForNonHTML];
                 
                // If the user had a scroll point scroll to it.  This will override
                // the anchor point.  After much discussion it was decided by folks
                // that the user scroll point should override the anchor point.
                if ([[client webView] backForwardList]) {
                    switch ([self loadType]) {
                    case WebFrameLoadTypeForward:
                    case WebFrameLoadTypeBack:
                    case WebFrameLoadTypeIndexedBackForward:
                    case WebFrameLoadTypeReload:
                        [client _restoreScrollPositionAndViewState];
                        break;

                    case WebFrameLoadTypeStandard:
                    case WebFrameLoadTypeInternal:
                    case WebFrameLoadTypeReloadAllowingStaleData:
                    case WebFrameLoadTypeSame:
                    case WebFrameLoadTypeReplace:
                        // Do nothing.
                        break;

                    default:
                        ASSERT_NOT_REACHED();
                        break;
                    }
                }

                NSError *error = [dl mainDocumentError];
                if (error != nil) {
                    [[client webView] _didFailLoadWithError:error forFrame:client];
                    [client _dispatchDidFailLoadWithError:error];
                    [[client _internalLoadDelegate] webFrame:client didFinishLoadWithError:error];
                } else {
                    [[client webView] _didFinishLoadForFrame:client];
                    [client _dispatchDidFinishLoadForFrame];
                    [[client _internalLoadDelegate] webFrame:client didFinishLoadWithError:nil];
                }
                
                [[client webView] _progressCompleted:client];
 
                return;
            }
            return;
        }
        
        case WebFrameStateComplete:
            // Even if already complete, we might have set a previous item on a frame that
            // didn't do any data loading on the past transaction. Make sure to clear these out.
            [client _frameLoadCompleted];
            return;
    }

    ASSERT_NOT_REACHED();
}

- (void)continueLoadRequestAfterNewWindowPolicy:(NSURLRequest *)request frameName:(NSString *)frameName formState:(WebFormState *)formState
{
    if (!request)
        return;

    WebFrameBridge *bridge = [self bridge];
    [bridge retain];

    WebFrame *mainFrame = [client _dispatchCreateWebViewWithRequest:nil];
    if (!mainFrame)
        goto exit;

    WebFrameBridge *mainBridge = [mainFrame _bridge];
    [mainBridge retain];

    [mainBridge setName:frameName];

    [mainFrame _dispatchShow];

    [mainBridge setOpener:bridge];
    [[mainFrame _frameLoader] _loadRequest:request triggeringAction:nil loadType:WebFrameLoadTypeStandard formState:formState];

    [mainBridge release];

exit:
    [bridge release];
}

- (void)sendRemainingDelegateMessagesWithIdentifier:(id)identifier response:(NSURLResponse *)response length:(unsigned)length error:(NSError *)error 
{    
    if (response != nil)
        [client _dispatchResource:identifier didReceiveResponse:response fromDocumentLoader:documentLoader];
    
    if (length > 0)
        [client _dispatchResource:identifier didReceiveContentLength:(WebNSUInteger)length fromDocumentLoader:documentLoader];
    
    if (error == nil)
        [client _dispatchResource:identifier didFinishLoadingFromDocumentLoader:documentLoader];
    else
        [client _dispatchResource:identifier didFailLoadingWithError:error fromDocumentLoader:documentLoader];
}

- (NSURLRequest *)requestFromDelegateForRequest:(NSURLRequest *)request identifier:(id *)identifier error:(NSError **)error
{
    ASSERT(request != nil);
    
    *identifier = [client _dispatchIdentifierForInitialRequest:request fromDocumentLoader:documentLoader]; 
    NSURLRequest *newRequest = [client _dispatchResource:*identifier willSendRequest:request redirectResponse:nil fromDocumentLoader:documentLoader];
    
    if (newRequest == nil)
        *error = [NSError _webKitErrorWithDomain:NSURLErrorDomain code:NSURLErrorCancelled URL:[request URL]];
    else
        *error = nil;
    
    return newRequest;
}

- (void)loadRequest:(NSURLRequest *)request inFrameNamed:(NSString *)frameName
{
    if (frameName == nil) {
        [client loadRequest:request];
        return;
    }

    WebFrame *frame = [client findFrameNamed:frameName];
    if (frame != nil) {
        [frame loadRequest:request];
        return;
    }

    NSDictionary *action = [self actionInformationForNavigationType:WebNavigationTypeOther
        event:nil originalURL:[request URL]];
    [self checkNewWindowPolicyForRequest:request action:action frameName:frameName formState:nil
        andCall:self withSelector:@selector(continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
}

- (void)postWithURL:(NSURL *)URL referrer:(NSString *)referrer target:(NSString *)target
    data:(NSArray *)postData contentType:(NSString *)contentType
    triggeringEvent:(NSEvent *)event form:(DOMElement *)form formValues:(NSDictionary *)values
{
    // When posting, use the NSURLRequestReloadIgnoringCacheData load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.

    // FIXME: Where's the code that implements what the comment above says?

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    [self addExtraFieldsToRequest:request mainResource:YES alwaysFromRequest:YES];
    [request _web_setHTTPReferrer:referrer];
    [request setHTTPMethod:@"POST"];
    webSetHTTPBody(request, postData);
    [request _web_setHTTPContentType:contentType];

    NSDictionary *action = [self actionInformationForLoadType:FrameLoadTypeStandard isFormSubmission:YES event:event originalURL:URL];
    WebFormState *formState = nil;
    if (form && values)
        formState = [[WebFormState alloc] initWithForm:form values:values sourceFrame:client];

    if (target != nil) {
        WebFrame *targetFrame = [client findFrameNamed:target];
        if (targetFrame != nil)
            [[targetFrame _frameLoader] _loadRequest:request triggeringAction:action loadType:FrameLoadTypeStandard formState:formState];
        else
            [self checkNewWindowPolicyForRequest:request action:action frameName:target formState:formState
                andCall:self withSelector:@selector(continueLoadRequestAfterNewWindowPolicy:frameName:formState:)];
    } else
        [self _loadRequest:request triggeringAction:action loadType:FrameLoadTypeStandard formState:formState];

    [request release];
    [formState release];
}

- (void)detachChildren
{
    // FIXME: is it really necessary to do this in reverse order any more?
    WebFrame *child = [client _lastChildFrame];
    WebFrame *prev = [child _previousSiblingFrame];
    for (; child; child = prev, prev = [child _previousSiblingFrame])
        [[child _frameLoader] detachFromParent];
}

- (void)detachFromParent
{
    WebFrameBridge *bridge = [[self bridge] retain];

    [bridge closeURL];
    [self stopLoading];
    [client _detachedFromParent1];
    [self detachChildren];
    [client _detachedFromParent2];
    [self setDocumentLoader:nil];
    [client _detachedFromParent3];
    [[[client parentFrame] _bridge] removeChild:bridge];

    NSObject <WebFrameLoaderClient>* c = [client retain];
    [bridge close];
    [c _detachedFromParent4];
    [c release];

    [bridge release];
}

- (void)addExtraFieldsToRequest:(NSMutableURLRequest *)request mainResource:(BOOL)mainResource alwaysFromRequest:(BOOL)f
{
    [request _web_setHTTPUserAgent:[[client webView] userAgentForURL:[request URL]]];
    
    if ([self loadType] == FrameLoadTypeReload)
        [request setValue:@"max-age=0" forHTTPHeaderField:@"Cache-Control"];
    
    // Don't set the cookie policy URL if it's already been set.
    if ([request mainDocumentURL] == nil) {
        if (mainResource && (client == [[client webView] mainFrame] || f))
            [request setMainDocumentURL:[request URL]];
        else
            [request setMainDocumentURL:[[[[client webView] mainFrame] dataSource] _URL]];
    }
    
    if (mainResource)
        [request setValue:@"text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5" forHTTPHeaderField:@"Accept"];
}

- (void)safeLoadURL:(NSURL *)URL
{
   // Call the bridge because this is where our security checks are made.
    [[self bridge] loadURL:URL 
                  referrer:[[[documentLoader request] URL] _web_originalDataAsString]
                    reload:NO
               userGesture:YES       
                    target:nil
           triggeringEvent:[NSApp currentEvent]
                      form:nil 
                formValues:nil];
}

- (NSDictionary *)actionInformationForLoadType:(FrameLoadType)type isFormSubmission:(BOOL)isFormSubmission event:(NSEvent *)event originalURL:(NSURL *)URL
{
    WebNavigationType navType;
    if (isFormSubmission) {
        navType = WebNavigationTypeFormSubmitted;
    } else if (event == nil) {
        if (type == FrameLoadTypeReload)
            navType = WebNavigationTypeReload;
        else if (isBackForwardLoadType(type))
            navType = WebNavigationTypeBackForward;
        else
            navType = WebNavigationTypeOther;
    } else {
        navType = WebNavigationTypeLinkClicked;
    }
    return [self actionInformationForNavigationType:navType event:event originalURL:URL];
}

- (NSDictionary *)actionInformationForNavigationType:(NavigationType)navigationType event:(NSEvent *)event originalURL:(NSURL *)URL
{
    switch ([event type]) {
        case NSLeftMouseDown:
        case NSRightMouseDown:
        case NSOtherMouseDown:
        case NSLeftMouseUp:
        case NSRightMouseUp:
        case NSOtherMouseUp: {
            NSView *topViewInEventWindow = [[event window] contentView];
            NSView *viewContainingPoint = [topViewInEventWindow hitTest:[topViewInEventWindow convertPoint:[event locationInWindow] fromView:nil]];
            while (viewContainingPoint != nil) {
                if ([viewContainingPoint isKindOfClass:[WebView class]])
                    break;
                viewContainingPoint = [viewContainingPoint superview];
            }
            if (viewContainingPoint != nil) {
                NSPoint point = [viewContainingPoint convertPoint:[event locationInWindow] fromView:nil];
                NSDictionary *elementInfo = [(WebView *)viewContainingPoint elementAtPoint:point];
                return [NSDictionary dictionaryWithObjectsAndKeys:
                    [NSNumber numberWithInt:navigationType], WebActionNavigationTypeKey,
                    elementInfo, WebActionElementKey,
                    [NSNumber numberWithInt:[event buttonNumber]], WebActionButtonKey,
                    [NSNumber numberWithInt:[event modifierFlags]], WebActionModifierFlagsKey,
                    URL, WebActionOriginalURLKey,
                    nil];
            }
        }
            
        // fall through
        
        default:
            return [NSDictionary dictionaryWithObjectsAndKeys:
                [NSNumber numberWithInt:navigationType], WebActionNavigationTypeKey,
                [NSNumber numberWithInt:[event modifierFlags]], WebActionModifierFlagsKey,
                URL, WebActionOriginalURLKey,
                nil];
    }
}

// Called every time a resource is completely loaded, or an error is received.
- (void)checkLoadComplete
{
    ASSERT([client webView] != nil);

    WebFrame *parent;
    for (WebFrame *frame = client; frame; frame = parent) {
        [frame retain];
        [[frame _frameLoader] checkLoadCompleteForThisFrame];
        parent = [frame parentFrame];
        [frame release];
    }
}

@end
