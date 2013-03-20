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
    : m_documentLoader(documentLoader)
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
    m_documentLoader->mainReceivedError(m_resource->resourceError());
}

void MainResourceLoader::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Loader);
    info.addMember(m_resource, "resource");
    info.addMember(m_initialRequest, "initialRequest");
    info.addMember(m_documentLoader, "documentLoader");
}

void MainResourceLoader::load(const ResourceRequest& initialRequest)
{
    ResourceRequest request(initialRequest);

    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, mainResourceLoadOptions,
        (SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, AskClientForCrossOriginCredentials, SkipSecurityCheck));
    CachedResourceRequest cachedResourceRequest(request, mainResourceLoadOptions);
    m_resource = documentLoader()->cachedResourceLoader()->requestMainResource(cachedResourceRequest);
    if (!m_resource) {
        documentLoader()->setRequest(ResourceRequest());
        return;
    }
    m_resource->addClient(this);
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
    if (ResourceLoader* resourceLoader = loader())
        return resourceLoader->identifier();
    return 0;
}

}
