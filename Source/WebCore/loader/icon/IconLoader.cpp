/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#include "config.h"
#include "IconLoader.h"

#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiators.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "IconController.h"
#include "IconDatabase.h"
#include "Logging.h"
#include "ResourceRequest.h"
#include "SharedBuffer.h"
#include <wtf/text/CString.h>

namespace WebCore {

IconLoader::IconLoader(Frame& frame)
    : m_frame(&frame)
    , m_url(frame.loader().icon().url())
{
}

IconLoader::IconLoader(DocumentLoader& documentLoader, const URL& url)
    : m_documentLoader(&documentLoader)
    , m_url(url)
{
}

IconLoader::~IconLoader()
{
    stopLoading();
}

void IconLoader::startLoading()
{
    ASSERT(m_frame || m_documentLoader);

    if (m_resource)
        return;

    if (m_frame && !m_frame->document())
        return;


    if (m_documentLoader && !m_documentLoader->frame())
        return;

    ResourceRequest resourceRequest = m_documentLoader ? m_url :  m_frame->loader().icon().url();
    resourceRequest.setPriority(ResourceLoadPriority::Low);
#if !ERROR_DISABLED
    // Copy this because we may want to access it after transferring the
    // `resourceRequest` to the `request`. If we don't, then the LOG_ERROR
    // below won't print a URL.
    auto resourceRequestURL = resourceRequest.url();
#endif

    // ContentSecurityPolicyImposition::DoPolicyCheck is a placeholder value. It does not affect the request since Content Security Policy does not apply to raw resources.
    CachedResourceRequest request(WTFMove(resourceRequest), ResourceLoaderOptions(SendCallbacks, SniffContent, BufferData, DoNotAllowStoredCredentials, ClientCredentialPolicy::CannotAskClientForCredentials, FetchOptions::Credentials::Omit, DoSecurityCheck, FetchOptions::Mode::NoCors, DoNotIncludeCertificateInfo, ContentSecurityPolicyImposition::DoPolicyCheck, DefersLoadingPolicy::AllowDefersLoading, CachingPolicy::AllowCaching));

    request.setInitiator(cachedResourceRequestInitiators().icon);

    auto* frame = m_frame ? m_frame : m_documentLoader->frame();
    m_resource = frame->document()->cachedResourceLoader().requestRawResource(WTFMove(request));
    if (m_resource)
        m_resource->addClient(*this);
    else
        LOG_ERROR("Failed to start load for icon at url %s", resourceRequestURL.string().ascii().data());
}

void IconLoader::stopLoading()
{
    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }
}

void IconLoader::notifyFinished(CachedResource& resource)
{
    ASSERT_UNUSED(resource, &resource == m_resource);

    // If we got a status code indicating an invalid response, then lets
    // ignore the data and not try to decode the error page as an icon.
    auto* data = m_resource->resourceBuffer();
    int status = m_resource->response().httpStatusCode();
    if (status && (status < 200 || status > 299))
        data = nullptr;

    static const char pdfMagicNumber[] = "%PDF";
    static unsigned pdfMagicNumberLength = sizeof(pdfMagicNumber) - 1;
    if (data && data->size() >= pdfMagicNumberLength && !memcmp(data->data(), pdfMagicNumber, pdfMagicNumberLength)) {
        LOG(IconDatabase, "IconLoader::finishLoading() - Ignoring icon at %s because it appears to be a PDF", m_resource->url().string().ascii().data());
        data = nullptr;
    }

    LOG(IconDatabase, "IconLoader::finishLoading() - Committing iconURL %s to database", m_resource->url().string().ascii().data());

    if (!m_frame) {
        // DocumentLoader::finishedLoadingIcon destroys this IconLoader as it finishes. This will automatically
        // trigger IconLoader::stopLoading() during destruction, so we should just return here.
        m_documentLoader->finishedLoadingIcon(*this, data);
        return;
    }

    m_frame->loader().icon().commitToDatabase(m_resource->url());

    // Setting the icon data only after committing to the database ensures that the data is
    // kept in memory (so it does not have to be read from the database asynchronously), since
    // there is a page URL referencing it.
    iconDatabase().setIconDataForIconURL(data, m_resource->url().string());
    m_frame->loader().client().dispatchDidReceiveIcon();

    stopLoading();
}

}
