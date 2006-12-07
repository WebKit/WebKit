/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "IconDatabase.h"
#include "Logging.h"
#include "ResourceHandle.h"
#include "ResourceResponse.h"
#include "ResourceRequest.h"
#include "SubresourceLoader.h"

using namespace std;

namespace WebCore {

const size_t defaultBufferSize = 4096; // bigger than most icons

IconLoader::IconLoader(Frame* frame)
    : m_frame(frame)
    , m_loadIsInProgress(false)
{
}

auto_ptr<IconLoader> IconLoader::create(Frame* frame)
{
    return auto_ptr<IconLoader>(new IconLoader(frame));
}

IconLoader::~IconLoader()
{
}

void IconLoader::startLoading()
{    
    if (m_resourceLoader)
        return;

    // FIXME: http://bugs.webkit.org/show_bug.cgi?id=10902
    // Once ResourceHandle will load without a DocLoader, we can remove this check.
    // A frame may be documentless - one example is a frame containing only a PDF.
    if (!m_frame->document()) {
        LOG(IconDatabase, "Documentless-frame - icon won't be loaded");
        return;
    }

    // Set flag so we can detect the case where the load completes before
    // SubresourceLoader::create returns.
    m_loadIsInProgress = true;
    m_buffer.reserveCapacity(defaultBufferSize);

    RefPtr<SubresourceLoader> loader = SubresourceLoader::create(m_frame, this, m_frame->loader()->iconURL());
    if (!loader)
        LOG_ERROR("Failed to start load for icon at url %s", m_frame->loader()->iconURL().url().ascii());

    // Store the handle so we can cancel the load if stopLoading is called later.
    // But only do it if the load hasn't already completed.
    if (m_loadIsInProgress)
        m_resourceLoader = loader.release();
}

void IconLoader::stopLoading()
{
    m_resourceLoader = 0;
    clearLoadingState();
}

void IconLoader::didReceiveResponse(SubresourceLoader* resourceLoader, const ResourceResponse& response)
{
    // If we got a status code indicating an invalid response, then lets
    // ignore the data and not try to decode the error page as an icon.
    int status = response.httpStatusCode();
    if (status && (status < 200 || status > 299)) {
        KURL iconURL = resourceLoader->handle()->url();
        m_resourceLoader = 0;
        finishLoading(iconURL);
    }
}

void IconLoader::didReceiveData(SubresourceLoader*, const char* data, int size)
{
    ASSERT(data || size == 0);
    ASSERT(size >= 0);
    m_buffer.append(data, size);
}

void IconLoader::didFail(SubresourceLoader* resourceLoader, const ResourceError&)
{
    ASSERT(m_loadIsInProgress);
    m_buffer.clear();
    finishLoading(resourceLoader->handle()->url());
}

void IconLoader::didFinishLoading(SubresourceLoader* resourceLoader)
{
    // If the icon load resulted in an error-response earlier, the ResourceHandle was killed and icon data commited via finishLoading().
    // In that case this didFinishLoading callback is pointless and we bail.  Otherwise, finishLoading() as expected
    if (m_loadIsInProgress) {
        ASSERT(resourceLoader == m_resourceLoader);
        finishLoading(resourceLoader->handle()->url());
    }
}

void IconLoader::finishLoading(const KURL& iconURL)
{
    IconDatabase::sharedIconDatabase()->setIconDataForIconURL(m_buffer.data(), m_buffer.size(), iconURL.url());
    m_frame->loader()->commitIconURLToIconDatabase(iconURL);
    m_frame->loader()->client()->dispatchDidReceiveIcon();
    clearLoadingState();
}

void IconLoader::clearLoadingState()
{
    m_resourceLoader = 0;
    m_buffer.clear();
    m_loadIsInProgress = false;
}

}
