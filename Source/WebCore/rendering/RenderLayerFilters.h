/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "CachedResourceHandle.h"
#include "CachedSVGDocumentClient.h"
#include "RenderFilterResource.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class CachedSVGDocument;

class RenderLayerFilters final : private CachedSVGDocumentClient, public RenderFilterResource {
    WTF_MAKE_TZONE_ALLOCATED(RenderLayerFilters);
public:
    RenderLayerFilters() = default;

    static bool isIdentity(RenderElement&);
    static IntOutsets calculateOutsets(RenderElement&, const FloatRect& targetBoundingBox);

    SwitcherState beginDrawSourceImage(RenderElement&, const FloatRect& targetBoundingBox, const FloatRect& clipRect, GraphicsContext*&);
    SwitcherState endDrawSourceImage(RenderElement&, GraphicsContext*&);

    void addReferenceFilterClient(RenderLayer&);
    void removeReferenceFilterClient(RenderLayer&);

private:
    std::optional<std::tuple<Ref<Filter>, FloatRect>> createFilter(RenderElement&, const FloatRect& targetBoundingBox, GraphicsContext&) final;

    Vector<RefPtr<Element>> m_internalSVGReferences;
    Vector<CachedResourceHandle<CachedSVGDocument>> m_externalSVGReferences;
};

} // namespace WebCore
