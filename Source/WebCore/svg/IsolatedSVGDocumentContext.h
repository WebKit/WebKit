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

#pragma once

#include "CachedImageClient.h"
#include "CachedResourceHandle.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CachedImage;
class Document;
class SVGDocument;
class WeakPtrImplWithEventTargetData;

// A thin wrapper around the SVGImage infrastructure. Where SVGImage's public
// contract is pixel output, IsolatedSVGDocumentContext's contract is the laid-out render tree: it exposes
// an SVG document's resources (paint servers, etc.) so they can be referenced from a host document.
class IsolatedSVGDocumentContext final : public CachedImageClient, public RefCounted<IsolatedSVGDocumentContext> {
    WTF_MAKE_TZONE_ALLOCATED(IsolatedSVGDocumentContext);
public:
    static Ref<IsolatedSVGDocumentContext> create(CachedImage&, Document&);
    ~IsolatedSVGDocumentContext();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    SVGDocument* document() const;

private:
    IsolatedSVGDocumentContext(CachedImage&, Document&);

    void imageChanged(CachedImage*, const IntRect*) final;

    const CachedResourceHandle<CachedImage> m_cachedImage;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_owningDocument;
};

} // namespace WebCore
