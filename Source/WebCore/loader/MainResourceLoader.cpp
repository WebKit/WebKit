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
#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "Console.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentLoadTiming.h"
#include "DocumentLoader.h"
#include "FeatureObserver.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLFormElement.h"
#include "HistoryItem.h"
#include "InspectorInstrumentation.h"
#include "MemoryCache.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceBuffer.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "SchemeRegistry.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SubresourceLoader.h"
#include <wtf/CurrentTime.h>

#if PLATFORM(QT)
#include "PluginDatabase.h"
#endif

namespace WebCore {

MainResourceLoader::MainResourceLoader(DocumentLoader* documentLoader)
    : m_dataLoadTimer(this, &MainResourceLoader::handleSubstituteDataLoadNow)
    , m_documentLoader(documentLoader)
    , m_loadingMultipartContent(false)
    , m_waitingForContentPolicy(false)
    , m_identifierForLoadWithoutResourceLoader(0)
{
}

MainResourceLoader::~MainResourceLoader()
{
    clearResource();
}

PassRefPtr<MainResourceLoader> MainResourceLoader::create(DocumentLoader* documentLoader)
{
    return adoptRef(new MainResourceLoader(documentLoader));
}

void MainResourceLoader::receivedError(const ResourceError& error)
{
    // Calling receivedMainResourceError will likely result in the last reference to this object to go away.
    RefPtr<MainResourceLoader> protect(this);
    RefPtr<Frame> protectFrame(m_documentLoader->frame());

    if (m_identifierForLoadWithoutResourceLoader) {
        ASSERT(!loader());
        frameLoader()->client()->dispatchDidFailLoading(documentLoader(), m_identifierForLoadWithoutResourceLoader, error);
    }

    // It is important that we call DocumentLoader::mainReceivedError before calling 
    // ResourceLoadNotifier::didFailToLoad because mainReceivedError clears out the relevant
    // document loaders. Also, mainReceivedError ends up calling a FrameLoadDelegate method
    // and didFailToLoad calls a ResourceLoadDelegate method and they need to be in the correct order.
    documentLoader()->mainReceivedError(error);
}

void MainResourceLoader::cancel()
{
    cancel(ResourceError());
}

void MainResourceLoader::cancel(const ResourceError& error)
{
    RefPtr<MainResourceLoader> protect(this);
    ResourceError resourceError = error.isNull() ? frameLoader()->cancelledError(request()) : error;

    m_dataLoadTimer.stop();

    if (m_waitingForContentPolicy) {
        frameLoader()->policyChecker()->cancelCheck();
        ASSERT(m_waitingForContentPolicy);
        m_waitingForContentPolicy = false;
        deref(); // balances ref in responseReceived
    }

    if (loader())
        loader()->cancel(resourceError);

    clearResource();
    receivedError(resourceError);
}

void MainResourceLoader::clearResource()
{
    if (m_resource) {
        m_resource->removeClient(this);
        m_resource = 0;
    }
}

FrameLoader* MainResourceLoader::frameLoader() const
{
    return m_documentLoader->frameLoader();
}

const ResourceRequest& MainResourceLoader::request() const
{
    return m_resource ? m_resource->resourceRequest() : m_initialRequest;
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

PassRefPtr<ResourceBuffer> MainResourceLoader::resourceData()
{
    if (m_resource)
        return m_resource->resourceBuffer();
    return 0;
}

void MainResourceLoader::redirectReceived(CachedResource* resource, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    m_documentLoader->willSendRequest(request, redirectResponse);
}

void MainResourceLoader::continueAfterContentPolicy(PolicyAction contentPolicy, const ResourceResponse& r)
{
    KURL url = request().url();
    const String& mimeType = r.mimeType();
    
    switch (contentPolicy) {
    case PolicyUse: {
        // Prevent remote web archives from loading because they can claim to be from any domain and thus avoid cross-domain security checks (4120255).
        bool isRemoteWebArchive = (equalIgnoringCase("application/x-webarchive", mimeType)
#if PLATFORM(GTK)
                                   || equalIgnoringCase("message/rfc822", mimeType)
#endif
                                   || equalIgnoringCase("multipart/related", mimeType))
            && !m_documentLoader->substituteData().isValid() && !SchemeRegistry::shouldTreatURLSchemeAsLocal(url.protocol());
        if (!frameLoader()->client()->canShowMIMEType(mimeType) || isRemoteWebArchive) {
            frameLoader()->policyChecker()->cannotShowMIMEType(r);
            // Check reachedTerminalState since the load may have already been canceled inside of _handleUnimplementablePolicyWithErrorCode::.
            stopLoadingForPolicyChange();
            return;
        }
        break;
    }

    case PolicyDownload: {
        // m_resource can be null, e.g. when loading a substitute resource from application cache.
        if (!m_resource) {
            receivedError(frameLoader()->client()->cannotShowURLError(request()));
            return;
        }
        InspectorInstrumentation::continueWithPolicyDownload(m_documentLoader->frame(), documentLoader(), identifier(), r);

        // When starting the request, we didn't know that it would result in download and not navigation. Now we know that main document URL didn't change.
        // Download may use this knowledge for purposes unrelated to cookies, notably for setting file quarantine data.
        ResourceRequest request = this->request();
        frameLoader()->setOriginalURLForDownloadRequest(request);

        frameLoader()->client()->convertMainResourceLoadToDownload(documentLoader(), request, r);

        // It might have gone missing
        if (loader())
            loader()->didFail(interruptedForPolicyChangeError());
        return;
    }
    case PolicyIgnore:
        InspectorInstrumentation::continueWithPolicyIgnore(m_documentLoader->frame(), documentLoader(), identifier(), r);
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

    if (!m_documentLoader->isStopping() && m_documentLoader->substituteData().isValid()) {
        if (m_documentLoader->substituteData().content()->size())
            dataReceived(0, m_documentLoader->substituteData().content()->data(), m_documentLoader->substituteData().content()->size());
        if (m_documentLoader->isLoadingMainResource())
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
    if (!m_documentLoader->isStopping())
        continueAfterContentPolicy(policy, m_response);
    deref(); // balances ref in responseReceived
}

void MainResourceLoader::responseReceived(CachedResource* resource, const ResourceResponse& r)
{
    ASSERT_UNUSED(resource, m_resource == resource);
    bool willLoadFallback = documentLoader()->applicationCacheHost()->maybeLoadFallbackForMainResponse(request(), r);

    // The memory cache doesn't understand the application cache or its caching rules. So if a main resource is served
    // from the application cache, ensure we don't save the result for future use.
    bool shouldRemoveResourceFromCache = willLoadFallback;
#if PLATFORM(CHROMIUM)
    // chromium's ApplicationCacheHost implementation always returns true for maybeLoadFallbackForMainResponse(). However, all responses loaded
    // from appcache will have a non-zero appCacheID().
    if (r.appCacheID())
        shouldRemoveResourceFromCache = true;
#endif
    if (shouldRemoveResourceFromCache)
        memoryCache()->remove(m_resource.get());

    if (willLoadFallback)
        return;

    DEFINE_STATIC_LOCAL(AtomicString, xFrameOptionHeader, ("x-frame-options", AtomicString::ConstructFromLiteral));
    HTTPHeaderMap::const_iterator it = r.httpHeaderFields().find(xFrameOptionHeader);
    if (it != r.httpHeaderFields().end()) {
        String content = it->value;
        if (frameLoader()->shouldInterruptLoadForXFrameOptions(content, r.url(), identifier())) {
            InspectorInstrumentation::continueAfterXFrameOptionsDenied(m_documentLoader->frame(), documentLoader(), identifier(), r);
            String message = "Refused to display '" + r.url().elidedString() + "' in a frame because it set 'X-Frame-Options' to '" + content + "'.";
            m_documentLoader->frame()->document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, message, identifier());

            cancel();
            return;
        }
    }

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!defersLoading());
#endif

    if (m_loadingMultipartContent) {
        m_documentLoader->setupForReplace();
        m_resource->clear();
    }
    
    if (r.isMultipart()) {
        FeatureObserver::observe(m_documentLoader->frame()->document(), FeatureObserver::MultipartMainResource);
        m_loadingMultipartContent = true;
    }
        
    // The additional processing can do anything including possibly removing the last
    // reference to this object; one example of this is 3266216.
    RefPtr<MainResourceLoader> protect(this);

    m_documentLoader->responseReceived(r);

    m_response = r;

    if (m_identifierForLoadWithoutResourceLoader)
        frameLoader()->notifier()->dispatchDidReceiveResponse(documentLoader(), identifier(), m_response, 0);

    ASSERT(!m_waitingForContentPolicy);
    m_waitingForContentPolicy = true;
    ref(); // balanced by deref in continueAfterContentPolicy and cancel

    // Always show content with valid substitute data.
    if (m_documentLoader->substituteData().isValid()) {
        callContinueAfterContentPolicy(this, PolicyUse);
        return;
    }

#if ENABLE(FTPDIR)
    // Respect the hidden FTP Directory Listing pref so it can be tested even if the policy delegate might otherwise disallow it
    Settings* settings = m_documentLoader->frame()->settings();
    if (settings && settings->forceFTPDirectoryListings() && m_response.mimeType() == "application/x-ftp-directory") {
        callContinueAfterContentPolicy(this, PolicyUse);
        return;
    }
#endif

    frameLoader()->policyChecker()->checkContentPolicy(m_response, callContinueAfterContentPolicy, this);
}

void MainResourceLoader::dataReceived(CachedResource* resource, const char* data, int length)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    documentLoader()->receivedData(data, length);
}

void MainResourceLoader::didFinishLoading(double finishTime)
{
    // The additional processing can do anything including possibly removing the last
    // reference to this object.
    RefPtr<MainResourceLoader> protect(this);
    documentLoader()->finishedLoading(finishTime);
}

void MainResourceLoader::notifyFinished(CachedResource* resource)
{
    ASSERT_UNUSED(resource, m_resource == resource);
    ASSERT(m_resource);
    if (!m_resource->errorOccurred() && !m_resource->wasCanceled()) {
        didFinishLoading(m_resource->loadFinishTime());
        return;
    }

    // FIXME: we should fix the design to eliminate the need for a platform ifdef here
#if !PLATFORM(CHROMIUM)
    if (m_documentLoader->request().cachePolicy() == ReturnCacheDataDontLoad && !m_resource->wasCanceled()) {
        frameLoader()->retryAfterFailedCacheOnlyMainResourceLoad();
        return;
    }
#endif

    const ResourceError& error = m_resource->resourceError();
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForMainError(request(), error))
        return;

    // There is a bug in CFNetwork where callbacks can be dispatched even when loads are deferred.
    // See <rdar://problem/6304600> for more details.
#if !USE(CF)
    ASSERT(!defersLoading());
#endif
    
    receivedError(error);
}

void MainResourceLoader::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Loader);
    info.addMember(m_resource, "resource");
    info.addMember(m_initialRequest, "initialRequest");
    info.addMember(m_dataLoadTimer, "dataLoadTimer");
    info.addMember(m_documentLoader, "documentLoader");
}

void MainResourceLoader::handleSubstituteDataLoadNow(MainResourceLoaderTimer*)
{
    RefPtr<MainResourceLoader> protect(this);

    KURL url = m_documentLoader->substituteData().responseURL();
    if (url.isEmpty())
        url = m_initialRequest.url();

    // Clear the initial request here so that subsequent entries into the
    // loader will not think there's still a deferred load left to do.
    m_initialRequest = ResourceRequest();
        
    ResourceResponse response(url, m_documentLoader->substituteData().mimeType(), m_documentLoader->substituteData().content()->size(), m_documentLoader->substituteData().textEncoding(), "");
    responseReceived(0, response);
}

void MainResourceLoader::startDataLoadTimer()
{
    m_dataLoadTimer.startOneShot(0);

#if HAVE(RUNLOOP_TIMER)
    if (SchedulePairHashSet* scheduledPairs = m_documentLoader->frame()->page()->scheduledRunLoopPairs())
        m_dataLoadTimer.schedule(*scheduledPairs);
#endif
}

void MainResourceLoader::handleSubstituteDataLoadSoon(const ResourceRequest& r)
{
    m_initialRequest = r;
    
    if (m_documentLoader->deferMainResourceDataLoad())
        startDataLoadTimer();
    else
        handleSubstituteDataLoadNow(0);
}

void MainResourceLoader::load(const ResourceRequest& initialRequest)
{
    RefPtr<MainResourceLoader> protect(this);
    ResourceRequest request(initialRequest);

    if (m_documentLoader->substituteData().isValid()) {
        m_identifierForLoadWithoutResourceLoader = m_documentLoader->frame()->page()->progress()->createUniqueIdentifier();
        frameLoader()->notifier()->assignIdentifierToInitialRequest(m_identifierForLoadWithoutResourceLoader, documentLoader(), request);
        frameLoader()->notifier()->dispatchWillSendRequest(documentLoader(), m_identifierForLoadWithoutResourceLoader, request, ResourceResponse());
        handleSubstituteDataLoadSoon(request);
        return;
    }

    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, mainResourceLoadOptions,
        (SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForCrossOriginCredentials, SkipSecurityCheck));
    CachedResourceRequest cachedResourceRequest(request, mainResourceLoadOptions);
    m_resource = documentLoader()->cachedResourceLoader()->requestMainResource(cachedResourceRequest);
    if (!m_resource) {
        documentLoader()->setRequest(ResourceRequest());
        return;
    }
    if (!loader()) {
        m_identifierForLoadWithoutResourceLoader = m_documentLoader->frame()->page()->progress()->createUniqueIdentifier();
        frameLoader()->notifier()->assignIdentifierToInitialRequest(m_identifierForLoadWithoutResourceLoader, documentLoader(), request);
        frameLoader()->notifier()->dispatchWillSendRequest(documentLoader(), m_identifierForLoadWithoutResourceLoader, request, ResourceResponse());
    }
    m_resource->addClient(this);

    // A bunch of headers are set when the underlying ResourceLoader is created, and DocumentLoader::m_request needs to include those.
    if (loader())
        request = loader()->originalRequest();
    // If there was a fragment identifier on initialRequest, the cache will have stripped it. DocumentLoader::m_request should include
    // the fragment identifier, so add that back in.
    if (equalIgnoringFragmentIdentifier(initialRequest.url(), request.url()))
        request.setURL(initialRequest.url());
    documentLoader()->setRequest(request);
}

void MainResourceLoader::setDefersLoading(bool defers)
{
    if (loader())
        loader()->setDefersLoading(defers);
}

bool MainResourceLoader::defersLoading() const
{
    return loader() ? loader()->defersLoading() : false;
}

void MainResourceLoader::setDataBufferingPolicy(DataBufferingPolicy dataBufferingPolicy)
{
    if (m_resource)
        m_resource->setDataBufferingPolicy(dataBufferingPolicy);
}

ResourceLoader* MainResourceLoader::loader() const
{ 
    return m_resource ? m_resource->loader() : 0;
}

unsigned long MainResourceLoader::identifier() const
{
    ASSERT(!m_identifierForLoadWithoutResourceLoader || !loader() || !loader()->identifier());
    if (m_identifierForLoadWithoutResourceLoader)
        return m_identifierForLoadWithoutResourceLoader;
    if (ResourceLoader* resourceLoader = loader())
        return resourceLoader->identifier();
    return 0;
}

}
