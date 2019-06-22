/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "ResourceLoadNotifier.h"

#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceLoader.h"
#include "RuntimeEnabledFeatures.h"

#if USE(QUICK_LOOK)
#include "QuickLook.h"
#endif

namespace WebCore {

ResourceLoadNotifier::ResourceLoadNotifier(Frame& frame)
    : m_frame(frame)
{
}

void ResourceLoadNotifier::didReceiveAuthenticationChallenge(ResourceLoader* loader, const AuthenticationChallenge& currentWebChallenge)
{
    didReceiveAuthenticationChallenge(loader->identifier(), loader->documentLoader(), currentWebChallenge);
}

void ResourceLoadNotifier::didReceiveAuthenticationChallenge(unsigned long identifier, DocumentLoader* loader, const AuthenticationChallenge& currentWebChallenge)
{
    m_frame.loader().client().dispatchDidReceiveAuthenticationChallenge(loader, identifier, currentWebChallenge);
}

void ResourceLoadNotifier::willSendRequest(ResourceLoader* loader, ResourceRequest& clientRequest, const ResourceResponse& redirectResponse)
{
    m_frame.loader().applyUserAgentIfNeeded(clientRequest);

    dispatchWillSendRequest(loader->documentLoader(), loader->identifier(), clientRequest, redirectResponse);
}

void ResourceLoadNotifier::didReceiveResponse(ResourceLoader* loader, const ResourceResponse& r)
{
    loader->documentLoader()->addResponse(r);

    if (Page* page = m_frame.page())
        page->progress().incrementProgress(loader->identifier(), r);

    dispatchDidReceiveResponse(loader->documentLoader(), loader->identifier(), r, loader);
}

void ResourceLoadNotifier::didReceiveData(ResourceLoader* loader, const char* data, int dataLength, int encodedDataLength)
{
    if (Page* page = m_frame.page())
        page->progress().incrementProgress(loader->identifier(), dataLength);

    dispatchDidReceiveData(loader->documentLoader(), loader->identifier(), data, dataLength, encodedDataLength);
}

void ResourceLoadNotifier::didFinishLoad(ResourceLoader* loader, const NetworkLoadMetrics& networkLoadMetrics)
{    
    if (Page* page = m_frame.page())
        page->progress().completeProgress(loader->identifier());

    dispatchDidFinishLoading(loader->documentLoader(), loader->identifier(), networkLoadMetrics, loader);
}

void ResourceLoadNotifier::didFailToLoad(ResourceLoader* loader, const ResourceError& error)
{
    if (Page* page = m_frame.page())
        page->progress().completeProgress(loader->identifier());

    // Notifying the FrameLoaderClient may cause the frame to be destroyed.
    Ref<Frame> protect(m_frame);
    if (!error.isNull())
        m_frame.loader().client().dispatchDidFailLoading(loader->documentLoader(), loader->identifier(), error);

    InspectorInstrumentation::didFailLoading(&m_frame, loader->documentLoader(), loader->identifier(), error);
}

void ResourceLoadNotifier::assignIdentifierToInitialRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
    m_frame.loader().client().assignIdentifierToInitialRequest(identifier, loader, request);
}

void ResourceLoadNotifier::dispatchWillSendRequest(DocumentLoader* loader, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
#if USE(QUICK_LOOK)
    // Always allow QuickLook-generated URLs based on the protocol scheme.
    if (!request.isNull() && isQuickLookPreviewURL(request.url()))
        return;
#endif

    String oldRequestURL = request.url().string();

    ASSERT(m_frame.loader().documentLoader());
    if (m_frame.loader().documentLoader())
        m_frame.loader().documentLoader()->didTellClientAboutLoad(request.url());

    // Notifying the FrameLoaderClient may cause the frame to be destroyed.
    Ref<Frame> protect(m_frame);
    m_frame.loader().client().dispatchWillSendRequest(loader, identifier, request, redirectResponse);

    // If the URL changed, then we want to put that new URL in the "did tell client" set too.
    if (!request.isNull() && oldRequestURL != request.url().string() && m_frame.loader().documentLoader())
        m_frame.loader().documentLoader()->didTellClientAboutLoad(request.url());

    InspectorInstrumentation::willSendRequest(&m_frame, identifier, loader, request, redirectResponse);
}

void ResourceLoadNotifier::dispatchDidReceiveResponse(DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r, ResourceLoader* resourceLoader)
{
    // Notifying the FrameLoaderClient may cause the frame to be destroyed.
    Ref<Frame> protect(m_frame);
    m_frame.loader().client().dispatchDidReceiveResponse(loader, identifier, r);

    InspectorInstrumentation::didReceiveResourceResponse(m_frame, identifier, loader, r, resourceLoader);
}

void ResourceLoadNotifier::dispatchDidReceiveData(DocumentLoader* loader, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    // Notifying the FrameLoaderClient may cause the frame to be destroyed.
    Ref<Frame> protect(m_frame);
    m_frame.loader().client().dispatchDidReceiveContentLength(loader, identifier, dataLength);

    InspectorInstrumentation::didReceiveData(&m_frame, identifier, data, dataLength, encodedDataLength);
}

void ResourceLoadNotifier::dispatchDidFinishLoading(DocumentLoader* loader, unsigned long identifier, const NetworkLoadMetrics& networkLoadMetrics, ResourceLoader* resourceLoader)
{
    // Notifying the FrameLoaderClient may cause the frame to be destroyed.
    Ref<Frame> protect(m_frame);
    m_frame.loader().client().dispatchDidFinishLoading(loader, identifier);

    InspectorInstrumentation::didFinishLoading(&m_frame, loader, identifier, networkLoadMetrics, resourceLoader);
}

void ResourceLoadNotifier::dispatchDidFailLoading(DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    // Notifying the FrameLoaderClient may cause the frame to be destroyed.
    Ref<Frame> protect(m_frame);
    m_frame.loader().client().dispatchDidFailLoading(loader, identifier, error);

    InspectorInstrumentation::didFailLoading(&m_frame, loader, identifier, error);
}

void ResourceLoadNotifier::sendRemainingDelegateMessages(DocumentLoader* loader, unsigned long identifier, const ResourceRequest& request, const ResourceResponse& response, const char* data, int dataLength, int encodedDataLength, const ResourceError& error)
{
    // If the request is null, willSendRequest cancelled the load. We should
    // only dispatch didFailLoading in this case.
    if (request.isNull()) {
        ASSERT(error.isCancellation());
        dispatchDidFailLoading(loader, identifier, error);
        return;
    }

    if (!response.isNull())
        dispatchDidReceiveResponse(loader, identifier, response);

    if (dataLength > 0)
        dispatchDidReceiveData(loader, identifier, data, dataLength, encodedDataLength);

    if (error.isNull()) {
        NetworkLoadMetrics emptyMetrics;
        dispatchDidFinishLoading(loader, identifier, emptyMetrics, nullptr);
    } else
        dispatchDidFailLoading(loader, identifier, error);
}

} // namespace WebCore
