/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <WebCore/RenderBox.h>

namespace WebCore {

class RenderReplaced : public RenderBox {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RenderReplaced);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderReplaced);
public:
    virtual ~RenderReplaced();

    virtual bool shouldRespectZeroIntrinsicWidth() const;

    void computeReplacedOutOfFlowPositionedLogicalHeight(LogicalExtentComputedValues&) const;
    void computeReplacedOutOfFlowPositionedLogicalWidth(LogicalExtentComputedValues&) const;

    LayoutRect replacedContentRect(const LayoutSize& intrinsicSize) const;
    LayoutRect replacedContentRect() const { return replacedContentRect(intrinsicSize()); }

    bool setNeedsLayoutIfNeededAfterIntrinsicSizeChange();

    LayoutSize intrinsicSize() const final;
    FloatSize intrinsicRatio() const;
    
    bool isContentLikelyVisibleInViewport();
    bool shouldInvalidatePreferredWidths() const override;

    double computeIntrinsicAspectRatio() const;

    virtual bool paintsContent() const { return true; }

    LayoutUnit computeReplacedLogicalHeightUsing(const Style::PreferredSize& logicalHeight) const;
    LayoutUnit computeReplacedLogicalHeightUsing(const Style::MinimumSize& logicalHeight) const;
    LayoutUnit computeReplacedLogicalHeightUsing(const Style::MaximumSize& logicalHeight) const;

    virtual LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred  = ShouldComputePreferred::ComputeActual) const;
    virtual LayoutUnit computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth = std::nullopt) const;

    bool replacedMinLogicalHeightComputesAsNone() const;
    bool replacedMaxLogicalHeightComputesAsNone() const;

protected:
    RenderReplaced(Type, Element&, RenderStyle&&, OptionSet<ReplacedFlag> = { });
    RenderReplaced(Type, Element&, RenderStyle&&, const LayoutSize& intrinsicSize, OptionSet<ReplacedFlag> = { });
    RenderReplaced(Type, Document&, RenderStyle&&, const LayoutSize& intrinsicSize, OptionSet<ReplacedFlag> = { });

    void layout() override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const final;

    virtual LayoutUnit minimumReplacedHeight() const { return 0_lu; }

    bool isSelected() const;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    virtual FloatSize computeIntrinsicSize() const;
    virtual FloatSize preferredAspectRatio() const;

    void setIntrinsicSize(const LayoutSize& intrinsicSize) { m_intrinsicSize = intrinsicSize; }
    virtual void intrinsicSizeChanged();
    virtual bool hasRelativeIntrinsicLogicalWidth() const { return false; }

    void paint(PaintInfo&, const LayoutPoint&) override;
    bool shouldPaint(PaintInfo&, const LayoutPoint&);
    LayoutRect localSelectionRect(bool checkWhetherSelected = true) const; // This is in local coordinates, but it's a physical rect (so the top left corner is physical top left).

    void willBeDestroyed() override;

    virtual void layoutShadowContent(const LayoutSize&);

    LayoutUnit computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit logicalWidth, ShouldComputePreferred = ShouldComputePreferred::ComputeActual) const;
    template<typename T> LayoutUnit computeReplacedLogicalWidthRespectingMinMaxWidth(T logicalWidth, ShouldComputePreferred shouldComputePreferred = ShouldComputePreferred::ComputeActual) const { return computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit(logicalWidth), shouldComputePreferred); }

private:
    LayoutUnit computeConstrainedLogicalWidth() const;

    template<typename SizeType> LayoutUnit computeReplacedLogicalWidthUsing(const SizeType& logicalWidth) const;
    template<typename SizeType> LayoutUnit computeReplacedLogicalHeightUsingGeneric(const SizeType& logicalHeight) const;
    LayoutUnit computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit logicalHeight) const;
    template<typename T> LayoutUnit computeReplacedLogicalHeightRespectingMinMaxHeight(T logicalHeight) const { return computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit(logicalHeight)); }
    bool replacedMinMaxLogicalHeightComputesAsNone(const auto& logicalHeight, const auto& initialLogicalHeight) const;

    void computeAspectRatioAdjustedIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const;

    virtual RenderBox* embeddedContentBox() const { return 0; }
    ASCIILiteral renderName() const override { return "RenderReplaced"_s; }

    bool canHaveChildren() const override { return false; }

    void computePreferredLogicalWidths() final;
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) { }

    RepaintRects localRectsForRepaint(RepaintOutlineBounds) const override;

    PositionWithAffinity positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*) final;

    bool canBeSelectionLeaf() const override { return true; }

    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent = true) final;
    void computeAspectRatioInformationForRenderBox(RenderBox*, FloatSize& constrainedSize, FloatSize& intrinsicRatio) const;
    void computeIntrinsicSizesConstrainedByTransferredMinMaxSizes(RenderBox* contentRenderer, FloatSize& constrainedSize, FloatSize& intrinsicRatio) const;

    virtual bool shouldDrawSelectionTint() const;
    
    Color calculateHighlightColor() const;
    bool isHighlighted(HighlightState, const RenderHighlight&) const;

    bool hasReplacedLogicalHeight() const;

    mutable LayoutSize m_intrinsicSize;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderReplaced, isRenderReplaced())
