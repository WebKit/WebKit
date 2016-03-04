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

#ifndef RenderInline_h
#define RenderInline_h

#include "InlineFlowBox.h"
#include "RenderBoxModelObject.h"
#include "RenderLineBoxList.h"

namespace WebCore {

class Position;
class RenderRegion;

class RenderInline : public RenderBoxModelObject {
public:
    RenderInline(Element&, Ref<RenderStyle>&&);
    RenderInline(Document&, Ref<RenderStyle>&&);

    void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;

    LayoutUnit marginLeft() const override final;
    LayoutUnit marginRight() const override final;
    LayoutUnit marginTop() const override final;
    LayoutUnit marginBottom() const override final;
    LayoutUnit marginBefore(const RenderStyle* otherStyle = 0) const override final;
    LayoutUnit marginAfter(const RenderStyle* otherStyle = 0) const override final;
    LayoutUnit marginStart(const RenderStyle* otherStyle = 0) const override final;
    LayoutUnit marginEnd(const RenderStyle* otherStyle = 0) const override final;

    void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override final;
    void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    LayoutSize offsetFromContainer(RenderElement&, const LayoutPoint&, bool* offsetDependsOnPoint = nullptr) const override final;

    IntRect borderBoundingBox() const override final
    {
        IntRect boundingBox = linesBoundingBox();
        return IntRect(0, 0, boundingBox.width(), boundingBox.height());
    }

    WEBCORE_EXPORT IntRect linesBoundingBox() const;
    LayoutRect linesVisualOverflowBoundingBox() const;
    LayoutRect linesVisualOverflowBoundingBoxInRegion(const RenderRegion*) const;

    InlineFlowBox* createAndAppendInlineFlowBox();

    void dirtyLineBoxes(bool fullLayout);
    void deleteLines();

    RenderLineBoxList& lineBoxes() { return m_lineBoxes; }
    const RenderLineBoxList& lineBoxes() const { return m_lineBoxes; }

    InlineFlowBox* firstLineBox() const { return m_lineBoxes.firstLineBox(); }
    InlineFlowBox* lastLineBox() const { return m_lineBoxes.lastLineBox(); }
    InlineBox* firstLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? firstLineBox() : culledInlineFirstLineBox(); }
    InlineBox* lastLineBoxIncludingCulling() const { return alwaysCreateLineBoxes() ? lastLineBox() : culledInlineLastLineBox(); }

#if PLATFORM(IOS)
    void absoluteQuadsForSelection(Vector<FloatQuad>& quads) const override;
#endif

    RenderBoxModelObject* virtualContinuation() const override final { return continuation(); }
    RenderInline* inlineElementContinuation() const;

    void updateDragState(bool dragOn) override final;
    
    LayoutSize offsetForInFlowPositionedInline(const RenderBox* child) const;

    void addFocusRingRects(Vector<LayoutRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override final;
    void paintOutline(PaintInfo&, const LayoutPoint&);

    using RenderBoxModelObject::continuation;
    using RenderBoxModelObject::setContinuation;

    bool alwaysCreateLineBoxes() const { return renderInlineAlwaysCreatesLineBoxes(); }
    void setAlwaysCreateLineBoxes() { setRenderInlineAlwaysCreatesLineBoxes(true); }
    void updateAlwaysCreateLineBoxes(bool fullLayout);

    LayoutRect localCaretRect(InlineBox*, int, LayoutUnit* extraWidthToEndOfLine) override final;

    bool hitTestCulledInline(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset);

protected:
    void willBeDestroyed() override;

    void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

    void updateFromStyle() override;

private:
    const char* renderName() const override;

    bool canHaveChildren() const override final { return true; }

    LayoutRect culledInlineVisualOverflowBoundingBox() const;
    InlineBox* culledInlineFirstLineBox() const;
    InlineBox* culledInlineLastLineBox() const;

    template<typename GeneratorContext>
    void generateLineBoxRects(GeneratorContext& yield) const;
    template<typename GeneratorContext>
    void generateCulledLineBoxRects(GeneratorContext& yield, const RenderInline* container) const;

    void addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild);
    void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild = nullptr) override final;

    void splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                      RenderObject* beforeChild, RenderBoxModelObject* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderBoxModelObject* oldCont);

    void layout() override final { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    void paint(PaintInfo&, const LayoutPoint&) override final;

    bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override final;

    bool requiresLayer() const override { return isInFlowPositioned() || createsGroup() || hasClipPath() || willChangeCreatesStackingContext(); }

    LayoutUnit offsetLeft() const override final;
    LayoutUnit offsetTop() const override final;
    LayoutUnit offsetWidth() const override final { return linesBoundingBox().width(); }
    LayoutUnit offsetHeight() const override final { return linesBoundingBox().height(); }

    LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override;
    LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const override final;
    LayoutRect computeRectForRepaint(const LayoutRect&, const RenderLayerModelObject* repaintContainer, bool fixed) const override final;

    void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags, bool* wasFixed) const override;
    const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;

    VisiblePosition positionForPoint(const LayoutPoint&, const RenderRegion*) override final;

    LayoutRect frameRectForStickyPositioning() const override final { return linesBoundingBox(); }

    virtual std::unique_ptr<InlineFlowBox> createInlineFlowBox(); // Subclassed by RenderSVGInline

    void dirtyLinesFromChangedChild(RenderObject& child) override final { m_lineBoxes.dirtyLinesFromChangedChild(*this, child); }

    LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override final;
    int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override final;
    
    void childBecameNonInline(RenderElement&) override final;

    void updateHitTestResult(HitTestResult&, const LayoutPoint&) override final;

    void imageChanged(WrappedImagePtr, const IntRect* = 0) override final;

#if ENABLE(DASHBOARD_SUPPORT)
    void addAnnotatedRegions(Vector<AnnotatedRegionValue>&) override final;
#endif
    
    RenderPtr<RenderInline> clone() const;

    void paintOutlineForLine(GraphicsContext&, const LayoutPoint&, const LayoutRect& prevLine, const LayoutRect& thisLine,
                             const LayoutRect& nextLine, const Color);
    RenderBoxModelObject* continuationBefore(RenderObject* beforeChild);

    bool willChangeCreatesStackingContext() const
    {
        return style().willChange() && style().willChange()->canCreateStackingContext();
    }

    RenderLineBoxList m_lineBoxes;   // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderInline, isRenderInline())

#endif // RenderInline_h
