/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#ifndef RenderNamedFlowFragment_h
#define RenderNamedFlowFragment_h

#include "RenderRegion.h"

namespace WebCore {

class Element;
class RenderStyle;

// RenderNamedFlowFragment represents a region that is responsible for the fragmentation of
// the RenderNamedFlowThread content.
//
// A RenderNamedFlowFragment object is created as an anonymous child for a RenderBlockFlow object
// that has a valid -webkit-flow-from property.
//
// This allows a non-replaced block to behave like a region if needed, following the CSSRegions specification:
// http://dev.w3.org/csswg/css-regions/#the-flow-from-property.
// list-item, table-caption, table-cell can become regions in addition to block | inline-block.

class RenderNamedFlowFragment FINAL : public RenderRegion {
public:
    RenderNamedFlowFragment(Document&, PassRef<RenderStyle>);
    virtual ~RenderNamedFlowFragment();

    static PassRef<RenderStyle> createStyle(const RenderStyle& parentStyle);

    virtual bool isRenderNamedFlowFragment() const OVERRIDE FINAL { return true; }
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) OVERRIDE;

    virtual LayoutUnit maxPageLogicalHeight() const;

    bool isPseudoElementRegion() const { return parent() && parent()->isPseudoElement(); }

    // When the content inside the region requires the region to have a layer, the layer will be created on the region's
    // parent renderer instead.
    // This method returns that renderer holding the layer.
    // The return value may be null.
    RenderLayerModelObject* layerOwner() const { return parent() && parent()->isRenderLayerModelObject() ?
        toRenderLayerModelObject(parent()) : nullptr; }

private:
    virtual bool shouldHaveAutoLogicalHeight() const OVERRIDE;
    virtual const char* renderName() const OVERRIDE { return "RenderNamedFlowFragment"; }
};

RENDER_OBJECT_TYPE_CASTS(RenderNamedFlowFragment, isRenderNamedFlowFragment())

} // namespace WebCore

#endif // RenderNamedFlowFragment_h
