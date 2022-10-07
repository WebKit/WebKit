/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "LegacyInlineFlowBox.h"
#include "RenderBoxModelObject.h"
#include "RenderLineBoxList.h"

namespace WebCore {

class Position;
class RenderFragmentContainer;

class RenderInline : public RenderBoxModelObject {
    WTF_MAKE_ISO_ALLOCATED(RenderInline);
public:
    RenderInline(Element&, RenderStyle&&);
    RenderInline(Document&, RenderStyle&&);

    LayoutUnit marginLeft() const final;
    LayoutUnit marginRight() const final;
    LayoutUnit marginTop() const final;
    LayoutUnit marginBottom() const final;
    LayoutUnit marginBefore(const RenderStyle* otherStyle = 0) const final;
    LayoutUnit marginAfter(const RenderStyle* otherStyle = 0) const final;
    LayoutUnit marginStart(const RenderStyle* otherStyle = 0) const final;
    LayoutUnit marginEnd(const RenderStyle* otherStyle = 0) const final;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const final;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    LayoutSize offsetFromContainer(RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const final;

    LayoutRect borderBoundingBox() const final
    {
        return LayoutRect(LayoutPoint(), linesBoundingBox().size());
    }

    WEBCORE_EXPORT IntRect linesBoundingBox() const;
    LayoutRect linesVisualOverflowBoundingBox() const;
    LayoutRect linesVisualOverflowBoundingBoxInFragment(const RenderFragmentContainer*) const;

    LegacyInlineFlowBox* createAndAppendInlineFlowBox();

    void dirtyLineBoxes(bool fullLayout);
    void deleteLines();

    RenderLineBoxList& lineBoxes() { return m_lineBoxes; }
    const RenderLineBoxList& lineBoxes() const { return m_lineBoxes; }

    LegacyInlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    LegacyInlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }

#if PLATFORM(IOS_FAMILY)
    void absoluteQuadsForSelection(Vector<FloatQuad>& quads) const override;
#endif
    
    LayoutSize offsetForInFlowPositionedInline(const RenderBox* child) const;

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) const final;
    void paintOutline(PaintInfo&, const LayoutPoint&);

    bool mayAffectLayout() const;

    bool requiresLayer() const override { return isInFlowPositioned() || createsGroup() || hasClipPath() || shouldApplyPaintContainment() || willChangeCreatesStackingContext() || hasRunningAcceleratedAnimations(); }

protected:
    void willBeDestroyed() override;

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void updateFromStyle() override;

private:
    ASCIILiteral renderName() const override;

    bool canHaveChildren() const final { return true; }

    void absoluteQuadsIgnoringContinuation(const FloatRect&, Vector<FloatQuad>&, bool* wasFixed) const override;

    template<typename GeneratorContext>
    void generateLineBoxRects(GeneratorContext& yield) const;

    void layout() final { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    void paint(PaintInfo&, const LayoutPoint&) final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    LayoutUnit offsetLeft() const final;
    LayoutUnit offsetTop() const final;
    LayoutUnit offsetWidth() const final { return linesBoundingBox().width(); }
    LayoutUnit offsetHeight() const final { return linesBoundingBox().height(); }
    LayoutPoint firstInlineBoxTopLeft() const;

protected:
    LayoutRect clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext) const override;
    LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const final;

    std::optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect&, const RenderLayerModelObject* container, VisibleRectContext) const final;
    LayoutRect computeVisibleRectUsingPaintOffset(const LayoutRect&) const;

    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, OptionSet<MapCoordinatesMode>, bool* wasFixed) const override;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;

private:
    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) final;

    LayoutRect frameRectForStickyPositioning() const final { return linesBoundingBox(); }

    virtual std::unique_ptr<LegacyInlineFlowBox> createInlineFlowBox(); // Subclassed by RenderSVGInline

    void dirtyLinesFromChangedChild(RenderObject& child) final { m_lineBoxes.dirtyLinesFromChangedChild(*this, child); }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    LayoutUnit baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    
    void updateHitTestResult(HitTestResult&, const LayoutPoint&) final;

    void imageChanged(WrappedImagePtr, const IntRect* = 0) final;

    bool willChangeCreatesStackingContext() const
    {
        return style().willChange() && style().willChange()->canCreateStackingContext();
    }

    RenderLineBoxList m_lineBoxes;   // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.
};

bool isEmptyInline(const RenderInline&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderInline, isRenderInline())
