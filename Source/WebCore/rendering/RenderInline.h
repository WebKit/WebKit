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

#include "InlineFlowBox.h"
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

    InlineFlowBox* createAndAppendInlineFlowBox();

    void dirtyLineBoxes(bool fullLayout);
    void deleteLines();

    RenderLineBoxList& lineBoxes() { return m_lineBoxes; }
    const RenderLineBoxList& lineBoxes() const { return m_lineBoxes; }

    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }
    InlineBox* firstLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? firstLineBox() : culledInlineFirstLineBox(); }
    InlineBox* lastLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? lastLineBox() : culledInlineLastLineBox(); }

#if PLATFORM(IOS_FAMILY)
    void absoluteQuadsForSelection(Vector<FloatQuad>& quads) const override;
#endif

    void updateDragState(bool dragOn) final;
    
    LayoutSize offsetForInFlowPositionedInline(const RenderBox* child) const;

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) final;
    void paintOutline(PaintInfo&, const LayoutPoint&);

    bool alwaysCreateLineBoxes() const { return renderInlineAlwaysCreatesLineBoxes(); }
    void setAlwaysCreateLineBoxes() { setRenderInlineAlwaysCreatesLineBoxes(true); }
    void updateAlwaysCreateLineBoxes(bool fullLayout);

    LayoutRect localCaretRect(InlineBox*, unsigned, LayoutUnit* extraWidthToEndOfLine) final;

    bool hitTestCulledInline(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset);

protected:
    void willBeDestroyed() override;

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void updateFromStyle() override;

private:
    const char* renderName() const override;

    bool canHaveChildren() const final { return true; }

    LayoutRect culledInlineVisualOverflowBoundingBox() const;
    InlineBox* culledInlineFirstLineBox() const;
    InlineBox* culledInlineLastLineBox() const;

    template<typename GeneratorContext>
    void generateLineBoxRects(GeneratorContext& yield) const;
    template<typename GeneratorContext>
    void generateCulledLineBoxRects(GeneratorContext& yield, const RenderInline* container) const;

    void layout() final { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    void paint(PaintInfo&, const LayoutPoint&) final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) final;

    bool requiresLayer() const override { return isInFlowPositioned() || createsGroup() || hasClipPath() || willChangeCreatesStackingContext() || hasRunningAcceleratedAnimations(); }

    LayoutUnit offsetLeft() const final;
    LayoutUnit offsetTop() const final;
    LayoutUnit offsetWidth() const final { return linesBoundingBox().width(); }
    LayoutUnit offsetHeight() const final { return linesBoundingBox().height(); }

    LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override;
    LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const final;

    std::optional<LayoutRect> computeVisibleRectInContainer(const LayoutRect&, const RenderLayerModelObject* container, VisibleRectContext) const final;
    LayoutRect computeVisibleRectUsingPaintOffset(const LayoutRect&) const;

    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const override;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderFragmentContainer*) final;

    LayoutRect frameRectForStickyPositioning() const final { return linesBoundingBox(); }

    virtual std::unique_ptr<InlineFlowBox> createInlineFlowBox(); // Subclassed by RenderSVGInline

    void dirtyLinesFromChangedChild(RenderObject& child) final { m_lineBoxes.dirtyLinesFromChangedChild(*this, child); }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const final;
    
    void updateHitTestResult(HitTestResult&, const LayoutPoint&) final;

    void imageChanged(WrappedImagePtr, const IntRect* = 0) final;

#if ENABLE(DASHBOARD_SUPPORT)
    void addAnnotatedRegions(Vector<AnnotatedRegionValue>&) final;
#endif

    void paintOutlineForLine(GraphicsContext&, const LayoutPoint&, const LayoutRect& prevLine, const LayoutRect& thisLine, const LayoutRect& nextLine, const Color&);

    bool willChangeCreatesStackingContext() const
    {
        return style().willChange() && style().willChange()->canCreateStackingContext();
    }

    RenderLineBoxList m_lineBoxes;   // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderInline, isRenderInline())
