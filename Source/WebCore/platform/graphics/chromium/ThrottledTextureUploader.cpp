/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "ThrottledTextureUploader.h"

#include "Extensions3DChromium.h"
#include <public/WebGraphicsContext3D.h>

namespace {

// Number of pending texture update queries to allow.
static const size_t maxPendingQueries = 2;

} // anonymous namespace

namespace WebCore {

ThrottledTextureUploader::Query::Query(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_queryId(0)
{
    m_queryId = m_context->createQueryEXT();
}

ThrottledTextureUploader::Query::~Query()
{
    m_context->deleteQueryEXT(m_queryId);
}

void ThrottledTextureUploader::Query::begin()
{
    m_context->beginQueryEXT(Extensions3DChromium::COMMANDS_ISSUED_CHROMIUM, m_queryId);
}

void ThrottledTextureUploader::Query::end()
{
    m_context->endQueryEXT(Extensions3DChromium::COMMANDS_ISSUED_CHROMIUM);
}

bool ThrottledTextureUploader::Query::isPending()
{
    unsigned available = 1;
    m_context->getQueryObjectuivEXT(m_queryId, Extensions3DChromium::QUERY_RESULT_AVAILABLE_EXT, &available);
    return !available;
}

void ThrottledTextureUploader::Query::wait()
{
    unsigned result;
    m_context->getQueryObjectuivEXT(m_queryId, Extensions3DChromium::QUERY_RESULT_EXT, &result);
}

ThrottledTextureUploader::ThrottledTextureUploader(WebKit::WebGraphicsContext3D* context)
    : m_context(context)
    , m_maxPendingQueries(maxPendingQueries)
{
}

ThrottledTextureUploader::ThrottledTextureUploader(WebKit::WebGraphicsContext3D* context, size_t pendingUploadLimit)
    : m_context(context)
    , m_maxPendingQueries(pendingUploadLimit)
{
    ASSERT(m_context);
}

ThrottledTextureUploader::~ThrottledTextureUploader()
{
}

bool ThrottledTextureUploader::isBusy()
{
    processQueries();

    if (!m_availableQueries.isEmpty())
        return false;

    if (m_pendingQueries.size() == m_maxPendingQueries)
        return true;

    m_availableQueries.append(Query::create(m_context));
    return false;
}

void ThrottledTextureUploader::beginUploads()
{
    // Wait for query to become available.
    while (isBusy())
        m_pendingQueries.first()->wait();

    ASSERT(!m_availableQueries.isEmpty());
    m_availableQueries.first()->begin();
}

void ThrottledTextureUploader::endUploads()
{
    m_availableQueries.first()->end();
    m_pendingQueries.append(m_availableQueries.takeFirst());
}

void ThrottledTextureUploader::uploadTexture(CCResourceProvider* resourceProvider, Parameters upload)
{
    upload.texture->updateRect(resourceProvider, upload.sourceRect, upload.destOffset);
}

void ThrottledTextureUploader::processQueries()
{
    while (!m_pendingQueries.isEmpty()) {
        if (m_pendingQueries.first()->isPending())
            break;

        m_availableQueries.append(m_pendingQueries.takeFirst());
    }
}

}
