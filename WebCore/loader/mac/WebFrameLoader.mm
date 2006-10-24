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

#import "config.h"
#import "WebFrameLoader.h"

#import "DOMElementInternal.h"
#import "Element.h"
#import "FrameLoadRequest.h"
#import "FrameMac.h"
#import "FrameTree.h"
#import "HTMLNames.h"
#import "LoaderNSURLExtras.h"
#import "LoaderNSURLRequestExtras.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreIconDatabaseBridge.h"
#import "WebCoreSystemInterface.h"
#import "WebDataProtocol.h"
#import "WebDocumentLoader.h"
#import "WebFormDataStream.h"
#import "WebFormState.h"
#import "WebFrameLoaderClient.h"
#import "WebMainResourceLoader.h"
#import "WebPolicyDecider.h"
#import <objc/objc-runtime.h>
#import <wtf/Assertions.h>

using namespace WebCore;

@interface WebCoreFrameLoaderAsDelegate : NSObject
{
    FrameLoader* m_loader;
}
- (id)initWithLoader:(FrameLoader*)loader;
- (void)detachFromLoader;
@end

namespace WebCore {

using namespace HTMLNames;

typedef HashSet<RefPtr<WebCore::WebResourceLoader> > ResourceLoaderSet;

static double storedTimeOfLastCompletedLoad;

static bool isCaseInsensitiveEqual(NSString *a, NSString *b)
{
    return [a caseInsensitiveCompare:b] == NSOrderedSame;
}

static void cancelAll(const ResourceLoaderSet& loaders)
{
    const ResourceLoaderSet copy = loaders;
    ResourceLoaderSet::const_iterator end = copy.end();
    for (ResourceLoaderSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->cancel();
}

bool isBackForwardLoadType(FrameLoadType type)
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

FrameLoader::FrameLoader(Frame* frame)
    : m_frame(frame)
    , m_client(nil)
    , m_state(WebFrameStateCommittedPage)
    , m_loadType(FrameLoadTypeStandard)
    , m_delegateIsHandlingProvisionalLoadError(false)
    , m_delegateIsDecidingNavigationPolicy(false)
    , m_delegateIsHandlingUnimplementablePolicy(false)
    , m_firstLayoutDone(false)
    , m_quickRedirectComing(false)
    , m_sentRedirectNotification(false)
    , m_isStoppingLoad(false)
{
}

FrameLoader::~FrameLoader()
{
    [m_asDelegate.get() detachFromLoader];
}

void FrameLoader::prepareForLoadStart()
{
    [m_client _progressStarted];
    [m_client _dispatchDidStartProvisionalLoadForFrame];
}

void FrameLoader::setupForReplace()
{
    setState(WebFrameStateProvisional);
    m_provisionalDocumentLoader = m_documentLoader;
    m_documentLoader = nil;
    detachChildren();
}

void FrameLoader::setupForReplaceByMIMEType(NSString *newMIMEType)
{
    activeDocumentLoader()->setupForReplaceByMIMEType(newMIMEType);
}

void FrameLoader::finalSetupForReplace(DocumentLoader* loader)
{
    [m_client _clearUnarchivingStateForLoader:loader];
}

void FrameLoader::safeLoad(NSURL *URL)
{
    // Call to the Frame because this is where our security checks are made.
    FrameLoadRequest request;
    request.m_request.setURL(URL);
    request.m_request.setHTTPReferrer(urlOriginalDataAsString([m_documentLoader->request() URL]));
    
    [bridge() impl]->loadRequest(request, true, [NSApp currentEvent], nil, nil);
}

void FrameLoader::load(NSURLRequest *request)
{
    // FIXME: is this the right place to reset loadType? Perhaps this should be done after loading is finished or aborted.
    m_loadType = FrameLoadTypeStandard;
    load([m_client _createDocumentLoaderWithRequest:request].get());
}

void FrameLoader::load(NSURLRequest *request, NSString *frameName)
{
    if (!frameName) {
        load(request);
        return;
    }

    Frame* frame = m_frame->tree()->find(frameName);
    if (frame) {
        [Mac(frame)->bridge() frameLoader]->load(request);
        return;
    }

    NSDictionary *action = actionInformation(NavigationTypeOther, nil, [request URL]);
    checkNewWindowPolicy(request, action, frameName, 0);
}

void FrameLoader::load(NSURLRequest *request, NSDictionary *action, FrameLoadType type, PassRefPtr<FormState> formState)
{
    RefPtr<DocumentLoader> loader = [m_client _createDocumentLoaderWithRequest:request];
    setPolicyDocumentLoader(loader.get());

    loader->setTriggeringAction(action);
    if (m_documentLoader)
        loader->setOverrideEncoding(m_documentLoader->overrideEncoding());

    load(loader.get(), type, formState);
}

void FrameLoader::load(NSURL *URL, NSString *referrer, FrameLoadType newLoadType, NSString *target, NSEvent *event, DOMElement *form, NSDictionary *values)
{
    bool isFormSubmission = values != nil;
    
    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    setHTTPReferrer(request, referrer);
    addExtraFieldsToRequest(request, true, event || isFormSubmission);
    if (newLoadType == FrameLoadTypeReload)
        [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];

    ASSERT(newLoadType != FrameLoadTypeSame);

    NSDictionary *action = actionInformation(newLoadType, isFormSubmission, event, URL);
    RefPtr<FormState> formState;
    if (form && values)
        formState = FormState::create(form, values, bridge());
    
    if (target) {
        Frame* targetFrame = m_frame->tree()->find(target);
        if (targetFrame)
            [Mac(targetFrame)->bridge() frameLoader]->load(URL, referrer, newLoadType, nil, event, form, values);
        else
            checkNewWindowPolicy(request, action, target, formState.release());
        [request release];
        return;
    }

    RefPtr<DocumentLoader> oldDocumentLoader = m_documentLoader;

    bool sameURL = [m_client _shouldTreatURLAsSameAsCurrent:URL];
    
    // Make sure to do scroll to anchor processing even if the URL is
    // exactly the same so pages with '#' links and DHTML side effects
    // work properly.
    if (!isFormSubmission
        && newLoadType != FrameLoadTypeReload
        && newLoadType != FrameLoadTypeSame
        && !shouldReload(URL, [bridge() URL])
        // We don't want to just scroll if a link from within a
        // frameset is trying to reload the frameset into _top.
        && ![bridge() isFrameSet]) {

        // Just do anchor navigation within the existing content.
        
        // We don't do this if we are submitting a form, explicitly reloading,
        // currently displaying a frameset, or if the new URL does not have a fragment.
        // These rules are based on what KHTML was doing in KHTMLPart::openURL.
        
        // FIXME: What about load types other than Standard and Reload?
        
        oldDocumentLoader->setTriggeringAction(action);
        invalidatePendingPolicyDecision(true);
        checkNavigationPolicy(request, oldDocumentLoader.get(), formState.release(),
            asDelegate(), @selector(continueFragmentScrollAfterNavigationPolicy:formState:));
    } else {
        // must grab this now, since this load may stop the previous load and clear this flag
        bool isRedirect = m_quickRedirectComing;
        load(request, action, newLoadType, formState.release());
        if (isRedirect) {
            m_quickRedirectComing = false;
            m_provisionalDocumentLoader->setIsClientRedirect(true);
        } else if (sameURL)
            // Example of this case are sites that reload the same URL with a different cookie
            // driving the generated content, or a master frame with links that drive a target
            // frame, where the user has clicked on the same link repeatedly.
            m_loadType = FrameLoadTypeSame;
    }
    
    [request release];
}

void FrameLoader::load(DocumentLoader* newDocumentLoader)
{
    invalidatePendingPolicyDecision(true);
    setPolicyDocumentLoader(newDocumentLoader);

    NSMutableURLRequest *r = newDocumentLoader->request();
    addExtraFieldsToRequest(r, true, false);
    FrameLoadType type;
    if ([m_client _shouldTreatURLAsSameAsCurrent:[newDocumentLoader->originalRequest() URL]]) {
        [r setCachePolicy:NSURLRequestReloadIgnoringCacheData];
        type = FrameLoadTypeSame;
    } else
        type = FrameLoadTypeStandard;

    if (m_documentLoader)
        newDocumentLoader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    // When we loading alternate content for an unreachable URL that we're
    // visiting in the b/f list, we treat it as a reload so the b/f list 
    // is appropriately maintained.
    if (shouldReloadToHandleUnreachableURL(newDocumentLoader->originalRequest())) {
        ASSERT(type == FrameLoadTypeStandard);
        type = FrameLoadTypeReload;
    }

    load(newDocumentLoader, type, 0);
}

void FrameLoader::load(DocumentLoader* loader, FrameLoadType type, PassRefPtr<FormState> formState)
{
    ASSERT([m_client _hasWebView]);

    // Unfortunately the view must be non-nil, this is ultimately due
    // to parser requiring a FrameView.  We should fix this dependency.

    ASSERT([m_client _hasFrameView]);

    m_policyLoadType = type;

    if (Frame* parent = m_frame->tree()->parent())
        loader->setOverrideEncoding([Mac(parent)->bridge() frameLoader]->documentLoader()->overrideEncoding());

    invalidatePendingPolicyDecision(true);
    setPolicyDocumentLoader(loader);

    checkNavigationPolicy(loader->request(), loader, formState,
        asDelegate(), @selector(continueLoadRequestAfterNavigationPolicy:formState:));
}

bool FrameLoader::startLoadingMainResource(NSMutableURLRequest *request, id identifier)
{
    ASSERT(!m_mainResourceLoader);
    m_mainResourceLoader = MainResourceLoader::create(m_frame);
    m_mainResourceLoader->setIdentifier(identifier);
    addExtraFieldsToRequest(request, true, false);
    if (!m_mainResourceLoader->load(request)) {
        // FIXME: If this should really be caught, we should just ASSERT this doesn't happen;
        // should it be caught by other parts of WebKit or other parts of the app?
        LOG_ERROR("could not create WebResourceHandle for URL %@ -- should be caught by policy handler level", [request URL]);
        m_mainResourceLoader = 0;
        return false;
    }
    return true;
}

// FIXME: Poor method name; also, why is this not part of startProvisionalLoad:?
void FrameLoader::startLoading()
{
    m_provisionalDocumentLoader->prepareForLoadStart();

    if (isLoadingMainResource())
        return;

    [m_client _clearLoadingFromPageCacheForDocumentLoader:m_provisionalDocumentLoader.get()];

    id identifier = [m_client _dispatchIdentifierForInitialRequest:m_provisionalDocumentLoader->originalRequest()
        fromDocumentLoader:m_provisionalDocumentLoader.get()];
        
    if (!startLoadingMainResource(m_provisionalDocumentLoader->actualRequest(), identifier))
        m_provisionalDocumentLoader->updateLoading();
}

void FrameLoader::stopLoadingPlugIns()
{
    cancelAll(m_plugInStreamLoaders);
}

void FrameLoader::stopLoadingSubresources()
{
    cancelAll(m_subresourceLoaders);
}

void FrameLoader::stopLoading(NSError *error)
{
    m_mainResourceLoader->cancel(error);
}

void FrameLoader::stopLoadingSubframes()
{
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        [Mac(child)->bridge() frameLoader]->stopLoading();
}

void FrameLoader::stopLoading()
{
    // If this method is called from within this method, infinite recursion can occur (3442218). Avoid this.
    if (m_isStoppingLoad)
        return;

    m_isStoppingLoad = true;

    invalidatePendingPolicyDecision(true);

    stopLoadingSubframes();
    if (m_provisionalDocumentLoader)
        m_provisionalDocumentLoader->stopLoading();
    if (m_documentLoader)
        m_documentLoader->stopLoading();
    setProvisionalDocumentLoader(0);
    [m_client _clearArchivedResources];

    m_isStoppingLoad = false;    
}

void FrameLoader::cancelMainResourceLoad()
{
    if (m_mainResourceLoader)
        m_mainResourceLoader->cancel();
}

void FrameLoader::cancelPendingArchiveLoad(WebResourceLoader* loader)
{
    [m_client _cancelPendingArchiveLoadForLoader:loader];
}

DocumentLoader* FrameLoader::activeDocumentLoader() const
{
    if (m_state == WebFrameStateProvisional)
        return m_provisionalDocumentLoader.get();
    return m_documentLoader.get();
}

void FrameLoader::addPlugInStreamLoader(WebResourceLoader* loader)
{
    m_plugInStreamLoaders.add(loader);
    activeDocumentLoader()->setLoading(true);
}

void FrameLoader::removePlugInStreamLoader(WebResourceLoader* loader)
{
    m_plugInStreamLoaders.remove(loader);
    activeDocumentLoader()->updateLoading();
}

void FrameLoader::defersCallbacksChanged()
{
    bool defers = defersCallbacks();
    for (Frame* child = m_frame; child; child = child->tree()->traverseNext(m_frame))
        [Mac(child)->bridge() frameLoader]->setDefersCallbacks(defers);
}

bool FrameLoader::defersCallbacks() const
{
    return [bridge() defersLoading];
}

static void setAllDefersCallbacks(const ResourceLoaderSet& loaders, bool defers)
{
    const ResourceLoaderSet copy = loaders;
    ResourceLoaderSet::const_iterator end = copy.end();
    for (ResourceLoaderSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->setDefersCallbacks(defers);
}

void FrameLoader::setDefersCallbacks(bool defers)
{
    if (m_mainResourceLoader)
        m_mainResourceLoader->setDefersCallbacks(defers);
    setAllDefersCallbacks(m_subresourceLoaders, defers);
    setAllDefersCallbacks(m_plugInStreamLoaders, defers);
    [m_client _setDefersCallbacks:defers];
}

bool FrameLoader::isLoadingMainResource() const
{
    return m_mainResourceLoader;
}

bool FrameLoader::isLoadingSubresources() const
{
    return !m_subresourceLoaders.isEmpty();
}

bool FrameLoader::isLoadingPlugIns() const
{
    return !m_plugInStreamLoaders.isEmpty();
}

bool FrameLoader::isLoading() const
{
    return isLoadingMainResource() || isLoadingSubresources() || isLoadingPlugIns();
}

void FrameLoader::addSubresourceLoader(WebResourceLoader* loader)
{
    ASSERT(!m_provisionalDocumentLoader);
    m_subresourceLoaders.add(loader);
    activeDocumentLoader()->setLoading(true);
}

void FrameLoader::removeSubresourceLoader(WebResourceLoader* loader)
{
    m_subresourceLoaders.remove(loader);
    activeDocumentLoader()->updateLoading();
    checkLoadComplete();
}

NSData *FrameLoader::mainResourceData() const
{
    if (!m_mainResourceLoader)
        return nil;
    return m_mainResourceLoader->resourceData();
}

void FrameLoader::releaseMainResourceLoader()
{
    m_mainResourceLoader = 0;
}

void FrameLoader::setDocumentLoader(DocumentLoader* loader)
{
    if (!loader && !m_documentLoader)
        return;
    
    ASSERT(loader != m_documentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    [m_client _prepareForDataSourceReplacement];
    if (m_documentLoader)
        m_documentLoader->detachFromFrame();

    m_documentLoader = loader;
}

DocumentLoader* FrameLoader::documentLoader() const
{
    return m_documentLoader.get();
}

void FrameLoader::setPolicyDocumentLoader(DocumentLoader* loader)
{
    if (m_policyDocumentLoader == loader)
        return;

    ASSERT(m_frame);
    if (loader)
        loader->setFrame(m_frame);
    if (m_policyDocumentLoader
            && m_policyDocumentLoader != m_provisionalDocumentLoader
            && m_policyDocumentLoader != m_documentLoader)
        m_policyDocumentLoader->detachFromFrame();

    m_policyDocumentLoader = loader;
}
   
DocumentLoader* FrameLoader::provisionalDocumentLoader()
{
    return m_provisionalDocumentLoader.get();
}

void FrameLoader::setProvisionalDocumentLoader(DocumentLoader* loader)
{
    ASSERT(!loader || !m_provisionalDocumentLoader);
    ASSERT(!loader || loader->frameLoader() == this);

    if (m_provisionalDocumentLoader && m_provisionalDocumentLoader != m_documentLoader)
        m_provisionalDocumentLoader->detachFromFrame();

    m_provisionalDocumentLoader = loader;
}

WebFrameState FrameLoader::state() const
{
    return m_state;
}

double FrameLoader::timeOfLastCompletedLoad()
{
    return storedTimeOfLastCompletedLoad;
}

void FrameLoader::provisionalLoadStarted()
{
    m_firstLayoutDone = false;
    [bridge() provisionalLoadStarted];
    [m_client _provisionalLoadStarted];
}

void FrameLoader::setState(WebFrameState newState)
{    
    m_state = newState;
    
    if (newState == WebFrameStateProvisional)
        provisionalLoadStarted();
    else if (newState == WebFrameStateComplete) {
        frameLoadCompleted();
        storedTimeOfLastCompletedLoad = CFAbsoluteTimeGetCurrent();
        m_documentLoader->stopRecordingResponses();
    }
}

void FrameLoader::clearProvisionalLoad()
{
    setProvisionalDocumentLoader(0);
    [m_client _progressCompleted];
    setState(WebFrameStateComplete);
}

void FrameLoader::markLoadComplete()
{
    setState(WebFrameStateComplete);
}

void FrameLoader::commitProvisionalLoad()
{
    stopLoadingSubresources();
    stopLoadingPlugIns();

    setDocumentLoader(m_provisionalDocumentLoader.get());
    setProvisionalDocumentLoader(0);
    setState(WebFrameStateCommittedPage);
}

id FrameLoader::identifierForInitialRequest(NSURLRequest *clientRequest)
{
    return [m_client _dispatchIdentifierForInitialRequest:clientRequest fromDocumentLoader:activeDocumentLoader()];
}

NSURLRequest *FrameLoader::willSendRequest(WebResourceLoader* loader, NSMutableURLRequest *clientRequest, NSURLResponse *redirectResponse)
{
    [clientRequest setValue:[bridge() userAgentForURL:[clientRequest URL]] forHTTPHeaderField:@"User-Agent"];
    return [m_client _dispatchResource:loader->identifier() willSendRequest:clientRequest
        redirectResponse:redirectResponse fromDocumentLoader:activeDocumentLoader()];
}

void FrameLoader::didReceiveAuthenticationChallenge(WebResourceLoader* loader, NSURLAuthenticationChallenge *currentWebChallenge)
{
    [m_client _dispatchDidReceiveAuthenticationChallenge:currentWebChallenge
        forResource:loader->identifier() fromDocumentLoader:activeDocumentLoader()];
}

void FrameLoader::didCancelAuthenticationChallenge(WebResourceLoader* loader, NSURLAuthenticationChallenge *currentWebChallenge)
{
    [m_client _dispatchDidCancelAuthenticationChallenge:currentWebChallenge
        forResource:loader->identifier() fromDocumentLoader:activeDocumentLoader()];
}

void FrameLoader::didReceiveResponse(WebResourceLoader* loader, NSURLResponse *r)
{
    activeDocumentLoader()->addResponse(r);
    
    [m_client _incrementProgressForIdentifier:loader->identifier() response:r];
    [m_client _dispatchResource:loader->identifier() didReceiveResponse:r fromDocumentLoader:activeDocumentLoader()];
}

void FrameLoader::didReceiveData(WebResourceLoader* loader, NSData *data, int lengthReceived)
{
    [m_client _incrementProgressForIdentifier:loader->identifier() data:data];
    [m_client _dispatchResource:loader->identifier() didReceiveContentLength:lengthReceived fromDocumentLoader:activeDocumentLoader()];
}

void FrameLoader::didFinishLoad(WebResourceLoader* loader)
{    
    [m_client _completeProgressForIdentifier:loader->identifier()];
    [m_client _dispatchResource:loader->identifier() didFinishLoadingFromDocumentLoader:activeDocumentLoader()];
}

void FrameLoader::didFailToLoad(WebResourceLoader* loader, NSError *error)
{
    [m_client _completeProgressForIdentifier:loader->identifier()];
    if (error)
        [m_client _dispatchResource:loader->identifier() didFailLoadingWithError:error fromDocumentLoader:activeDocumentLoader()];
}

bool FrameLoader::privateBrowsingEnabled() const
{
    return [m_client _privateBrowsingEnabled];
}

NSURLRequest *FrameLoader::originalRequest() const
{
    return activeDocumentLoader()->originalRequestCopy();
}

void FrameLoader::receivedMainResourceError(NSError *error, bool isComplete)
{
    RefPtr<DocumentLoader> loader = activeDocumentLoader();
    
    WebCoreFrameBridge *bridge = FrameLoader::bridge();
    
    // Retain the bridge because the stop may release the last reference to it.
    [bridge retain];
 
    id<WebFrameLoaderClient> cli = [m_client retain];
   
    if (isComplete) {
        // FIXME: Don't want to do this if an entirely new load is going, so should check
        // that both data sources on the frame are either this or nil.
        // Can't call _bridge because we might not have commited yet
        [bridge stop];
        if ([cli _shouldFallBackForError:error])
            [bridge handleFallbackContent];
    }
    
    if (m_state == WebFrameStateProvisional) {
        NSURL *failedURL = [m_provisionalDocumentLoader->originalRequestCopy() URL];
        [bridge didNotOpenURL:failedURL];
        [m_client _invalidateCurrentItemPageCache];
        
        // Call clientRedirectCancelledOrFinished here so that the frame load delegate is notified that the redirect's
        // status has changed, if there was a redirect. The frame load delegate may have saved some state about
        // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:. Since we are definitely
        // not going to use this provisional resource, as it was cancelled, notify the frame load delegate that the redirect
        // has ended.
        if (m_sentRedirectNotification)
            clientRedirectCancelledOrFinished(false);
    }
    
    
    loader->mainReceivedError(error, isComplete);

    [bridge release];
    [cli release];
}

void FrameLoader::clientRedirectCancelledOrFinished(bool cancelWithLoadInProgress)
{
    // Note that -webView:didCancelClientRedirectForFrame: is called on the frame load delegate even if
    // the redirect succeeded.  We should either rename this API, or add a new method, like
    // -webView:didFinishClientRedirectForFrame:
    [m_client _dispatchDidCancelClientRedirectForFrame];

    if (!cancelWithLoadInProgress)
        m_quickRedirectComing = false;

    m_sentRedirectNotification = false;
}

void FrameLoader::clientRedirected(NSURL *URL, double seconds, NSDate *date, bool lockHistory, bool isJavaScriptFormAction)
{
    [m_client _dispatchWillPerformClientRedirectToURL:URL delay:seconds fireDate:date];
    
    // Remember that we sent a redirect notification to the frame load delegate so that when we commit
    // the next provisional load, we can send a corresponding -webView:didCancelClientRedirectForFrame:
    m_sentRedirectNotification = true;
    
    // If a "quick" redirect comes in an, we set a special mode so we treat the next
    // load as part of the same navigation. If we don't have a document loader, we have
    // no "original" load on which to base a redirect, so we treat the redirect as a normal load.
    m_quickRedirectComing = lockHistory && m_documentLoader && !isJavaScriptFormAction;
}

bool FrameLoader::shouldReload(NSURL *currentURL, NSURL *destinationURL)
{
    return !(([currentURL fragment] || [destinationURL fragment])
        && [urlByRemovingFragment(currentURL) isEqual:urlByRemovingFragment(destinationURL)]);
}

void FrameLoader::continueFragmentScrollAfterNavigationPolicy(NSURLRequest *request)
{
    if (!request)
        return;
    
    NSURL *URL = [request URL];
    
    bool isRedirect = m_quickRedirectComing;
    m_quickRedirectComing = false;
    
    m_documentLoader->replaceRequestURLForAnchorScroll(URL);
    if (!isRedirect && ![m_client _shouldTreatURLAsSameAsCurrent:URL]) {
        // NB: must happen after _setURL, since we add based on the current request.
        // Must also happen before we openURL and displace the scroll position, since
        // adding the BF item will save away scroll state.
        
        // NB2:  If we were loading a long, slow doc, and the user anchor nav'ed before
        // it was done, currItem is now set the that slow doc, and prevItem is whatever was
        // before it.  Adding the b/f item will bump the slow doc down to prevItem, even
        // though its load is not yet done.  I think this all works out OK, for one because
        // we have already saved away the scroll and doc state for the long slow load,
        // but it's not an obvious case.

        [m_client _addHistoryItemForFragmentScroll];
    }
    
    [bridge() scrollToAnchorWithURL:URL];
    
    if (!isRedirect)
        // This will clear previousItem from the rest of the frame tree that didn't
        // doing any loading. We need to make a pass on this now, since for anchor nav
        // we'll not go through a real load and reach Completed state.
        checkLoadComplete();
 
    [m_client _dispatchDidChangeLocationWithinPageForFrame];
    [m_client _didFinishLoad];
}

void FrameLoader::closeOldDataSources()
{
    // FIXME: Is it important for this traversal to be postorder instead of preorder?
    // If so, add helpers for postorder traversal, and use them. If not, then lets not
    // use a recursive algorithm here.
    for (Frame* child = m_frame->tree()->firstChild(); child; child = child->tree()->nextSibling())
        [Mac(child)->bridge() frameLoader]->closeOldDataSources();
    
    if (m_documentLoader)
        [m_client _dispatchWillCloseFrame];

    [m_client _setMainFrameDocumentReady:NO]; // stop giving out the actual DOMDocument to observers
}

void FrameLoader::opened()
{
    if (m_loadType == FrameLoadTypeStandard && m_documentLoader->isClientRedirect())
        [m_client _updateHistoryAfterClientRedirect];

    if ([m_client _isDocumentLoaderLoadingFromPageCache:m_documentLoader.get()]) {
        // Force a layout to update view size and thereby update scrollbars.
        [m_client _forceLayout];

        const ResponseVector& responses = m_documentLoader->responses();
        size_t count = responses.size();
        for (size_t i = 0; i < count; i++) {
            NSURLResponse *response = responses[i].get();
            // FIXME: If the WebKit client changes or cancels the request, this is not respected.
            NSError *error;
            id identifier;
            NSURLRequest *request = [[NSURLRequest alloc] initWithURL:[response URL]];
            requestFromDelegate(request, identifier, error);
            sendRemainingDelegateMessages(identifier, response, (unsigned)[response expectedContentLength], error);
            [request release];
        }
        
        [m_client _loadedFromPageCache];

        m_documentLoader->setPrimaryLoadComplete(true);

        // FIXME: Why only this frame and not parent frames?
        checkLoadCompleteForThisFrame();
    }
}

void FrameLoader::commitProvisionalLoad(NSDictionary *pageCache)
{
    bool reload = m_loadType == FrameLoadTypeReload || m_loadType == FrameLoadTypeReloadAllowingStaleData;
    
    RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;
    
    NSURLResponse *response = pdl->response();
    
    NSDictionary *headers = [response isKindOfClass:[NSHTTPURLResponse class]]
        ? [(NSHTTPURLResponse *)response allHeaderFields] : nil;
    
    if (m_loadType != FrameLoadTypeReplace)
        closeOldDataSources();
    
    if (!pageCache)
        [m_client _makeRepresentationForDocumentLoader:pdl.get()];
    
    transitionToCommitted(pageCache);
    
    // Call -_clientRedirectCancelledOrFinished: here so that the frame load delegate is notified that the redirect's
    // status has changed, if there was a redirect.  The frame load delegate may have saved some state about
    // the redirect in its -webView:willPerformClientRedirectToURL:delay:fireDate:forFrame:.  Since we are
    // just about to commit a new page, there cannot possibly be a pending redirect at this point.
    if (m_sentRedirectNotification)
        clientRedirectCancelledOrFinished(false);
    
    NSURL *baseURL = [pdl->request() _webDataRequestBaseURL];        
    NSURL *URL = baseURL ? baseURL : [response URL];
    
    if (!URL || urlIsEmpty(URL))
        URL = [NSURL URLWithString:@"about:blank"];    
    
    [bridge() openURL:URL
               reload:reload 
          contentType:[response MIMEType]
              refresh:[headers objectForKey:@"Refresh"]
         lastModified:(pageCache ? nil : wkGetNSURLResponseLastModifiedDate(response))
            pageCache:pageCache];
    
    opened();
}

NSURLRequest *FrameLoader::initialRequest() const
{
    return activeDocumentLoader()->initialRequest();
}

void FrameLoader::receivedData(NSData *data)
{
    activeDocumentLoader()->receivedData(data);
}

void FrameLoader::setRequest(NSURLRequest *request)
{
    activeDocumentLoader()->setRequest(request);
}

void FrameLoader::download(NSURLConnection *connection, NSURLRequest *request, NSURLResponse *response, id proxy)
{
    [m_client _downloadWithLoadingConnection:connection request:request response:response proxy:proxy];
}

WebCoreFrameBridge *FrameLoader::bridge() const
{
    return Mac(m_frame)->bridge();
}

void FrameLoader::handleFallbackContent()
{
    [bridge() handleFallbackContent];
}

bool FrameLoader::isStopping() const
{
    return activeDocumentLoader()->isStopping();
}

void FrameLoader::setResponse(NSURLResponse *response)
{
    activeDocumentLoader()->setResponse(response);
}

void FrameLoader::mainReceivedError(NSError *error, bool isComplete)
{
    activeDocumentLoader()->mainReceivedError(error, isComplete);
}

void FrameLoader::finishedLoading()
{
    RefPtr<DocumentLoader> dl = activeDocumentLoader();
    
    WebCoreFrameBridge *bridge = FrameLoader::bridge();

    [bridge retain];
    dl->finishedLoading();

    if (dl->mainDocumentError() || !dl->frameLoader()) {
        [bridge release];
        return;
    }

    dl->setPrimaryLoadComplete(true);
    [m_client _dispatchDidLoadMainResourceForDocumentLoader:dl.get()];
    checkLoadComplete();

    [bridge release];
}

void FrameLoader::notifyIconChanged(NSURL *iconURL)
{
    ASSERT([[WebCoreIconDatabaseBridge sharedInstance] _isEnabled]);
    NSImage *icon = [[WebCoreIconDatabaseBridge sharedInstance]
        iconForPageURL:urlOriginalDataAsString(activeDocumentLoader()->URL())
        withSize:NSMakeSize(16, 16)];
    [m_client _dispatchDidReceiveIcon:icon];
}

NSURL *FrameLoader::URL() const
{
    return activeDocumentLoader()->URL();
}

NSError *FrameLoader::cancelledError(NSURLRequest *request) const
{
    return [m_client _cancelledErrorWithRequest:request];
}

NSError *FrameLoader::fileDoesNotExistError(NSURLResponse *response) const
{
    return [m_client _fileDoesNotExistErrorWithResponse:response];    
}

bool FrameLoader::willUseArchive(WebResourceLoader* loader, NSURLRequest *request, NSURL *originalURL) const
{
    return [m_client _willUseArchiveForRequest:request originalURL:originalURL loader:loader];
}

bool FrameLoader::isArchiveLoadPending(WebResourceLoader* loader) const
{
    return [m_client _archiveLoadPendingForLoader:loader];
}

void FrameLoader::handleUnimplementablePolicy(NSError *error)
{
    m_delegateIsHandlingUnimplementablePolicy = YES;
    [m_client _dispatchUnableToImplementPolicyWithError:error];
    m_delegateIsHandlingUnimplementablePolicy = NO;
}

void FrameLoader::cannotShowMIMEType(NSURLResponse *response)
{
    handleUnimplementablePolicy([m_client _cannotShowMIMETypeErrorWithResponse:response]);
}

NSError *FrameLoader::interruptionForPolicyChangeError(NSURLRequest *request)
{
    return [m_client _interruptForPolicyChangeErrorWithRequest:request];
}

bool FrameLoader::isHostedByObjectElement() const
{
    Element* owner = m_frame->ownerElement();
    return owner && owner->hasTagName(objectTag);
}

bool FrameLoader::isLoadingMainFrame() const
{
    return [bridge() isMainFrame];
}

bool FrameLoader::canShowMIMEType(NSString *MIMEType) const
{
    return [m_client _canShowMIMEType:MIMEType];
}

bool FrameLoader::representationExistsForURLScheme(NSString *URLScheme)
{
    return [m_client _representationExistsForURLScheme:URLScheme];
}

NSString *FrameLoader::generatedMIMETypeForURLScheme(NSString *URLScheme)
{
    return [m_client _generatedMIMETypeForURLScheme:URLScheme];
}

void FrameLoader::checkNavigationPolicy(NSURLRequest *newRequest, id obj, SEL sel)
{
    checkNavigationPolicy(newRequest, activeDocumentLoader(), 0, obj, sel);
}

void FrameLoader::checkContentPolicy(NSString *MIMEType, id obj, SEL sel)
{
    WebPolicyDecider *d = [m_client _createPolicyDeciderWithTarget:obj action:sel];
    m_policyDecider = d;
    [m_client _dispatchDecidePolicyForMIMEType:MIMEType request:activeDocumentLoader()->request() decider:d];
    [d release];
}

void FrameLoader::cancelContentPolicyCheck()
{
    [m_policyDecider.get() invalidate];
    m_policyDecider = nil;
}

bool FrameLoader::shouldReloadToHandleUnreachableURL(NSURLRequest *request)
{
    NSURL *unreachableURL = [request _webDataRequestUnreachableURL];
    if (unreachableURL == nil)
        return false;

    if (!isBackForwardLoadType(m_policyLoadType))
        return false;

    // We only treat unreachableURLs specially during the delegate callbacks
    // for provisional load errors and navigation policy decisions. The former
    // case handles well-formed URLs that can't be loaded, and the latter
    // case handles malformed URLs and unknown schemes. Loading alternate content
    // at other times behaves like a standard load.
    DocumentLoader* compareDocumentLoader = 0;
    if (m_delegateIsDecidingNavigationPolicy || m_delegateIsHandlingUnimplementablePolicy)
        compareDocumentLoader = m_policyDocumentLoader.get();
    else if (m_delegateIsHandlingProvisionalLoadError)
        compareDocumentLoader = m_provisionalDocumentLoader.get();

    return compareDocumentLoader && [unreachableURL isEqual:[compareDocumentLoader->request() URL]];
}

void FrameLoader::reloadAllowingStaleData(NSString *encoding)
{
    if (!m_documentLoader)
        return;

    NSMutableURLRequest *request = [m_documentLoader->request() mutableCopy];
    NSURL *unreachableURL = m_documentLoader->unreachableURL();
    if (unreachableURL)
        [request setURL:unreachableURL];

    [request setCachePolicy:NSURLRequestReturnCacheDataElseLoad];

    RefPtr<DocumentLoader> loader = [m_client _createDocumentLoaderWithRequest:request];
    setPolicyDocumentLoader(loader.get());

    [request release];

    loader->setOverrideEncoding(encoding);

    load(loader.get(), FrameLoadTypeReloadAllowingStaleData, 0);
}

void FrameLoader::reload()
{
    if (!m_documentLoader)
        return;

    NSMutableURLRequest *initialRequest = m_documentLoader->request();
    
    // If a window is created by javascript, its main frame can have an empty but non-nil URL.
    // Reloading in this case will lose the current contents (see 4151001).
    if ([[[initialRequest URL] absoluteString] length] == 0)
        return;

    // Replace error-page URL with the URL we were trying to reach.
    NSURL *unreachableURL = [initialRequest _webDataRequestUnreachableURL];
    if (unreachableURL != nil)
        initialRequest = [NSURLRequest requestWithURL:unreachableURL];
    
    RefPtr<DocumentLoader> loader = [m_client _createDocumentLoaderWithRequest:initialRequest];
    setPolicyDocumentLoader(loader.get());

    NSMutableURLRequest *request = loader->request();

    [request setCachePolicy:NSURLRequestReloadIgnoringCacheData];

    // If we're about to rePOST, set up action so the app can warn the user
    if (isCaseInsensitiveEqual([request HTTPMethod], @"POST"))
        loader->setTriggeringAction(actionInformation(NavigationTypeFormResubmitted, nil, [request URL]));

    loader->setOverrideEncoding(m_documentLoader->overrideEncoding());
    
    load(loader.get(), FrameLoadTypeReload, 0);
}

void FrameLoader::didReceiveServerRedirectForProvisionalLoadForFrame()
{
    [m_client _dispatchDidReceiveServerRedirectForProvisionalLoadForFrame];
}

void FrameLoader::finishedLoadingDocument(DocumentLoader* loader)
{
    [m_client _finishedLoadingDocument:loader];
}

void FrameLoader::committedLoad(DocumentLoader* loader, NSData *data)
{
    [m_client _committedLoadWithDocumentLoader:loader data:data];
}

bool FrameLoader::isReplacing() const
{
    return m_loadType == FrameLoadTypeReplace;
}

void FrameLoader::setReplacing()
{
    m_loadType = FrameLoadTypeReplace;
}

void FrameLoader::revertToProvisional(DocumentLoader* loader)
{
    [m_client _revertToProvisionalStateForDocumentLoader:loader];
}

void FrameLoader::setMainDocumentError(DocumentLoader* loader, NSError *error)
{
    [m_client _setMainDocumentError:error forDocumentLoader:loader];
}

void FrameLoader::mainReceivedCompleteError(DocumentLoader* loader, NSError *error)
{
    loader->setPrimaryLoadComplete(true);
    [m_client _dispatchDidLoadMainResourceForDocumentLoader:activeDocumentLoader()];
    checkLoadComplete();
}

bool FrameLoader::subframeIsLoading() const
{
    // It's most likely that the last added frame is the last to load so we walk backwards.
    for (Frame* child = m_frame->tree()->lastChild(); child; child = child->tree()->previousSibling()) {
        FrameLoader* childLoader = [Mac(child)->bridge() frameLoader];
        DocumentLoader* documentLoader = childLoader->documentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
        documentLoader = childLoader->provisionalDocumentLoader();
        if (documentLoader && documentLoader->isLoadingInAPISense())
            return true;
    }
    return false;
}

void FrameLoader::willChangeTitle(DocumentLoader* loader)
{
    [m_client _willChangeTitleForDocument:loader];
}

void FrameLoader::didChangeTitle(DocumentLoader* loader)
{
    [m_client _didChangeTitleForDocument:loader];

    // The title doesn't get communicated to the WebView until we are committed.
    if (loader->isCommitted())
        if (NSURL *URLForHistory = canonicalURL(loader->URLForHistory())) {
            // Must update the entries in the back-forward list too.
            // This must go through the WebFrame because it has the right notion of the current b/f item.
            [m_client _setTitle:loader->title() forURL:URLForHistory];
            [m_client _setMainFrameDocumentReady:YES]; // update observers with new DOMDocument
            [m_client _dispatchDidReceiveTitle:loader->title()];
        }
}

FrameLoadType FrameLoader::loadType() const
{
    return m_loadType;
}

void FrameLoader::invalidatePendingPolicyDecision(bool callDefaultAction)
{
    [m_policyDecider.get() invalidate];
    m_policyDecider = nil;

    bool hadFrameName = m_policyFrameName;
    RetainPtr<id> target = m_policyTarget;
    SEL selector = m_policySelector;

    m_policyRequest = nil;
    m_policyFrameName = nil;
    m_policyTarget = nil;
    m_policyFormState = 0;

    if (callDefaultAction) {
        if (hadFrameName)
            objc_msgSend(target.get(), selector, nil, nil, nil);
        else
            objc_msgSend(target.get(), selector, nil, nil);
    }
}

void FrameLoader::checkNewWindowPolicy(NSURLRequest *request, NSDictionary *action,
    NSString *frameName, PassRefPtr<FormState> formState)
{
    WebPolicyDecider *decider = [m_client _createPolicyDeciderWithTarget:asDelegate()
        action:@selector(continueAfterNewWindowPolicy:)];

    m_policyRequest = request;
    m_policyTarget = asDelegate();
    m_policyFrameName = frameName;
    m_policySelector = @selector(continueLoadRequestAfterNewWindowPolicy:frameName:formState:);
    m_policyDecider = decider;
    m_policyFormState = formState;

    [m_client _dispatchDecidePolicyForNewWindowAction:action request:request
        newFrameName:frameName decider:decider];

    [decider release];
}

void FrameLoader::continueAfterNewWindowPolicy(WebPolicyAction policy)
{
    RetainPtr<NSURLRequest> request = m_policyRequest;
    RetainPtr<NSString> frameName = m_policyFrameName;
    RetainPtr<id> target = m_policyTarget;
    SEL selector = m_policySelector;
    RefPtr<FormState> formState = m_policyFormState;

    invalidatePendingPolicyDecision(false);

    switch (policy) {
        case WebPolicyIgnore:
            request = nil;
            break;
        case WebPolicyDownload:
            [m_client _startDownloadWithRequest:request.get()];
            request = nil;
            break;
        case WebPolicyUse:
            break;
    }

    objc_msgSend(target.get(), selector, request.get(), frameName.get(), formState.get());
}

void FrameLoader::checkNavigationPolicy(NSURLRequest *request, DocumentLoader* loader,
    PassRefPtr<FormState> formState, id target, SEL selector)
{
    NSDictionary *action = loader->triggeringAction();
    if (!action) {
        action = actionInformation(NavigationTypeOther, nil, [request URL]);
        loader->setTriggeringAction(action);
    }
        
    // Don't ask more than once for the same request or if we are loading an empty URL.
    // This avoids confusion on the part of the client.
    if ([request isEqual:loader->lastCheckedRequest()] || urlIsEmpty([request URL])) {
        objc_msgSend(target, selector, request, nil);
        return;
    }
    
    // We are always willing to show alternate content for unreachable URLs;
    // treat it like a reload so it maintains the right state for b/f list.
    if ([request _webDataRequestUnreachableURL] != nil) {
        if (isBackForwardLoadType(m_policyLoadType))
            m_policyLoadType = FrameLoadTypeReload;
        objc_msgSend(target, selector, request, nil);
        return;
    }
    
    loader->setLastCheckedRequest(request);

    WebPolicyDecider *decider = [m_client _createPolicyDeciderWithTarget:asDelegate()
        action:@selector(continueAfterNavigationPolicy:)];
    
    m_policyRequest = request;
    m_policyTarget = target;
    m_policySelector = selector;
    m_policyDecider = decider;
    m_policyFormState = formState;

    m_delegateIsDecidingNavigationPolicy = true;
    [m_client _dispatchDecidePolicyForNavigationAction:action request:request decider:decider];
    m_delegateIsDecidingNavigationPolicy = false;
    
    [decider release];
}

void FrameLoader::continueAfterNavigationPolicy(WebPolicyAction policy)
{
    RetainPtr<NSURLRequest> request = m_policyRequest;
    RetainPtr<id> target = m_policyTarget;
    SEL selector = m_policySelector;
    RefPtr<FormState> formState = m_policyFormState.release();
    
    invalidatePendingPolicyDecision(false);

    switch (policy) {
        case WebPolicyIgnore:
            request = nil;
            break;
        case WebPolicyDownload:
            [m_client _startDownloadWithRequest:request.get()];
            request = nil;
            break;
        case WebPolicyUse:
            if (![m_client _canHandleRequest:request.get()]) {
                handleUnimplementablePolicy([m_client _cannotShowURLErrorWithRequest:request.get()]);
                request = nil;
            }
            break;
    }

    objc_msgSend(target.get(), selector, request.get(), formState.get());
}

void FrameLoader::continueAfterWillSubmitForm(WebPolicyAction policy)
{
    if (m_policyDecider) {
        [m_policyDecider.get() invalidate];
        m_policyDecider = nil;
    }
    startLoading();
}

void FrameLoader::continueLoadRequestAfterNavigationPolicy(NSURLRequest *request, FormState* formState)
{
    // If we loaded an alternate page to replace an unreachableURL, we'll get in here with a
    // nil policyDataSource because loading the alternate page will have passed
    // through this method already, nested; otherwise, policyDataSource should still be set.
    ASSERT(m_policyDocumentLoader || m_provisionalDocumentLoader->unreachableURL());

    BOOL isTargetItem = [m_client _provisionalItemIsTarget];

    // Two reasons we can't continue:
    //    1) Navigation policy delegate said we can't so request is nil. A primary case of this 
    //       is the user responding Cancel to the form repost nag sheet.
    //    2) User responded Cancel to an alert popped up by the before unload event handler.
    // The "before unload" event handler runs only for the main frame.
    bool canContinue = request && (!isLoadingMainFrame() || [bridge() shouldClose]);

    if (!canContinue) {
        // If we were waiting for a quick redirect, but the policy delegate decided to ignore it, then we 
        // need to report that the client redirect was cancelled.
        if (m_quickRedirectComing)
            clientRedirectCancelledOrFinished(false);

        setPolicyDocumentLoader(0);

        // If the navigation request came from the back/forward menu, and we punt on it, we have the 
        // problem that we have optimistically moved the b/f cursor already, so move it back.  For sanity, 
        // we only do this when punting a navigation for the target frame or top-level frame.  
        if ((isTargetItem || isLoadingMainFrame()) && isBackForwardLoadType(m_policyLoadType))
            [m_client _resetBackForwardList];

        return;
    }

    FrameLoadType type = m_policyLoadType;
    stopLoading();
    setProvisionalDocumentLoader(m_policyDocumentLoader.get());
    m_loadType = type;
    setState(WebFrameStateProvisional);

    setPolicyDocumentLoader(0);

    if (isBackForwardLoadType(type) && [m_client _loadProvisionalItemFromPageCache])
        return;

    if (formState) {
        // It's a bit of a hack to reuse the WebPolicyDecisionListener for the continuation
        // mechanism across the willSubmitForm callout.
        WebPolicyDecider *decider = [m_client _createPolicyDeciderWithTarget:asDelegate() action:@selector(continueAfterWillSubmitForm:)];
        m_policyDecider = decider;
        [m_client _dispatchSourceFrame:Mac(formState->sourceFrame())->bridge()
            willSubmitForm:[DOMElement _elementWith:formState->form()]
            withValues:formState->valuesAsNSDictionary() submissionDecider:decider];
        [decider release];
    } else
        continueAfterWillSubmitForm(WebPolicyUse);
}

void FrameLoader::didFirstLayout()
{
    if (isBackForwardLoadType(m_loadType) && [m_client _hasBackForwardList])
        [m_client _restoreScrollPositionAndViewState];

    m_firstLayoutDone = true;
    [m_client _dispatchDidFirstLayoutInFrame];
}

void FrameLoader::frameLoadCompleted()
{
    [m_client _frameLoadCompleted];

    // After a canceled provisional load, firstLayoutDone is false.
    // Reset it to YES if we're displaying a page.
    if (m_documentLoader)
        m_firstLayoutDone = true;
}

bool FrameLoader::firstLayoutDone() const
{
    return m_firstLayoutDone;
}

bool FrameLoader::isQuickRedirectComing() const
{
    return m_quickRedirectComing;
}

void FrameLoader::transitionToCommitted(NSDictionary *pageCache)
{
    ASSERT([m_client _hasWebView]);
    ASSERT(m_state == WebFrameStateProvisional);

    if (m_state != WebFrameStateProvisional)
        return;

    [m_client _setCopiesOnScroll];
    [m_client _updateHistoryForCommit];

    // The call to closeURL invokes the unload event handler, which can execute arbitrary
    // JavaScript. If the script initiates a new load, we need to abandon the current load,
    // or the two will stomp each other.
    DocumentLoader* pdl = m_provisionalDocumentLoader.get();
    [bridge() closeURL];
    if (pdl != m_provisionalDocumentLoader)
        return;

    commitProvisionalLoad();

    // Handle adding the URL to the back/forward list.
    DocumentLoader* dl = m_documentLoader.get();
    NSString *ptitle = dl->title();

    switch (m_loadType) {
    case FrameLoadTypeForward:
    case FrameLoadTypeBack:
    case FrameLoadTypeIndexedBackForward:
        if ([m_client _hasBackForwardList]) {
            [m_client _updateHistoryForBackForwardNavigation];

            // Create a document view for this document, or used the cached view.
            if (pageCache)
                [m_client _setDocumentViewFromPageCache:pageCache];
            else
                [m_client _makeDocumentView];
        }
        break;

    case FrameLoadTypeReload:
    case FrameLoadTypeSame:
    case FrameLoadTypeReplace:
        [m_client _updateHistoryForReload];
        [m_client _makeDocumentView];
        break;

    // FIXME - just get rid of this case, and merge FrameLoadTypeReloadAllowingStaleData with the above case
    case FrameLoadTypeReloadAllowingStaleData:
        [m_client _makeDocumentView];
        break;

    case FrameLoadTypeStandard:
        [m_client _updateHistoryForStandardLoad];
        [m_client _makeDocumentView];
        break;

    case FrameLoadTypeInternal:
        [m_client _updateHistoryForInternalLoad];
        [m_client _makeDocumentView];
        break;

    // FIXME Remove this check when dummy ds is removed (whatever "dummy ds" is).
    // An exception should be thrown if we're in the FrameLoadTypeUninitialized state.
    default:
        ASSERT_NOT_REACHED();
    }

    // Tell the client we've committed this URL.
    ASSERT([m_client _hasFrameView]);
    [m_client _dispatchDidCommitLoadForFrame];
    
    // If we have a title let the WebView know about it.
    if (ptitle)
        [m_client _dispatchDidReceiveTitle:ptitle];
}

void FrameLoader::checkLoadCompleteForThisFrame()
{
    ASSERT([m_client _hasWebView]);

    switch (m_state) {
        case WebFrameStateProvisional: {
            if (m_delegateIsHandlingProvisionalLoadError)
                return;

            RefPtr<DocumentLoader> pdl = m_provisionalDocumentLoader;

            // If we've received any errors we may be stuck in the provisional state and actually complete.
            NSError *error = pdl->mainDocumentError();
            if (!error)
                return;

            // Check all children first.
            LoadErrorResetToken *resetToken = [m_client _tokenForLoadErrorReset];
            bool shouldReset = true;
            if (!pdl->isLoadingInAPISense()) {
                m_delegateIsHandlingProvisionalLoadError = true;
                [m_client _dispatchDidFailProvisionalLoadWithError:error];
                m_delegateIsHandlingProvisionalLoadError = false;

                // FIXME: can stopping loading here possibly have any effect, if isLoading is false,
                // which it must be to be in this branch of the if? And is it OK to just do a full-on
                // stopLoading instead of stopLoadingSubframes?
                stopLoadingSubframes();
                pdl->stopLoading();

                // Finish resetting the load state, but only if another load hasn't been started by the
                // delegate callback.
                if (pdl == m_provisionalDocumentLoader)
                    clearProvisionalLoad();
                else {
                    NSURL *unreachableURL = m_provisionalDocumentLoader->unreachableURL();
                    if (unreachableURL && [unreachableURL isEqual:[pdl->request() URL]])
                        shouldReset = false;
                }
            }
            if (shouldReset)
                [m_client _resetAfterLoadError:resetToken];
            else
                [m_client _doNotResetAfterLoadError:resetToken];
            return;
        }
        
        case WebFrameStateCommittedPage: {
            DocumentLoader* dl = m_documentLoader.get();            
            if (dl->isLoadingInAPISense())
                return;

            markLoadComplete();

            // FIXME: Is this subsequent work important if we already navigated away?
            // Maybe there are bugs because of that, or extra work we can skip because
            // the new page is ready.

            [m_client _forceLayoutForNonHTML];
             
            // If the user had a scroll point, scroll to it, overriding the anchor point if any.
            if ((isBackForwardLoadType(m_loadType) || m_loadType == FrameLoadTypeReload)
                    && [m_client _hasBackForwardList])
                [m_client _restoreScrollPositionAndViewState];

            NSError *error = dl->mainDocumentError();
            if (error)
                [m_client _dispatchDidFailLoadWithError:error];
            else
                [m_client _dispatchDidFinishLoadForFrame];

            [m_client _progressCompleted];
            return;
        }
        
        case WebFrameStateComplete:
            // Even if already complete, we might have set a previous item on a frame that
            // didn't do any data loading on the past transaction. Make sure to clear these out.
            [m_client _frameLoadCompleted];
            return;
    }

    ASSERT_NOT_REACHED();
}

void FrameLoader::continueLoadRequestAfterNewWindowPolicy(NSURLRequest *request, NSString *frameName, FormState* formState)
{
    if (!request)
        return;

    RefPtr<Frame> frame = m_frame;

    WebCoreFrameBridge *mainBridge = [m_client _dispatchCreateWebViewWithRequest:nil];
    if (!mainBridge)
        return;

    [mainBridge retain];

    [mainBridge setName:frameName];
    [[mainBridge frameLoader]->client() _dispatchShow];
    [mainBridge impl]->setOpener(frame.get());
    [mainBridge frameLoader]->load(request, nil, FrameLoadTypeStandard, formState);

    [mainBridge release];
}

void FrameLoader::sendRemainingDelegateMessages(id identifier, NSURLResponse *response, unsigned length, NSError *error)
{    
    if (response)
        [m_client _dispatchResource:identifier didReceiveResponse:response fromDocumentLoader:m_documentLoader.get()];
    
    if (length > 0)
        [m_client _dispatchResource:identifier didReceiveContentLength:length fromDocumentLoader:m_documentLoader.get()];
    
    if (!error)
        [m_client _dispatchResource:identifier didFinishLoadingFromDocumentLoader:m_documentLoader.get()];
    else
        [m_client _dispatchResource:identifier didFailLoadingWithError:error fromDocumentLoader:m_documentLoader.get()];
}

NSURLRequest *FrameLoader::requestFromDelegate(NSURLRequest *request, id& identifier, NSError *& error)
{
    ASSERT(request != nil);

    identifier = [m_client _dispatchIdentifierForInitialRequest:request fromDocumentLoader:m_documentLoader.get()]; 
    NSURLRequest *newRequest = [m_client _dispatchResource:identifier
        willSendRequest:request redirectResponse:nil fromDocumentLoader:m_documentLoader.get()];

    if (newRequest == nil)
        error = [m_client _cancelledErrorWithRequest:request];
    else
        error = nil;

    return newRequest;
}

void FrameLoader::post(NSURL *URL, NSString *referrer, NSString *target, NSArray *postData,
    NSString *contentType, NSEvent *event, DOMElement *form, NSDictionary *formValues)
{
    // When posting, use the NSURLRequestReloadIgnoringCacheData load flag.
    // This prevents a potential bug which may cause a page with a form that uses itself
    // as an action to be returned from the cache without submitting.

    // FIXME: Where's the code that implements what the comment above says?

    NSMutableURLRequest *request = [[NSMutableURLRequest alloc] initWithURL:URL];
    addExtraFieldsToRequest(request, true, true);

    setHTTPReferrer(request, referrer);
    [request setHTTPMethod:@"POST"];
    webSetHTTPBody(request, postData);
    [request setValue:contentType forHTTPHeaderField:@"Content-Type"];

    NSDictionary *action = actionInformation(FrameLoadTypeStandard, true, event, URL);
    RefPtr<FormState> formState;
    if (form && formValues)
        formState = FormState::create(form, formValues, bridge());

    if (target != nil) {
        Frame* targetFrame = m_frame->tree()->find(target);
        if (targetFrame)
            [Mac(targetFrame)->bridge() frameLoader]->load(request, action, FrameLoadTypeStandard, formState.release());
        else
            checkNewWindowPolicy(request, action, target, formState.release());
    } else
        load(request, action, FrameLoadTypeStandard, formState.release());

    [request release];
}

void FrameLoader::detachChildren()
{
    // FIXME: Is it really necessary to do this in reverse order?
    Frame* previous;
    for (Frame* child = m_frame->tree()->lastChild(); child; child = previous) {
        previous = child->tree()->previousSibling();
        [Mac(child)->bridge() frameLoader]->detachFromParent();
    }
}

void FrameLoader::detachFromParent()
{
    WebCoreFrameBridge *bridge = [FrameLoader::bridge() retain];
    id <WebFrameLoaderClient> client = [m_client retain];

    [bridge closeURL];
    stopLoading();
    [client _detachedFromParent1];
    detachChildren();
    [client _detachedFromParent2];
    setDocumentLoader(0);
    [client _detachedFromParent3];
    if (Frame* parent = m_frame->tree()->parent())
        parent->tree()->removeChild(m_frame);
    [bridge close];
    [client _detachedFromParent4];

    [client release];
    [bridge release];
}

void FrameLoader::addExtraFieldsToRequest(NSMutableURLRequest *request, bool mainResource, bool alwaysFromRequest)
{
    [request setValue:[bridge() userAgentForURL:[request URL]] forHTTPHeaderField:@"User-Agent"];
    
    if (m_loadType == FrameLoadTypeReload)
        [request setValue:@"max-age=0" forHTTPHeaderField:@"Cache-Control"];
    
    // Don't set the cookie policy URL if it's already been set.
    if ([request mainDocumentURL] == nil) {
        if (mainResource && (isLoadingMainFrame() || alwaysFromRequest))
            [request setMainDocumentURL:[request URL]];
        else
            [request setMainDocumentURL:[m_client _mainFrameURL]];
    }
    
    if (mainResource)
        [request setValue:@"text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5" forHTTPHeaderField:@"Accept"];
}

NSDictionary *FrameLoader::actionInformation(FrameLoadType type, bool isFormSubmission, NSEvent *event, NSURL *originalURL)
{
    NavigationType navType;
    if (isFormSubmission)
        navType = NavigationTypeFormSubmitted;
    else if (event)
        navType = NavigationTypeLinkClicked;
    else if (type == FrameLoadTypeReload)
        navType = NavigationTypeReload;
    else if (isBackForwardLoadType(type))
        navType = NavigationTypeBackForward;
    else
        navType = NavigationTypeOther;
    return actionInformation(navType, event, originalURL);
}
 
NSDictionary *FrameLoader::actionInformation(NavigationType navigationType, NSEvent *event, NSURL *originalURL)
{
    NSString *ActionNavigationTypeKey = @"WebActionNavigationTypeKey";
    NSString *ActionElementKey = @"WebActionElementKey";
    NSString *ActionButtonKey = @"WebActionButtonKey"; 
    NSString *ActionModifierFlagsKey = @"WebActionModifierFlagsKey";
    NSString *ActionOriginalURLKey = @"WebActionOriginalURLKey";
    NSDictionary *elementInfo = [m_client _elementForEvent:event];
    if (elementInfo)
        return [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:navigationType], ActionNavigationTypeKey,
            elementInfo, ActionElementKey,
            [NSNumber numberWithInt:[event buttonNumber]], ActionButtonKey,
            [NSNumber numberWithInt:[event modifierFlags]], ActionModifierFlagsKey,
            originalURL, ActionOriginalURLKey,
            nil];
    return [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt:navigationType], ActionNavigationTypeKey,
        [NSNumber numberWithInt:[event modifierFlags]], ActionModifierFlagsKey,
        originalURL, ActionOriginalURLKey,
        nil];
}

// Called every time a resource is completely loaded, or an error is received.
void FrameLoader::checkLoadComplete()
{
    ASSERT([m_client _hasWebView]);

    for (RefPtr<Frame> frame = m_frame; frame; frame = frame->tree()->parent())
        [Mac(frame.get())->bridge() frameLoader]->checkLoadCompleteForThisFrame();
}

void FrameLoader::setFrameLoaderClient(id <WebFrameLoaderClient> client)
{
    m_client = client;
}

id <WebFrameLoaderClient> FrameLoader::client() const
{
    return m_client;
}

WebCoreFrameLoaderAsDelegate *FrameLoader::asDelegate()
{
    if (!m_asDelegate) {
        WebCoreFrameLoaderAsDelegate *d = [[WebCoreFrameLoaderAsDelegate alloc] initWithLoader:this];
        m_asDelegate = d;
        [d release];
    }
    return m_asDelegate.get();
}

}

@implementation WebCoreFrameLoaderAsDelegate

- (id)initWithLoader:(FrameLoader*)loader
{
    self = [self init];
    if (!self)
        return nil;
    m_loader = loader;
    return self;
}

- (void)detachFromLoader
{
    m_loader = 0;
}

- (void)continueFragmentScrollAfterNavigationPolicy:(NSURLRequest *)request formState:(FormState*)formState
{
    if (m_loader)
        m_loader->continueFragmentScrollAfterNavigationPolicy(request);
}

- (void)continueAfterNewWindowPolicy:(WebPolicyAction)policy
{
    if (m_loader)
        m_loader->continueAfterNewWindowPolicy(policy);
}

- (void)continueAfterNavigationPolicy:(WebPolicyAction)policy
{
    if (m_loader)
        m_loader->continueAfterNavigationPolicy(policy);
}

- (void)continueAfterWillSubmitForm:(WebPolicyAction)policy
{
    if (m_loader)
        m_loader->continueAfterWillSubmitForm(policy);
}

- (void)continueLoadRequestAfterNavigationPolicy:(NSURLRequest *)request formState:(FormState*)formState
{
    if (m_loader)
        m_loader->continueLoadRequestAfterNavigationPolicy(request, formState);
}

- (void)continueLoadRequestAfterNewWindowPolicy:(NSURLRequest *)request frameName:(NSString *)frameName formState:(FormState *)formState
{
    if (m_loader)
        m_loader->continueLoadRequestAfterNewWindowPolicy(request, frameName, formState);
}

@end
