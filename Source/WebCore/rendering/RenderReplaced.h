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

#include "RenderBox.h"

namespace WebCore {

class RenderReplaced : public RenderBox {
    WTF_MAKE_ISO_ALLOCATED(RenderReplaced);
public:
    virtual ~RenderReplaced();

    LayoutUnit computeReplacedLogicalWidth(ShouldComputePreferred = ComputeActual) const override;
    LayoutUnit computeReplacedLogicalHeight(std::optional<LayoutUnit> estimatedUsedWidth = std::nullopt) const override;

    LayoutRect replacedContentRect(const LayoutSize& intrinsicSize) const;
    LayoutRect replacedContentRect() const { return replacedContentRect(intrinsicSize()); }

    bool hasReplacedLogicalWidth() const;
    bool hasReplacedLogicalHeight() const;
    bool setNeedsLayoutIfNeededAfterIntrinsicSizeChange();

    LayoutSize intrinsicSize() const final
    {
        if (shouldApplySizeContainment(*this))
            return LayoutSize();
        return m_intrinsicSize;
    }
    
    RoundedRect roundedContentBoxRect() const;
    
    bool isContentLikelyVisibleInViewport();
    bool needsPreferredWidthsRecalculation() const override;

protected:
    RenderReplaced(Element&, RenderStyle&&);
    RenderReplaced(Element&, RenderStyle&&, const LayoutSize& intrinsicSize);
    RenderReplaced(Document&, RenderStyle&&, const LayoutSize& intrinsicSize);

    void layout() override;

    void computeIntrinsicRatioInformation(FloatSize& intrinsicSize, double& intrinsicRatio) const override;

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const final;

    virtual LayoutUnit minimumReplacedHeight() const { return 0_lu; }

    bool isSelected() const;

    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void setIntrinsicSize(const LayoutSize& intrinsicSize) { m_intrinsicSize = intrinsicSize; }
    virtual void intrinsicSizeChanged();
    virtual bool hasRelativeIntrinsicLogicalWidth() const { return false; }

    void paint(PaintInfo&, const LayoutPoint&) override;
    bool shouldPaint(PaintInfo&, const LayoutPoint&);
    LayoutRect localSelectionRect(bool checkWhetherSelected = true) const; // This is in local coordinates, but it's a physical rect (so the top left corner is physical top left).

    void willBeDestroyed() override;

private:
    LayoutUnit computeConstrainedLogicalWidth(ShouldComputePreferred) const;

    virtual RenderBox* embeddedContentBox() const { return 0; }
    const char* renderName() const override { return "RenderReplaced"; }

    bool canHaveChildren() const override { return false; }

    void computePreferredLogicalWidths() final;
    virtual void paintReplaced(PaintInfo&, const LayoutPoint&) { }

    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const override;

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) final;
    
    bool canBeSelectionLeaf() const override { return true; }

    LayoutRect selectionRectForRepaint(const RenderLayerModelObject* repaintContainer, bool clipToVisibleContent = true) final;
    void computeAspectRatioInformationForRenderBox(RenderBox*, FloatSize& constrainedSize, double& intrinsicRatio) const;

    virtual bool shouldDrawSelectionTint() const;
    
    Color calculateHighlightColor() const;
    bool isHighlighted(HighlightState, const HighlightData&) const;

    mutable LayoutSize m_intrinsicSize;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderReplaced, isRenderReplaced())
