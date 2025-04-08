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

#include "CachedResourceHandle.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CachedResourceRequestInitiatorTypes.h"
#include "CachedSVGDocument.h"

namespace WebCore {

CachedSVGDocumentReference::CachedSVGDocumentReference(const Style::URL& location)
    : m_location { location }
{
}

CachedSVGDocumentReference::~CachedSVGDocumentReference()
{
    if (CachedResourceHandle document = m_document)
        document->removeClient(*this);
}

void CachedSVGDocumentReference::load(CachedResourceLoader& loader, const ResourceLoaderOptions& options)
{
    if (m_loadRequested)
        return;

    auto fetchOptions = options;

    CSS::applyModifiersToLoaderOptions(m_location.modifiers, fetchOptions);

    // FIXME: CSS::applyModifiersToLoaderOptions will set `fetchOptions.mode` to `FetchOptions::Mode::Cors` if `modifiers.crossorigin` is set. This will immediately be undone here. Which should win?

    fetchOptions.mode = FetchOptions::Mode::SameOrigin;

    CachedResourceRequest request(ResourceRequest(m_location.resolved), fetchOptions);
    request.setInitiatorType(cachedResourceRequestInitiatorTypes().css);

    m_document = loader.requestSVGDocument(WTFMove(request)).value_or(nullptr);

    if (CachedResourceHandle document = m_document)
        document->addClient(*this);

    m_loadRequested = true;
}

} // namespace WebCore
