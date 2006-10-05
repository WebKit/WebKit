/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
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
#include "FrameQtClient.h"

#include "FrameQt.h"
#include "ResourceLoader.h"
#include "ResourceLoaderInternal.h"

namespace WebCore {

FrameQtClientDefault::FrameQtClientDefault()
    : FrameQtClient()
    , ResourceLoaderClient()
    , m_frame(0)
    , m_beginCalled(false)
{
}

FrameQtClientDefault::~FrameQtClientDefault()
{
}

void FrameQtClientDefault::setFrame(const FrameQt* frame)
{
    Q_ASSERT(frame != 0);
    m_frame = const_cast<FrameQt*>(frame);
}

void FrameQtClientDefault::openURL(const KURL& url)
{
    m_frame->didOpenURL(url);
    m_beginCalled = false;

    RefPtr<ResourceLoader> loader = ResourceLoader::create(this, "GET", url);
    loader->start(0);
}

void FrameQtClientDefault::submitForm(const String& method, const KURL& url, const FormData* postData)
{
    m_beginCalled = false;

    RefPtr<ResourceLoader> loader = ResourceLoader::create(this, method, url, *postData);
    loader->start(0);
}

void FrameQtClientDefault::receivedResponse(ResourceLoader*, PlatformResponse)
{
    // no-op
}

void FrameQtClientDefault::receivedData(ResourceLoader* job, const char* data, int length)
{
    if (!m_beginCalled) {
        m_beginCalled = true;

        // Assign correct mimetype _before_ calling begin()!
        ResourceLoaderInternal* d = job->getInternal();
        if (d) {
            ResourceRequest request(m_frame->resourceRequest());
            request.m_responseMIMEType = d->m_mimetype;
            m_frame->setResourceRequest(request);
        }

        m_frame->begin(job->url());
    }

    m_frame->write(data, length);
}

void FrameQtClientDefault::receivedAllData(ResourceLoader* job, PlatformData data)
{
    m_frame->end();
    m_beginCalled = false;
}

}
