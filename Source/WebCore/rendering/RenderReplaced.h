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
    WTF_MAKE_TZONE_ALLOCATED(RenderReplaced);
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

    bool isContentLikelyVisibleInViewport();
    bool shouldInvalidateContentWidths() const override;

    virtual bool paintsContent() const { return true; }

    LayoutUnit computeReplacedLogicalHeightUsing(const Style::PreferredSize& logicalHeight) const;
    LayoutUnit computeReplacedLogicalHeightUsing(const Style::MinimumSize& logicalHeight) const;
    LayoutUnit computeReplacedLogicalHeightUsing(const Style::MaximumSize& logicalHeight) const;

    virtual LayoutUnit computeReplacedLogicalWidth(IsComputingIntrinsicSize  = IsComputingIntrinsicSize::No) const;
    virtual LayoutUnit computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth = std::nullopt) const;

    bool replacedMinLogicalHeightComputesAsNone() const;
    bool replacedMaxLogicalHeightComputesAsNone() const;

    virtual RenderReplaced* embeddedSVGRoot() const { return nullptr; }

    std::optional<double> preferredAspectRatio() const override;
    FloatSize preferredAspectRatioAsSize() const override;

protected:
    RenderReplaced(Type, Element&, Style::ComputedStyle&&, OptionSet<ReplacedFlag> = { });
    RenderReplaced(Type, Element&, Style::ComputedStyle&&, const LayoutSize& intrinsicSize, OptionSet<ReplacedFlag> = { });
    RenderReplaced(Type, Document&, Style::ComputedStyle&&, const LayoutSize& intrinsicSize, OptionSet<ReplacedFlag> = { });

    void layout() override;

    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicLogicalWidths() const final;
    std::pair<LayoutUnit, LayoutUnit> computeIntrinsicKeywordLogicalWidths() const final;
    std::pair<LayoutUnit, LayoutUnit> computeAspectRatioAdjustedIntrinsicLogicalWidths() const;

    virtual LayoutUnit minimumReplacedHeight() const { return 0_lu; }

    bool NODELETE isSelected() const;

    void styleDidChange(Style::Difference, const Style::ComputedStyle* oldStyle) override;

    void setIntrinsicSize(const LayoutSize& intrinsicSize) { m_intrinsicSize = intrinsicSize; }
    virtual void intrinsicSizeChanged();
    virtual bool hasRelativeIntrinsicLogicalWidth() const { return false; }

    void paint(PaintInfo&, const LayoutPoint&) override;
    bool NODELETE shouldPaint(PaintInfo&, const LayoutPoint&);
    LayoutRect NODELETE localSelectionRect(bool checkWhetherSelected = true) const; // This is in local coordinates, but it's a physical rect (so the top left corner is physical top left).

    void willBeDestroyed() override;

    virtual void layoutShadowContent(const LayoutSize&);

    LayoutUnit computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit logicalWidth, IsComputingIntrinsicSize = IsComputingIntrinsicSize::No) const;
    template<typename T> LayoutUnit computeReplacedLogicalWidthRespectingMinMaxWidth(T logicalWidth, IsComputingIntrinsicSize isComputingIntrinsicSize = IsComputingIntrinsicSize::No) const { return computeReplacedLogicalWidthRespectingMinMaxWidth(LayoutUnit(logicalWidth), isComputingIntrinsicSize); }

    LayoutUnit computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit logicalHeight) const;
    template<typename T> LayoutUnit computeReplacedLogicalHeightRespectingMinMaxHeight(T logicalHeight) const { return computeReplacedLogicalHeightRespectingMinMaxHeight(LayoutUnit(logicalHeight)); }

private:
    LayoutUnit computeConstrainedLogicalWidth() const;

    template<typename SizeType> LayoutUnit computeReplacedLogicalWidthUsing(const SizeType& logicalWidth) const;
    template<typename SizeType> LayoutUnit computeReplacedLogicalHeightUsingGeneric(const SizeType& logicalHeight) const;
    bool replacedMinMaxLogicalHeightComputesAsNone(const auto& logicalHeight, const auto& initialLogicalHeight) const;


    ASCIILiteral renderName() const override { return "RenderReplaced"_s; }

    bool canHaveChildren() const override { return false; }

    void computeIntrinsicLogicalWidthContributions() final;
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) { }

    RepaintRects localRectsForRepaint(RepaintOutlineBounds) const override;

    PositionWithAffinity positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*) final;

    bool canBeSelectionLeaf() const override { return true; }

    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent = true) final;
    void computeIntrinsicSizesConstrainedByTransferredMinMaxSizes(FloatSize& constrainedSize, FloatSize& intrinsicRatio) const;

    virtual bool shouldDrawSelectionTint() const;
    
    Color calculateHighlightColor() const;
    bool NODELETE isHighlighted(HighlightState, const RenderHighlight&) const;

    bool hasReplacedLogicalHeight() const;

    mutable LayoutSize m_intrinsicSize;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderReplaced, isRenderReplaced())
