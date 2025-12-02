/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ReferenceFilterOperation.h"

#include "CachedResourceLoader.h"
#include "CachedSVGDocumentReference.h"
#include "SVGURIReference.h"

namespace WebCore {
namespace Style {

ReferenceFilterOperation::ReferenceFilterOperation(URL&& url, AtomString&& fragment)
    : FilterOperation(Type::Reference)
    , m_url(WTFMove(url))
    , m_fragment(WTFMove(fragment))
{
}

ReferenceFilterOperation::~ReferenceFilterOperation() = default;

bool ReferenceFilterOperation::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;

    return m_url == downcast<ReferenceFilterOperation>(operation).m_url;
}

bool ReferenceFilterOperation::isIdentity() const
{
    // Answering this question requires access to the renderer and the referenced filterElement.
    ASSERT_NOT_REACHED();
    return false;
}

IntOutsets ReferenceFilterOperation::outsets() const
{
    // Answering this question requires access to the renderer and the referenced filterElement.
    ASSERT_NOT_REACHED();
    return { };
}

void ReferenceFilterOperation::loadExternalDocumentIfNeeded(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    if (m_cachedSVGDocumentReference)
        return;
    if (!SVGURIReference::isExternalURIReference(m_url.resolved.string(), *cachedResourceLoader.protectedDocument()))
        return;
    lazyInitialize(m_cachedSVGDocumentReference, CachedSVGDocumentReference::create(m_url));
    m_cachedSVGDocumentReference->load(cachedResourceLoader, options);
}

} // Style
} // WebCore
