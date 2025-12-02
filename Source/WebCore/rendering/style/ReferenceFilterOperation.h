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

#pragma once

#include "FilterOperation.h"
#include "StyleURL.h"
#include <wtf/text/AtomString.h>

namespace WebCore {

class CachedResourceLoader;
class CachedSVGDocumentReference;
struct ResourceLoaderOptions;

namespace Style {

class ReferenceFilterOperation final : public FilterOperation {
public:
    static Ref<ReferenceFilterOperation> create(URL url, AtomString fragment)
    {
        return adoptRef(*new ReferenceFilterOperation(WTFMove(url), WTFMove(fragment)));
    }
    virtual ~ReferenceFilterOperation();

    Ref<FilterOperation> clone() const final
    {
        // Reference filters cannot be cloned.
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool affectsOpacity() const override { return true; }
    bool movesPixels() const override { return true; }
    // FIXME: This only needs to return true for graphs that include ConvolveMatrix, DisplacementMap, Morphology and possibly Lighting.
    // https://bugs.webkit.org/show_bug.cgi?id=171753
    bool shouldBeRestrictedBySecurityOrigin() const override { return true; }

    const URL& url() const { return m_url; }
    const AtomString& fragment() const { return m_fragment; }

    void loadExternalDocumentIfNeeded(CachedResourceLoader&, const ResourceLoaderOptions&);

    CachedSVGDocumentReference* cachedSVGDocumentReference() const { return m_cachedSVGDocumentReference.get(); }

private:
    ReferenceFilterOperation(URL&&, AtomString&&);

    bool operator==(const FilterOperation&) const override;

    bool isIdentity() const override;
    IntOutsets outsets() const override;

    URL m_url;
    AtomString m_fragment;

    const RefPtr<CachedSVGDocumentReference> m_cachedSVGDocumentReference;
};

} // Style
} // WebCore

SPECIALIZE_TYPE_TRAITS_FILTEROPERATION(Style::ReferenceFilterOperation, type() == WebCore::FilterOperation::Type::Reference)
