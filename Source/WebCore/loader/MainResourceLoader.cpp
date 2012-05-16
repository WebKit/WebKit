/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "config.h"
#include "MainResourceLoader.h"

#include "ApplicationCacheHost.h"
#include "BackForwardController.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentLoadTiming.h"
#include "DocumentLoader.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLFormElement.h"
#include "HistoryItem.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoadScheduler.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/CurrentTime.h>

#if PLATFORM(QT)
#include "PluginDatabase.h"
#endif

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
#include "WebCoreSystemInterface.h"
#endif

// FIXME: More that is in common with SubresourceLoader should move up into ResourceLoader.

namespace WebCore {

static bool shouldLoadAsEmptyDocument(const KURL& url)
{
    return url.isEmpty() || SchemeRegistry::shouldLoadURLSchemeAsEmptyDocument(url.protocol());
}

MainResourceLoader::MainResourceLoader(Frame* frame)
    : ResourceLoader(frame, ResourceLoaderOptions(SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForCrossOriginCredentials, SkipSecurityCheck))
    , m_dataLoadTimer(this, &MainResourceLoader::handleDataLoadNow)
    , m_loadingMultipartContent(false)
    , m_waitingForContentPolicy(false)
    , m_timeOfLastDataReceived(0.0)
#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    , m_filter(0)
#endif
{
}

MainResourceLoader::~MainResourceLoader()
{
#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    ASSERT(!m_filter);
#endif
}

PassRefPtr<MainResourceLoader> MainResourceLoader::create(Frame* frame)
{
    return adoptRef(new MainResourceLoader(frame));
}

void MainResourceLoader::receivedError(const ResourceError& error)
{
    // Calling receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);
    RefPtr<Frame> protectFrame(m_frame);

    // It is important that we call DocumentLoader::mainReceivedError before calling 
    // ResourceLoadNotifier::didFailToLoad because mainReceivedError clears out the relevant
    // document loaders. Also, mainReceivedError ends up calling a FrameLoadDelegate method
    // and didFailToLoad calls a ResourceLoadDelegate method and they need to be in the correct order.
    documentLoader()->mainReceivedError(error);

    if (!cancelled()) {
        ASSERT(!reachedTerminalState());
        frameLoader()->notifier()->didFailToLoad(this, error);
        
        releaseResources();
    }

    ASSERT(reachedTerminalState());
}

void MainResourceLoader::willCancel(const ResourceError&)
{
    m_dataLoadTimer.stop();

    if (m_waitingForContentPolicy) {
        frameLoader()->policyChecker()->cancelCheck();
        ASSERT(m_waitingForContentPolicy);
        m_waitingForContentPolicy = false;
        deref(); // balances ref in didReceiveResponse
    }
}

void MainResourceLoader::didCancel(const ResourceError& error)
{
    // We should notify the frame loader after fully canceling the load, because it can do complicated work
    // like calling DOMWindow::print(), during which a half-canceled load could try to finish.
    documentLoader()->mainReceivedError(error);

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    if (m_filter) {
        wkFilterRelease(m_filter);
        m_filter = 0;
    }
#endif
}

ResourceError MainResourceLoader::interruptedForPolicyChangeError() const
{
    return frameLoader()->client()->interruptedForPolicyChangeError(request());
}

void MainResourceLoader::stopLoadingForPolicyChange()
{
    ResourceError error = interruptedForPolicyChangeError();
    error.setIsCancellation(true);
    cancel(error);
}

void MainResourceLoader::callContinueAfterNavigationPolicy(void* argument, const ResourceRequest& request, PassRefPtr<FormState>, bool shouldContinue)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterNavigationPolicy(request, shouldContinue);
}

void MainResourceLoader::continueAfterNavigationPolicy(const ResourceRequest& request, bool shouldContinue)
{
    if (!shouldContinue)
        stopLoadingForPolicyChange();
    else if (m_substituteData.isValid()) {
        // A redirect resulted in loading substitute data.
        ASSERT(documentLoader()->timing()->redirectCount());
        handle()->cancel();
        handleDataLoadSoon(request);
    }

    deref(); // balances ref in willSendRequest
}

bool MainResourceLoader::isPostOrRedirectAfterPost(const ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (newRequest.httpMethod() == "POST")
        return true;

    int status = redirectResponse.httpStatusCode();
    if (((status >= 301 && status <= 303) || status == 307)
        && frameLoader()->initialRequest().httpMethod() == "POST")
        return true;
    
    return false;
}

void MainResourceLoader::addData(const char* data, int length, bool allAtOnce)
{
    ResourceLoader::addData(data, length, allAtOnce);
    documentLoader()->receivedData(data, length);
}

void MainResourceLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Note that there are no asserts here as there are for the other callbacks. This is due to the
    // fact that this "callback" is sent when starting every load, and the state of callback
    // deferrals plays less of a part in this function in preventing the bad behavior deferring 
    // callbacks is meant to prevent.
    ASSERT(!newRequest.isNull());

    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    ASSERT(documentLoader()->timing()->fetchStart());
    if (!redirectResponse.isNull()) {
        // If the redirecting url is not allowed to display content from the target origin,
        // then block the redirect.
        RefPtr<SecurityOrigin> redirectingOrigin = SecurityOrigin::create(redirectResponse.url());
        if (!redirectingOrigin->canDisplay(newRequest.url())) {
            FrameLoader::reportLocalLoadFailed(m_frame.get(), newRequest.url().string());
            cancel();
            return;
        }
        documentLoader()->timing()->addRedirect(redirectResponse.url(), newRequest.url());
    }

    // Update cookie policy base URL as URL changes, except for subframes, which use the
    // URL of the main frame which doesn't change when we redirect.
    if (frameLoader()->isLoadingMainFrame())
        newRequest.setFirstPartyForCookies(newRequest.url());

    // If we're fielding a redirect in response to a POST, force a load from origin, since
    // this is a common site technique to return to a page viewing some data that the POST
    // just modified.
    // Also, POST requests always load from origin, but this does not affect subresources.
    if (newRequest.cachePolicy() == UseProtocolCachePolicy && isPostOrRedirectAfterPost(newRequest, redirectResponse))
        newRequest.setCachePolicy(ReloadIgnoringCacheData);

    Frame* top = m_frame->tree()->top();
    if (top != m_frame) {
        if (!frameLoader()->checkIfDisplayInsecureContent(top->document()->securityOrigin(), newRequest.url())) {
            cancel();
            return;
        }
    }

    ResourceLoader::willSendRequest(newRequest, redirectResponse);

    // Don't set this on the first request. It is set when the main load was started.
    m_documentLoader->setRequest(newRequest);

    if (!redirectResponse.isNull()) {
        // We checked application cache for initial URL, now we need to check it for redirected one.
        ASSERT(!m_substituteData.isValid());
        documentLoader()->applicationCacheHost()->maybeLoadMainResourceForRedirect(newRequest, m_substituteData);
    }

    // FIXME: Ideally we'd stop the I/O until we hear back from the navigation policy delegate
    // listener. But there's no way to do that in practice. So instead we cancel later if the
    // listener tells us to. In practice that means the navigation policy needs to be decided
    // synchronously for these redirect cases.
    if (!redirectResponse.isNull()) {
        ref(); // balanced by deref in continueAfterNavigationPolicy
        frameLoader()->policyChecker()->checkNavigationPolicy(newRequest, callContinueAfterNavigationPolicy, this);
    }
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction contentPolicy, const ResourceResponse& r)
{
    KURL url = request().url();
    const String& mimeType = r.mimeType();
    
    switch (contentPolicy) {
    case PolicyUse: {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        bool isRemoteWebArchive = (equalIgnoringCase("application/x-webarchive", mimeType) || equalIgnoringCase("multipart/related", mimeType))
            && !m_substituteData.isValid() && !SchemeRegistry::shouldTreatURLSchemeAsLocal(url);
        if (!frameLoader()->client()->canShowMIMEType(mimeType) || isRemoteWebArchive) {
            frameLoader()->policyChecker()->cannotShowMIMEType(r);
            // Check reachedTerminalState since the load may have already been cancelled inside of _handleUnimplementablePolicyWithErrorCode::.
            if (!reachedTerminalState())
                stopLoadingForPolicyChange();
            return;
        }
        break;
    }

    case PolicyDownload: {
        // m_handle can be null, e.g. when loading a substitute resource from application cache.
        if (!m_handle) {
            receivedError(cannotShowURLError());
            return;
        }
        InspectorInstrumentation::continueWithPolicyDownload(m_frame.get(), documentLoader(), identifier(), r);

        // When starting the request, we didn't know that it would result in download and not navigation. Now we know that main document URL didn't change.
        // Download may use this knowledge for purposes unrelated to cookies, notably for setting file quarantine data.
        ResourceRequest request = this->request();
        frameLoader()->setOriginalURLForDownloadRequest(request);

        frameLoader()->client()->download(m_handle.get(), request, r);

        // It might have gone missing
        if (frameLoader())
            receivedError(interruptedForPolicyChangeError());
        return;
    }
    case PolicyIgnore:
        InspectorInstrumentation::continueWithPolicyIgnore(m_frame.get(), documentLoader(), identifier(), r);
        stopLoadingForPolicyChange();
        return;
    
    default:
        ASSERT_NOT_REACHED();
    }

    RefPtr<MainResourceLoader> protect(this);

    if (r.isHTTP()) {
        int status = r.httpStatusCode();
        if (status < 200 || status >= 300) {
            bool hostedByObject = frameLoader()->isHostedByObjectElement();

            frameLoader()->handleFallbackContent();
            // object elements are no longer rendered after we fallback, so don't
            // keep trying to process data from their load

            if (hostedByObject)
                cancel();
        }
    }

    // we may have cancelled this load as part of switching to fallback content
    if (!reachedTerminalState())
        ResourceLoader::didReceiveResponse(r);

    if (frameLoader() && !frameLoader()->activeDocumentLoader()->isStopping()) {
        if (m_substituteData.isValid()) {
            if (m_substituteData.content()->size())
                didReceiveData(m_substituteData.content()->data(), m_substituteData.content()->size(), m_substituteData.content()->size(), true);
            if (frameLoader() && !frameLoader()->activeDocumentLoader()->isStopping()) 
                didFinishLoading(0);
        } else if (shouldLoadAsEmptyDocument(url) || frameLoader()->client()->representationExistsForURLScheme(url.protocol()))
            didFinishLoading(0);
    }
}

void MainResourceLoader::callContinueAfterContentPolicy(void* argument, PolicyAction policy)
{
    static_cast<MainResourceLoader*>(argument)->continueAfterContentPolicy(policy);
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction policy)
{
    ASSERT(m_waitingForContentPolicy);
    m_waitingForContentPolicy = false;
    if (frameLoader() && !frameLoader()->activeDocumentLoader()->isStopping())
        continueAfterContentPolicy(policy, m_response);
    deref(); // balances ref in didReceiveResponse
}

#if PLATFORM(QT)
void MainResourceLoader::substituteMIMETypeFromPluginDatabase(const ResourceResponse& r)
{
    if (!m_frame->loader()->subframeLoader()->allowPlugins(NotAboutToInstantiatePlugin))
        return;

    String filename = r.url().lastPathComponent();
    if (filename.endsWith('/'))
        return;

    size_t extensionPos = filename.reverseFind('.');
    if (extensionPos == notFound)
        return;

    String extension = filename.substring(extensionPos + 1);
    String mimeType = PluginDatabase::installedPlugins()->MIMETypeForExtension(extension);
    if (!mimeType.isEmpty()) {
        ResourceResponse* response = const_cast<ResourceResponse*>(&r);
        response->setMimeType(mimeType);
    }
}
#endif

void MainResourceLoader::didReceiveResponse(const ResourceResponse& r)
{
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForMainResponse(request(), r))
        return;

    HTTPHeaderMap::const_iterator it = r.httpHeaderFields().find(AtomicString("x-frame-options"));
    if (it != r.httpHeaderFields().end()) {
        String content = it->second;
        if (m_frame->loader()->shouldInterruptLoadForXFrameOptions(content, r.url())) {
            InspectorInstrumentation::continueAfterXFrameOptionsDenied(m_frame.get(), documentLoader(), identifier(), r);
            DEFINE_STATIC_LOCAL(String, consoleMessage, ("Refused to display document because display forbidden by X-Frame-Options.\n"));
            m_frame->domWindow()->console()->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, consoleMessage);

            cancel();
            return;
        }
    }

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(shouldLoadAsEmptyDocument(r.url()) || !defersLoading());
#endif

#if PLATFORM(QT)
    if (r.mimeType() == "application/octet-stream")
        substituteMIMETypeFromPluginDatabase(r);
#endif

    if (m_loadingMultipartContent) {
        frameLoader()->activeDocumentLoader()->setupForReplaceByMIMEType(r.mimeType());
        clearResourceData();
    }
    
    if (r.isMultipart())
        m_loadingMultipartContent = true;
        
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    m_documentLoader->setResponse(r);

    m_response = r;

    ASSERT(!m_waitingForContentPolicy);
    m_waitingForContentPolicy = true;
    ref(); // balanced by deref in continueAfterContentPolicy and didCancel

    ASSERT(frameLoader()->activeDocumentLoader());

    // Always show content with valid substitute data.
    if (frameLoader()->activeDocumentLoader()->substituteData().isValid()) {
        callContinueAfterContentPolicy(this, PolicyUse);
        return;
    }

#if ENABLE(FTPDIR)
    // Respect the hidden FTP Directory Listing pref so it can be tested even if the policy delegate might otherwise disallow it
    Settings* settings = m_frame->settings();
    if (settings && settings->forceFTPDirectoryListings() && m_response.mimeType() == "application/x-ftp-directory") {
        callContinueAfterContentPolicy(this, PolicyUse);
        return;
    }
#endif

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    if (r.url().protocolIs("https") && wkFilterIsManagedSession())
        m_filter = wkFilterCreateInstance(r.nsURLResponse());
#endif

    frameLoader()->policyChecker()->checkContentPolicy(m_response, callContinueAfterContentPolicy, this);
}

void MainResourceLoader::didReceiveData(const char* data, int length, long long encodedDataLength, bool allAtOnce)
{
    ASSERT(data);
    ASSERT(length != 0);

    ASSERT(!m_response.isNull());

#if USE(CFNETWORK) || PLATFORM(MAC)
    // Workaround for <rdar://problem/6060782>
    if (m_response.isNull()) {
        m_response = ResourceResponse(KURL(), "text/html", 0, String(), String());
        if (DocumentLoader* documentLoader = frameLoader()->activeDocumentLoader())
            documentLoader->setResponse(m_response);
    }
#endif

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!defersLoading());
#endif

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    if (m_filter) {
        ASSERT(!wkFilterWasBlocked(m_filter));
        const char* blockedData = wkFilterAddData(m_filter, data, &length);
        // If we don't have blockedData, that means we're still accumulating data
        if (!blockedData) {
            // Transition to committed state.
            ResourceLoader::didReceiveData("", 0, 0, false);
            return;
        }

        data = blockedData;
        encodedDataLength = -1;
    }
#endif

    documentLoader()->applicationCacheHost()->mainResourceDataReceived(data, length, encodedDataLength, allAtOnce);

    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    m_timeOfLastDataReceived = monotonicallyIncreasingTime();

    ResourceLoader::didReceiveData(data, length, encodedDataLength, allAtOnce);

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    if (WebFilterEvaluator *filter = m_filter) {
        // If we got here, it means we know if we were blocked or not. If we were blocked, we're
        // done loading the page altogether. Either way, we don't need the filter anymore.

        // Remove this->m_filter early so didFinishLoading doesn't see it.
        m_filter = 0;
        if (wkFilterWasBlocked(filter))
            cancel();
        wkFilterRelease(filter);
    }
#endif
}

void MainResourceLoader::didFinishLoading(double finishTime)
{
    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(shouldLoadAsEmptyDocument(frameLoader()->activeDocumentLoader()->url()) || !defersLoading() || InspectorInstrumentation::isDebuggerPaused(m_frame.get()));
#endif

    // The additional processing can do anything including possibly removing the last
    // reference to this object.
    RefPtr<MainResourceLoader> protect(this);
    RefPtr<DocumentLoader> dl = documentLoader();

#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    if (m_filter) {
        int length;
        const char* data = wkFilterDataComplete(m_filter, &length);
        WebFilterEvaluator *filter = m_filter;
        // Remove this->m_filter early so didReceiveData doesn't see it.
        m_filter = 0;
        if (data)
            didReceiveData(data, length, -1, false);
        wkFilterRelease(filter);
    }
#endif

    if (m_loadingMultipartContent)
        dl->maybeFinishLoadingMultipartContent();

    documentLoader()->timing()->setResponseEnd(finishTime ? finishTime : (m_timeOfLastDataReceived ? m_timeOfLastDataReceived : monotonicallyIncreasingTime()));
    documentLoader()->finishedLoading();
    ResourceLoader::didFinishLoading(finishTime);

    dl->applicationCacheHost()->finishedLoadingMainResource();
}

void MainResourceLoader::didFail(const ResourceError& error)
{
#if PLATFORM(MAC) && !defined(BUILDING_ON_SNOW_LEOPARD) && !defined(BUILDING_ON_LION) && !PLATFORM(IOS)
    if (m_filter) {
        wkFilterRelease(m_filter);
        m_filter = 0;
    }
#endif

    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForMainError(request(), error))
        return;

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!defersLoading());
#endif
    
    receivedError(error);
}

void MainResourceLoader::handleEmptyLoad(const KURL& url, bool forURLScheme)
{
    String mimeType;
    if (forURLScheme)
        mimeType = frameLoader()->client()->generatedMIMETypeForURLScheme(url.protocol());
    else
        mimeType = "text/html";
    
    ResourceResponse response(url, mimeType, 0, String(), String());
    didReceiveResponse(response);
}

void MainResourceLoader::handleDataLoadNow(MainResourceLoaderTimer*)
{
    RefPtr<MainResourceLoader> protect(this);

    KURL url = m_substituteData.responseURL();
    if (url.isEmpty())
        url = m_initialRequest.url();

    // Clear the initial request here so that subsequent entries into the
    // loader will not think there's still a deferred load left to do.
    m_initialRequest = ResourceRequest();
        
    ResourceResponse response(url, m_substituteData.mimeType(), m_substituteData.content()->size(), m_substituteData.textEncoding(), "");
    didReceiveResponse(response);
}

void MainResourceLoader::startDataLoadTimer()
{
    m_dataLoadTimer.startOneShot(0);

#if HAVE(RUNLOOP_TIMER)
    if (SchedulePairHashSet* scheduledPairs = m_frame->page()->scheduledRunLoopPairs())
        m_dataLoadTimer.schedule(*scheduledPairs);
#endif
}

void MainResourceLoader::handleDataLoadSoon(const ResourceRequest& r)
{
    m_initialRequest = r;
    
    if (m_documentLoader->deferMainResourceDataLoad())
        startDataLoadTimer();
    else
        handleDataLoadNow(0);
}

bool MainResourceLoader::loadNow(ResourceRequest& r)
{
    bool shouldLoadEmptyBeforeRedirect = shouldLoadAsEmptyDocument(r.url());

    ASSERT(!m_handle);
    ASSERT(shouldLoadEmptyBeforeRedirect || !defersLoading());

    // Send this synthetic delegate callback since clients expect it, and
    // we no longer send the callback from within NSURLConnection for
    // initial requests.
    willSendRequest(r, ResourceResponse());
    ASSERT(!deletionHasBegun());

    // <rdar://problem/4801066>
    // willSendRequest() is liable to make the call to frameLoader() return NULL, so we need to check that here
    if (!frameLoader())
        return false;
    
    const KURL& url = r.url();
    bool shouldLoadEmpty = shouldLoadAsEmptyDocument(url) && !m_substituteData.isValid();

    if (shouldLoadEmptyBeforeRedirect && !shouldLoadEmpty && defersLoading())
        return true;

    resourceLoadScheduler()->addMainResourceLoad(this);
    if (m_substituteData.isValid()) 
        handleDataLoadSoon(r);
    else if (shouldLoadEmpty || frameLoader()->client()->representationExistsForURLScheme(url.protocol()))
        handleEmptyLoad(url, !shouldLoadEmpty);
    else
        m_handle = ResourceHandle::create(m_frame->loader()->networkingContext(), r, this, false, true);

    return false;
}

void MainResourceLoader::load(const ResourceRequest& r, const SubstituteData& substituteData)
{
    ASSERT(!m_handle);

    // It appears that it is possible for this load to be cancelled and derefenced by the DocumentLoader
    // in willSendRequest() if loadNow() is called.
    RefPtr<MainResourceLoader> protect(this);

    m_substituteData = substituteData;

    ASSERT(documentLoader()->timing()->navigationStart());
    ASSERT(!documentLoader()->timing()->fetchStart());
    documentLoader()->timing()->markFetchStart();
    ResourceRequest request(r);

    documentLoader()->applicationCacheHost()->maybeLoadMainResource(request, m_substituteData);

    bool defer = defersLoading();
    if (defer) {
        bool shouldLoadEmpty = shouldLoadAsEmptyDocument(request.url());
        if (shouldLoadEmpty)
            defer = false;
    }
    if (!defer) {
        if (loadNow(request)) {
            // Started as an empty document, but was redirected to something non-empty.
            ASSERT(defersLoading());
            defer = true;
        }
    }
    if (defer)
        m_initialRequest = request;
}

void MainResourceLoader::setDefersLoading(bool defers)
{
    ResourceLoader::setDefersLoading(defers);

    if (defers) {
        if (m_dataLoadTimer.isActive())
            m_dataLoadTimer.stop();
    } else {
        if (m_initialRequest.isNull())
            return;

        if (m_substituteData.isValid() && m_documentLoader->deferMainResourceDataLoad())
            startDataLoadTimer();
        else {
            ResourceRequest r(m_initialRequest);
            m_initialRequest = ResourceRequest();
            loadNow(r);
        }
    }
}

}
