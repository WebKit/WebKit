/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "RenderTreeBuilder.h"

namespace WebCore {

class LegacyRenderSVGContainer;
class LegacyRenderSVGRoot;
class RenderSVGContainer;
class RenderSVGViewportContainer;
class RenderSVGInline;
class RenderSVGRoot;
class RenderSVGText;

class RenderTreeBuilder::SVG {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVG(RenderTreeBuilder&);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    void updateAfterDescendants(RenderSVGRoot&);

    void attach(RenderSVGRoot& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    void attach(RenderSVGContainer& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
#endif
    void attach(LegacyRenderSVGRoot& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    void attach(LegacyRenderSVGContainer& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);
    void attach(RenderSVGInline& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);

    void attach(RenderSVGText& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    RenderPtr<RenderObject> detach(RenderSVGRoot& parent, RenderObject& child) WARN_UNUSED_RETURN;
    RenderPtr<RenderObject> detach(RenderSVGContainer& parent, RenderObject& child) WARN_UNUSED_RETURN;
#endif
    RenderPtr<RenderObject> detach(LegacyRenderSVGRoot& parent, RenderObject& child) WARN_UNUSED_RETURN;
    RenderPtr<RenderObject> detach(LegacyRenderSVGContainer& parent, RenderObject& child) WARN_UNUSED_RETURN;
    RenderPtr<RenderObject> detach(RenderSVGInline& parent, RenderObject& child) WARN_UNUSED_RETURN;

    RenderPtr<RenderObject> detach(RenderSVGText& parent, RenderObject& child) WARN_UNUSED_RETURN;

private:
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    RenderSVGViewportContainer& findOrCreateParentForChild(RenderSVGRoot&);
    RenderSVGViewportContainer& createViewportContainer(RenderSVGRoot&);
#endif
    RenderTreeBuilder& m_builder;
};

}
