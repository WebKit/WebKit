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

class RenderInline : public RenderBoxModelObject {
public:
    RenderInline(Element&, PassRef<RenderStyle>);
    RenderInline(Document&, PassRef<RenderStyle>);

    virtual void addChild(RenderObject* newChild, RenderObject* beforeChild = 0) override;

    virtual LayoutUnit marginLeft() const override FINAL;
    virtual LayoutUnit marginRight() const override FINAL;
    virtual LayoutUnit marginTop() const override FINAL;
    virtual LayoutUnit marginBottom() const override FINAL;
    virtual LayoutUnit marginBefore(const RenderStyle* otherStyle = 0) const override FINAL;
    virtual LayoutUnit marginAfter(const RenderStyle* otherStyle = 0) const override FINAL;
    virtual LayoutUnit marginStart(const RenderStyle* otherStyle = 0) const override FINAL;
    virtual LayoutUnit marginEnd(const RenderStyle* otherStyle = 0) const override FINAL;

    virtual void absoluteRects(Vector<IntRect>&, const LayoutPoint& accumulatedOffset) const override FINAL;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override;

    virtual LayoutSize offsetFromContainer(RenderObject*, const LayoutPoint&, bool* offsetDependsOnPoint = 0) const override FINAL;

    IntRect linesBoundingBox() const;
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
    virtual void absoluteQuadsForSelection(Vector<FloatQuad>& quads) const override;
#endif

    virtual RenderBoxModelObject* virtualContinuation() const override FINAL { return continuation(); }
    RenderInline* inlineElementContinuation() const;

    virtual void updateDragState(bool dragOn) override FINAL;
    
    LayoutSize offsetForInFlowPositionedInline(const RenderBox* child) const;

    virtual void addFocusRingRects(Vector<IntRect>&, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer = 0) override FINAL;
    void paintOutline(PaintInfo&, const LayoutPoint&);

    using RenderBoxModelObject::continuation;
    using RenderBoxModelObject::setContinuation;

    bool alwaysCreateLineBoxes() const { return renderInlineAlwaysCreatesLineBoxes(); }
    void setAlwaysCreateLineBoxes() { setRenderInlineAlwaysCreatesLineBoxes(true); }
    void updateAlwaysCreateLineBoxes(bool fullLayout);

    virtual LayoutRect localCaretRect(InlineBox*, int, LayoutUnit* extraWidthToEndOfLine) override FINAL;

    bool hitTestCulledInline(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset);

protected:
    virtual void willBeDestroyed() override;

    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;

private:
    virtual const char* renderName() const override;

    virtual bool canHaveChildren() const override FINAL { return true; }

    LayoutRect culledInlineVisualOverflowBoundingBox() const;
    InlineBox* culledInlineFirstLineBox() const;
    InlineBox* culledInlineLastLineBox() const;

    template<typename GeneratorContext>
    void generateLineBoxRects(GeneratorContext& yield) const;
    template<typename GeneratorContext>
    void generateCulledLineBoxRects(GeneratorContext& yield, const RenderInline* container) const;

    void addChildToContinuation(RenderObject* newChild, RenderObject* beforeChild);
    virtual void addChildIgnoringContinuation(RenderObject* newChild, RenderObject* beforeChild = 0) override FINAL;

    void splitInlines(RenderBlock* fromBlock, RenderBlock* toBlock, RenderBlock* middleBlock,
                      RenderObject* beforeChild, RenderBoxModelObject* oldCont);
    void splitFlow(RenderObject* beforeChild, RenderBlock* newBlockBox,
                   RenderObject* newChild, RenderBoxModelObject* oldCont);

    virtual void layout() override FINAL { ASSERT_NOT_REACHED(); } // Do nothing for layout()

    virtual void paint(PaintInfo&, const LayoutPoint&) override FINAL;

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction) override FINAL;

    virtual bool requiresLayer() const override { return isInFlowPositioned() || createsGroup() || hasClipPath(); }

    virtual LayoutUnit offsetLeft() const override FINAL;
    virtual LayoutUnit offsetTop() const override FINAL;
    virtual LayoutUnit offsetWidth() const override FINAL { return linesBoundingBox().width(); }
    virtual LayoutUnit offsetHeight() const override FINAL { return linesBoundingBox().height(); }

    virtual LayoutRect clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const override;
    virtual LayoutRect rectWithOutlineForRepaint(const RenderLayerModelObject* repaintContainer, LayoutUnit outlineWidth) const override FINAL;
    virtual void computeRectForRepaint(const RenderLayerModelObject* repaintContainer, LayoutRect&, bool fixed) const override FINAL;

    virtual void mapLocalToContainer(const RenderLayerModelObject* repaintContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0) const override;
    virtual const RenderObject* pushMappingToContainer(const RenderLayerModelObject* ancestorToStopAt, RenderGeometryMap&) const override;

    virtual VisiblePosition positionForPoint(const LayoutPoint&) override FINAL;

    virtual LayoutRect frameRectForStickyPositioning() const override FINAL { return linesBoundingBox(); }

    virtual IntRect borderBoundingBox() const override FINAL
    {
        IntRect boundingBox = linesBoundingBox();
        return IntRect(0, 0, boundingBox.width(), boundingBox.height());
    }

    virtual std::unique_ptr<InlineFlowBox> createInlineFlowBox(); // Subclassed by RenderSVGInline

    virtual void dirtyLinesFromChangedChild(RenderObject* child) override FINAL { m_lineBoxes.dirtyLinesFromChangedChild(this, child); }

    virtual LayoutUnit lineHeight(bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override FINAL;
    virtual int baselinePosition(FontBaseline, bool firstLine, LineDirectionMode, LinePositionMode = PositionOnContainingLine) const override FINAL;
    
    virtual void childBecameNonInline(RenderObject* child) override FINAL;

    virtual void updateHitTestResult(HitTestResult&, const LayoutPoint&) override FINAL;

    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) override FINAL;

#if ENABLE(DASHBOARD_SUPPORT) || ENABLE(DRAGGABLE_REGION)
    virtual void addAnnotatedRegions(Vector<AnnotatedRegionValue>&) override FINAL;
#endif
    
    virtual void updateFromStyle() override FINAL;
    
    RenderPtr<RenderInline> clone() const;

    void paintOutlineForLine(GraphicsContext*, const LayoutPoint&, const LayoutRect& prevLine, const LayoutRect& thisLine,
                             const LayoutRect& nextLine, const Color);
    RenderBoxModelObject* continuationBefore(RenderObject* beforeChild);

    RenderLineBoxList m_lineBoxes;   // All of the line boxes created for this inline flow.  For example, <i>Hello<br>world.</i> will have two <i> line boxes.
};

RENDER_OBJECT_TYPE_CASTS(RenderInline, isRenderInline())

} // namespace WebCore

#endif // RenderInline_h
