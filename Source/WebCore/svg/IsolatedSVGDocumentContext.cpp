/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "IsolatedSVGDocumentContext.h"

#include "CachedImage.h"
#include "Document.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Page.h"
#include "SVGDocument.h"
#include "SVGImage.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IsolatedSVGDocumentContext);

Ref<IsolatedSVGDocumentContext> IsolatedSVGDocumentContext::create(CachedImage& cachedImage, Document& document)
{
    Ref result = adoptRef(*new IsolatedSVGDocumentContext(cachedImage, document));
    // Register only after adoptRef(): addClient() may call imageChanged() synchronously (already-loaded
    // resource), which refs `this`.
    cachedImage.addClient(result.get());
    return result;
}

IsolatedSVGDocumentContext::IsolatedSVGDocumentContext(CachedImage& cachedImage, Document& owningDocument)
    : m_cachedImage(&cachedImage)
    , m_owningDocument(owningDocument)
{
}

IsolatedSVGDocumentContext::~IsolatedSVGDocumentContext()
{
    protect(*m_cachedImage)->removeClient(*this);
}

SVGDocument* IsolatedSVGDocumentContext::document() const
{
    RefPtr svgImage = dynamicDowncast<SVGImage>(protect(*m_cachedImage)->image());
    if (!svgImage)
        return nullptr;

    RefPtr page = svgImage->internalPage();
    if (!page)
        return nullptr;

    RefPtr localMainFrame = page->localMainFrame();
    return localMainFrame ? dynamicDowncast<SVGDocument>(localMainFrame->document()) : nullptr;
}

void IsolatedSVGDocumentContext::imageChanged(CachedImage*, const IntRect*)
{
    // The isolated document just finished loading and laying out. Force referencing renderers to
    // rebuild their SVGResources so paint servers re-resolve.
    // FIXME: Track referencing elements and invalidate only those (as RenderLayerFilters does).
    RefPtr owningDocument = m_owningDocument.get();
    if (RefPtr documentElement = owningDocument ? owningDocument->documentElement() : nullptr)
        documentElement->invalidateStyleAndRenderersForSubtree();
}

} // namespace WebCore
