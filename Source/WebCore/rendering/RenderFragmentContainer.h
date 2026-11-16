/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
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

#include "LayerFragment.h"
#include "RenderBlockFlow.h"
#include "RenderFragmentedFlow.h"
#include "VisiblePosition.h"
#include <memory>
#include <wtf/WeakHashMap.h>

namespace WebCore {

class Element;
class RenderBox;
class RenderBoxFragmentInfo;
class RenderFragmentedFlow;

class RenderFragmentContainer : public RenderBlockFlow {
    WTF_MAKE_TZONE_ALLOCATED(RenderFragmentContainer);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderFragmentContainer);
public:
    void setFragmentedFlowPortionRect(const LayoutRect& rect) { m_fragmentedFlowPortionRect = rect; }
    LayoutRect fragmentedFlowPortionRect() const { return m_fragmentedFlowPortionRect; }
    LayoutRect fragmentedFlowPortionOverflowRect() const;

    void attachFragment();
    void detachFragment();

    RenderFragmentedFlow* fragmentedFlow() const { return m_fragmentedFlow.get(); }

    // Set once attachFragment() has added the fragment to its fragmented flow. It stays set after detachFragment().
    bool isValid() const { return m_isValid; }

    NODELETE RenderBoxFragmentInfo* renderBoxFragmentInfo(const RenderBox&) const;
    void setRenderBoxFragmentInfo(const RenderBox&, LayoutUnit logicalLeft, LayoutUnit logicalWidth);
    std::unique_ptr<RenderBoxFragmentInfo> takeRenderBoxFragmentInfo(const RenderBox&);
    void removeRenderBoxFragmentInfo(const RenderBox&);

    void deleteAllRenderBoxFragmentInfo();

    bool isFirstFragment() const;
    bool isLastFragment() const;

    // The width and height of a single column or page in the set.
    virtual LayoutUnit pageLogicalWidth() const = 0;
    virtual LayoutUnit pageLogicalHeight() const = 0;

    NODELETE LayoutUnit logicalTopOfFragmentedFlowContentRect(const LayoutRect&) const;
    NODELETE LayoutUnit logicalBottomOfFragmentedFlowContentRect(const LayoutRect&) const;
    LayoutUnit logicalTopForFragmentedFlowContent() const { return logicalTopOfFragmentedFlowContentRect(fragmentedFlowPortionRect()); };
    LayoutUnit logicalBottomForFragmentedFlowContent() const { return logicalBottomOfFragmentedFlowContentRect(fragmentedFlowPortionRect()); };

    // The logical height of the entire flow thread portion used by the set, i.e. the height of all the pages
    // or columns added together.
    virtual LayoutUnit logicalHeightOfAllFragmentedFlowContent() const = 0;

    // The top of the nearest column or page inside the set.
    virtual LayoutUnit pageLogicalTopForOffset(LayoutUnit offset) const = 0;

    virtual void repaintFragmentedFlowContent(const LayoutRect& repaintRect) const = 0;

    virtual void collectLayerFragments(LayerFragments&, const LayoutRect&, const LayoutRect&) const = 0;

    void addLayoutOverflowForBox(const RenderBox&, const LayoutRect&);
    void addVisualOverflowForBox(const RenderBox&, const LayoutRect&);
    LayoutRect visualOverflowRectForBox(const RenderBox&) const;
    LayoutRect layoutOverflowRectForBoxForPropagation(const RenderBox&);
    LayoutRect visualOverflowRectForBoxForPropagation(const RenderBox&);

    LayoutRect rectFlowPortionForBox(const RenderBox&, const LayoutRect&) const;

    bool canHaveChildren() const override { return false; }
    bool canHaveGeneratedChildren() const override { return true; }

    virtual Vector<LayoutRect> fragmentRectsForFlowContentRect(const LayoutRect&) const = 0;
    virtual bool contentRectSpansFragments(const LayoutRect&) const = 0;

protected:
    RenderFragmentContainer(Type, Element&, Style::ComputedStyle&&, RenderFragmentedFlow*);
    RenderFragmentContainer(Type, Document&, Style::ComputedStyle&&, RenderFragmentedFlow*);
    virtual ~RenderFragmentContainer();

    RenderOverflow* overflowForBox(const RenderBox&) const;

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidthContributions() override;
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const override;

    LayoutRect overflowRectForFragmentedFlowPortion(const LayoutRect& fragmentedFlowPortionRect, bool isFirstPortion, bool isLastPortion) const;
    void repaintFragmentedFlowContentRectangle(const LayoutRect& repaintRect, const LayoutRect& fragmentedFlowPortionRect, const LayoutPoint& fragmentLocation, const LayoutRect* fragmentedFlowPortionClipRect = 0) const;

    NODELETE LayoutRect fragmentedFlowContentRectangle(const LayoutRect&, const LayoutRect& fragmentedFlowPortionRect, const LayoutPoint& fragmentLocation, const LayoutRect* fragmentedFlowPortionClipRect = 0) const;

private:
    ASCIILiteral renderName() const override { return "RenderFragmentContainer"_s; }

    void insertedIntoTree() override;
    void willBeRemovedFromTree() override;

    LayoutRect computedVisualOverflowRectForBox(const RenderBox&) const;
    LayoutRect computedLayoutOverflowRectForBox(const RenderBox&) const;

protected:
    SingleThreadWeakPtr<RenderFragmentedFlow> m_fragmentedFlow;

private:
    LayoutRect m_fragmentedFlowPortionRect;

    // This map holds unique information about a block that is split across fragments.
    // A RenderBoxFragmentInfo* tells us about any layout information for a RenderBox that
    // is unique to the fragment. For now it just holds logical width information for RenderBlocks, but eventually
    // it will also hold a custom style for any box (for fragment styling).
    using RenderBoxFragmentInfoMap = SingleThreadWeakHashMap<const RenderBox, std::unique_ptr<RenderBoxFragmentInfo>>;
    RenderBoxFragmentInfoMap m_renderBoxFragmentInfo;

    bool m_isValid { false };
};

#ifndef NDEBUG
TextStream& operator<<(TextStream&, const RenderFragmentContainer&);
#endif

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderFragmentContainer, isRenderFragmentContainer())
