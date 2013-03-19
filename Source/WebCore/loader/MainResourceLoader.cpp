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

void MainResourceLoader::responseReceived(CachedResource* resource, const ResourceResponse& r)
{
    ASSERT_UNUSED(resource, m_resource == resource);
    m_documentLoader->responseReceived(r);
}

void MainResourceLoader::dataReceived(CachedResource* resource, const char* data, int length)
{
    ASSERT_UNUSED(resource, resource == m_resource);
    documentLoader()->receivedData(data, length);
}

void MainResourceLoader::notifyFinished(CachedResource* resource)
{
    ASSERT_UNUSED(resource, m_resource == resource);
    ASSERT(m_resource);
    if (!m_resource->errorOccurred() && !m_resource->wasCanceled()) {
        documentLoader()->finishedLoading(m_resource->loadFinishTime());
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
