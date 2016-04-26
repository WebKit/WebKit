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

class RenderNamedFlowFragment final : public RenderRegion {
public:
    RenderNamedFlowFragment(Document&, RenderStyle&&);
    virtual ~RenderNamedFlowFragment();

    static RenderStyle createStyle(const RenderStyle& parentStyle);

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void getRanges(Vector<RefPtr<Range>>&) const;

    LayoutUnit pageLogicalHeight() const override;
    LayoutUnit maxPageLogicalHeight() const;
    
    LayoutRect flowThreadPortionRectForClipping(bool isFirstRegionInRange, bool isLastRegionInRange) const;

    RenderBlockFlow& fragmentContainer() const;
    RenderLayer& fragmentContainerLayer() const;
    
    bool shouldClipFlowThreadContent() const override;
    
    LayoutSize offsetFromContainer(RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const override;

    bool isPseudoElementRegion() const { return parent() && parent()->isPseudoElement(); }

    // When the content inside the region requires the region to have a layer, the layer will be created on the region's
    // parent renderer instead.
    // This method returns that renderer holding the layer.
    // The return value cannot be null because CSS Regions create Stacking Contexts (which means they create layers).
    RenderLayerModelObject& layerOwner() const { return downcast<RenderLayerModelObject>(*parent()); }

    bool hasCustomRegionStyle() const { return m_hasCustomRegionStyle; }
    void clearObjectStyleInRegion(const RenderObject*);

    void setRegionObjectsRegionStyle();
    void restoreRegionObjectsOriginalStyle();
    
    LayoutRect visualOverflowRect() const override;

    RenderNamedFlowThread* namedFlowThread() const;

    bool hasAutoLogicalHeight() const override { return m_hasAutoLogicalHeight; }

    LayoutUnit computedAutoHeight() const { return m_computedAutoHeight; }

    void setComputedAutoHeight(LayoutUnit computedAutoHeight)
    {
        m_hasComputedAutoHeight = true;
        m_computedAutoHeight = computedAutoHeight;
    }

    void clearComputedAutoHeight()
    {
        m_hasComputedAutoHeight = false;
        m_computedAutoHeight = 0;
    }

    bool hasComputedAutoHeight() const { return m_hasComputedAutoHeight; }

    RegionOversetState regionOversetState() const;

    void attachRegion() override;
    void detachRegion() override;

    void updateLogicalHeight() override;

    void updateRegionFlags();

    void absoluteQuadsForBoxInRegion(Vector<FloatQuad>&, bool*, const RenderBox*, float, float) override;

    void invalidateRegionIfNeeded();

private:
    bool isRenderNamedFlowFragment() const override { return true; }
    const char* renderName() const override { return "RenderNamedFlowFragment"; }

    std::unique_ptr<RenderStyle> computeStyleInRegion(RenderElement&, const RenderStyle& parentStyle) const;
    void computeChildrenStyleInRegion(RenderElement&);
    void setObjectStyleInRegion(RenderObject*, std::unique_ptr<RenderStyle>, bool objectRegionStyleCached);

    void checkRegionStyle();
    void setHasCustomRegionStyle(bool hasCustomRegionStyle) { m_hasCustomRegionStyle = hasCustomRegionStyle; }

    void updateRegionHasAutoLogicalHeightFlag();

    void incrementAutoLogicalHeightCount();
    void decrementAutoLogicalHeightCount();

    bool shouldHaveAutoLogicalHeight() const;

    void updateOversetState();
    void setRegionOversetState(RegionOversetState);

    void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight = 0) override;

    struct ObjectRegionStyleInfo {
        // Used to store the original style of the object in region
        // so that the original style is properly restored after paint.
        // Also used to store computed style of the object in region between
        // region paintings, so that the style in region is computed only
        // when necessary.
        std::unique_ptr<RenderStyle> style;
        // True if the computed style in region is cached.
        bool cached;
    };

    typedef HashMap<const RenderObject*, ObjectRegionStyleInfo > RenderObjectRegionStyleMap;
    RenderObjectRegionStyleMap m_renderObjectRegionStyle;

    bool m_hasCustomRegionStyle : 1;
    bool m_hasAutoLogicalHeight : 1;
    bool m_hasComputedAutoHeight : 1;

    LayoutUnit m_computedAutoHeight;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderNamedFlowFragment, isRenderNamedFlowFragment())

#endif // RenderNamedFlowFragment_h
