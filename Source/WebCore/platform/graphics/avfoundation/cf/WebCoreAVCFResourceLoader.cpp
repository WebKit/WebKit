/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCoreAVCFResourceLoader.h"

#if ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_LOADER_DELEGATE)

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "MediaPlayerPrivateAVFoundationCF.h"
#include "NotImplemented.h"
#include "ResourceBuffer.h"
#include "ResourceLoaderOptions.h"
#include "SharedBuffer.h"
#include "SoftLinking.h"
#include <AVFoundationCF/AVFoundationCF.h>
#include <AVFoundationCF/AVCFAssetResourceLoader.h>
#include <wtf/text/CString.h>

// The softlink header files must be included after the AVCF and CoreMedia header files.
#include "AVFoundationCFSoftLinking.h"

namespace WebCore {

PassRefPtr<WebCoreAVCFResourceLoader> WebCoreAVCFResourceLoader::create(MediaPlayerPrivateAVFoundationCF* parent, AVCFAssetResourceLoadingRequestRef avRequest)
{
    ASSERT(avRequest);
    ASSERT(parent);
    return adoptRef(new WebCoreAVCFResourceLoader(parent, avRequest));
}

WebCoreAVCFResourceLoader::WebCoreAVCFResourceLoader(MediaPlayerPrivateAVFoundationCF* parent, AVCFAssetResourceLoadingRequestRef avRequest)
    : m_parent(parent)
    , m_avRequest(avRequest)
{
}

WebCoreAVCFResourceLoader::~WebCoreAVCFResourceLoader()
{
    stopLoading();
}

void WebCoreAVCFResourceLoader::startLoading()
{
    if (m_resource || !m_parent)
        return;

    RetainPtr<CFURLRequestRef> urlRequest = AVCFAssetResourceLoadingRequestGetURLRequest(m_avRequest.get());
    URL requestURL = CFURLRequestGetURL(urlRequest.get());

    CachedResourceRequest request(ResourceRequest(requestURL), ResourceLoaderOptions(SendCallbacks, DoNotSniffContent, BufferData, DoNotAllowStoredCredentials, DoNotAskClientForCrossOriginCredentials, DoSecurityCheck, UseDefaultOriginRestrictionsForType));

    request.mutableResourceRequest().setPriority(ResourceLoadPriorityLow);
    CachedResourceLoader* loader = m_parent->player()->cachedResourceLoader();
    m_resource = loader ? loader->requestRawResource(request) : 0;
    if (m_resource)
        m_resource->addClient(this);
    else {
        LOG_ERROR("Failed to start load for media at url %s", requestURL.string().ascii().data());
        AVCFAssetResourceLoadingRequestFinishLoadingWithError(m_avRequest.get(), nullptr);
    }
}

void WebCoreAVCFResourceLoader::stopLoading()
{
    if (!m_resource)
        return;

    m_resource->removeClient(this);
    m_resource = 0;

    if (m_parent)
        m_parent->didStopLoadingRequest(m_avRequest.get());
}

void WebCoreAVCFResourceLoader::invalidate()
{
    m_parent = nullptr;
    stopLoading();
}

void WebCoreAVCFResourceLoader::responseReceived(CachedResource* resource, const ResourceResponse& response)
{
    ASSERT(resource == m_resource);
    UNUSED_PARAM(resource);

    int status = response.httpStatusCode();
    if (status && (status < 200 || status > 299)) {
        AVCFAssetResourceLoadingRequestFinishLoadingWithError(m_avRequest.get(), nullptr);
        return;
    }

    notImplemented();
}

void WebCoreAVCFResourceLoader::dataReceived(CachedResource* resource, const char*, int)
{
    fulfillRequestWithResource(resource);
}

void WebCoreAVCFResourceLoader::notifyFinished(CachedResource* resource)
{
    if (resource->loadFailedOrCanceled()) {
        // <rdar://problem/13987417> Set the contentType of the contentInformationRequest to an empty
        // string to trigger AVAsset's playable value to complete loading.
        // FIXME: if ([m_avRequest.get() contentInformationRequest] && ![[m_avRequest.get() contentInformationRequest] contentType])
        // FIXME:    [[m_avRequest.get() contentInformationRequest] setContentType:@""];
        notImplemented();

        AVCFAssetResourceLoadingRequestFinishLoadingWithError(m_avRequest.get(), nullptr);
    } else {
        fulfillRequestWithResource(resource);
        // FIXME: [m_avRequest.get() finishLoading];
        notImplemented();
    }
    stopLoading();
}

void WebCoreAVCFResourceLoader::fulfillRequestWithResource(CachedResource* resource)
{
    ASSERT(resource == m_resource);
    notImplemented();
}

}

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)
