/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "CachedSVGDocumentReference.h"

#if ENABLE(CSS_FILTERS)
#include "CachedResourceHandle.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiators.h"
#include "CachedSVGDocument.h"

namespace WebCore {

CachedSVGDocumentReference::CachedSVGDocumentReference(const String& url)
    : m_url(url)
    , m_document(0)
    , m_loadRequested(false)
{
}

CachedSVGDocumentReference::~CachedSVGDocumentReference()
{
    if (m_document)
        m_document->removeClient(this);
}

void CachedSVGDocumentReference::load(CachedResourceLoader* loader)
{
    ASSERT(loader);
    if (m_loadRequested)
        return;

    CachedResourceRequest request(ResourceRequest(loader->document()->completeURL(m_url)));
    request.setInitiator(cachedResourceRequestInitiators().css);
    m_document = loader->requestSVGDocument(request);
    if (m_document)
        m_document->addClient(this);

    m_loadRequested = true;
}

}

#endif
