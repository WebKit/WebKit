/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef RenderBox_h
#define RenderBox_h

#include "RenderBoxModelObject.h"
#include "RenderOverflow.h"
#include "ScrollTypes.h"

namespace WebCore {

enum WidthType { Width, MinWidth, MaxWidth };

class RenderBox : public RenderBoxModelObject {
public:
    RenderBox(Node*);
    virtual ~RenderBox();

    // Use this with caution! No type checking is done!
    RenderBox* firstChildBox() const;
    RenderBox* lastChildBox() const;

    int x() const { return m_frameRect.x(); }
    int y() const { return m_frameRect.y(); }
    int width() const { return m_frameRect.width(); }
    int height() const { return m_frameRect.height(); }
    
    void setX(int x) { m_frameRect.setX(x); }
    void setY(int y) { m_frameRect.setY(y); }
    void setWidth(int width) { m_frameRect.setWidth(width); }
    void setHeight(int height) { m_frameRect.setHeight(height); }
    
    IntPoint location() const { return m_frameRect.location(); }
    IntSize locationOffset() const { return IntSize(x(), y()); }
    IntSize size() const { return m_frameRect.size(); }

    void setLocation(const IntPoint& location) { m_frameRect.setLocation(location); }
    void setLocation(int x, int y) { setLocation(IntPoint(x, y)); }
    
    void setSize(const IntSize& size) { m_frameRect.setSize(size); }
    void move(int dx, int dy) { m_frameRect.move(dx, dy); }

    IntRect frameRect() const { return m_frameRect; }
    void setFrameRect(const IntRect& rect) { m_frameRect = rect; }

    IntRect borderBoxRect() const { return IntRect(0, 0, width(), height()); }
    virtual IntRect borderBoundingBox() const { return borderBoxRect(); } 
    
    // The content area of the box (excludes padding and border).
    IntRect contentBoxRect() const { return IntRect(borderLeft() + paddingLeft(), borderTop() + paddingTop(), contentWidth(), contentHeight()); }
    // The content box in absolute coords. Ignores transforms.
    IntRect absoluteContentBox() const;
    // The content box converted to absolute coords (taking transforms into account).
    FloatQuad absoluteContentQuad() const;

    // Bounds of the outline box in absolute coords. Respects transforms
    virtual IntRect outlineBoundsForRepaint(RenderBoxModelObject* /*repaintContainer*/, IntPoint* cachedOffsetToRepaintContainer) const;
    virtual void addFocusRingRects(Vector<IntRect>&, int tx, int ty);

    // Use this with caution! No type checking is done!
    RenderBox* previousSiblingBox() const;
    RenderBox* nextSiblingBox() const;
    RenderBox* parentBox() const;

    IntRect visibleOverflowRect() const { return hasOverflowClip() ? visualOverflowRect() : (m_overflow ? m_overflow->visibleOverflowRect() : borderBoxRect()); }
    int topVisibleOverflow() const { return hasOverflowClip() ? topVisualOverflow() : std::min(topLayoutOverflow(), topVisualOverflow()); }
    int bottomVisibleOverflow() const { return hasOverflowClip() ? bottomVisualOverflow() : std::max(bottomLayoutOverflow(), bottomVisualOverflow()); }
    int leftVisibleOverflow() const { return hasOverflowClip() ? leftVisualOverflow() : std::min(leftLayoutOverflow(), leftVisualOverflow()); }
    int rightVisibleOverflow() const { return hasOverflowClip() ? rightVisualOverflow() :  std::max(rightLayoutOverflow(), rightVisualOverflow()); }
    
    IntRect layoutOverflowRect() const { return m_overflow ? m_overflow->layoutOverflowRect() : borderBoxRect(); }
    int topLayoutOverflow() const { return m_overflow? m_overflow->topLayoutOverflow() : 0; }
    int bottomLayoutOverflow() const { return m_overflow ? m_overflow->bottomLayoutOverflow() : height(); }
    int leftLayoutOverflow() const { return m_overflow ? m_overflow->leftLayoutOverflow() : 0; }
    int rightLayoutOverflow() const { return m_overflow ? m_overflow->rightLayoutOverflow() : width(); }
    
    IntRect visualOverflowRect() const { return m_overflow ? m_overflow->visualOverflowRect() : borderBoxRect(); }
    int topVisualOverflow() const { return m_overflow? m_overflow->topVisualOverflow() : 0; }
    int bottomVisualOverflow() const { return m_overflow ? m_overflow->bottomVisualOverflow() : height(); }
    int leftVisualOverflow() const { return m_overflow ? m_overflow->leftVisualOverflow() : 0; }
    int rightVisualOverflow() const { return m_overflow ? m_overflow->rightVisualOverflow() : width(); }

    void addLayoutOverflow(const IntRect&);
    void addVisualOverflow(const IntRect&);
    
    void addShadowOverflow();
    void addOverflowFromChild(RenderBox* child) { addOverflowFromChild(child, IntSize(child->x(), child->y())); }
    void addOverflowFromChild(RenderBox* child, const IntSize& delta);
    void clearLayoutOverflow();

    int contentWidth() const { return clientWidth() - paddingLeft() - paddingRight(); }
    int contentHeight() const { return clientHeight() - paddingTop() - paddingBottom(); }

    // IE extensions. Used to calculate offsetWidth/Height.  Overridden by inlines (RenderFlow)
    // to return the remaining width on a given line (and the height of a single line).
    virtual int offsetWidth() const { return width(); }
    virtual int offsetHeight() const { return height(); }

    // More IE extensions.  clientWidth and clientHeight represent the interior of an object
    // excluding border and scrollbar.  clientLeft/Top are just the borderLeftWidth and borderTopWidth.
    int clientLeft() const { return borderLeft(); }
    int clientTop() const { return borderTop(); }
    int clientWidth() const;
    int clientHeight() const;

    // scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
    // object has overflow:hidden/scroll/auto specified and also has overflow.
    // scrollLeft/Top return the current scroll position.  These methods are virtual so that objects like
    // textareas can scroll shadow content (but pretend that they are the objects that are
    // scrolling).
    virtual int scrollLeft() const;
    virtual int scrollTop() const;
    virtual int scrollWidth() const;
    virtual int scrollHeight() const;
    virtual void setScrollLeft(int);
    virtual void setScrollTop(int);

    virtual int marginTop() const { return m_marginTop; }
    virtual int marginBottom() const { return m_marginBottom; }
    virtual int marginLeft() const { return m_marginLeft; }
    virtual int marginRight() const { return m_marginRight; }

    // The following five functions are used to implement collapsing margins.
    // All objects know their maximal positive and negative margins.  The
    // formula for computing a collapsed margin is |maxPosMargin| - |maxNegmargin|.
    // For a non-collapsing box, such as a leaf element, this formula will simply return
    // the margin of the element.  Blocks override the maxTopMargin and maxBottomMargin
    // methods.
    virtual bool isSelfCollapsingBlock() const { return false; }
    int collapsedMarginTop() const { return maxTopMargin(true) - maxTopMargin(false); }
    int collapsedMarginBottom() const { return maxBottomMargin(true) - maxBottomMargin(false); }
    virtual int maxTopMargin(bool positive) const { return positive ? std::max(0, marginTop()) : -std::min(0, marginTop()); }
    virtual int maxBottomMargin(bool positive) const { return positive ? std::max(0, marginBottom()) : -std::min(0, marginBottom()); }

    virtual void absoluteRects(Vector<IntRect>&, int tx, int ty);
    virtual void absoluteQuads(Vector<FloatQuad>&);
    
    IntRect reflectionBox() const;
    int reflectionOffset() const;
    // Given a rect in the object's coordinate space, returns the corresponding rect in the reflection.
    IntRect reflectedRect(const IntRect&) const;

    virtual void layout();
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual void destroy();

    virtual int minPrefWidth() const;
    virtual int maxPrefWidth() const;

    int overrideSize() const;
    int overrideWidth() const;
    int overrideHeight() const;
    virtual void setOverrideSize(int);

    virtual IntSize offsetFromContainer(RenderObject*, const IntPoint&) const;
    
    int calcBorderBoxWidth(int width) const;
    int calcBorderBoxHeight(int height) const;
    int calcContentBoxWidth(int width) const;
    int calcContentBoxHeight(int height) const;

    virtual void borderFitAdjust(int& /*x*/, int& /*w*/) const { } // Shrink the box in which the border paints if border-fit is set.

    // This method is now public so that centered objects like tables that are
    // shifted right by left-aligned floats can recompute their left and
    // right margins (so that they can remain centered after being
    // shifted. -dwh
    void calcHorizontalMargins(const Length& marginLeft, const Length& marginRight, int containerWidth);

    void positionLineBox(InlineBox*);

    virtual InlineBox* createInlineBox();
    void dirtyLineBoxes(bool fullLayout);

    // For inline replaced elements, this function returns the inline box that owns us.  Enables
    // the replaced RenderObject to quickly determine what line it is contained on and to easily
    // iterate over structures on the line.
    InlineBox* inlineBoxWrapper() const { return m_inlineBoxWrapper; }
    void setInlineBoxWrapper(InlineBox* boxWrapper) { m_inlineBoxWrapper = boxWrapper; }
    void deleteLineBoxWrapper();

    virtual int lowestPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int rightmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int leftmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;

    virtual IntRect clippedOverflowRectForRepaint(RenderBoxModelObject* repaintContainer);
    virtual void computeRectForRepaint(RenderBoxModelObject* repaintContainer, IntRect&, bool fixed = false);

    virtual void repaintDuringLayoutIfMoved(const IntRect&);

    virtual int containingBlockWidthForContent() const;

    virtual void calcWidth();
    virtual void calcHeight();

    bool stretchesToViewHeight() const
    {
        return document()->inQuirksMode() && style()->height().isAuto() && !isFloatingOrPositioned() && (isRoot() || isBody());
    }

    virtual IntSize intrinsicSize() const { return IntSize(); }

    // Whether or not the element shrinks to its intrinsic width (rather than filling the width
    // of a containing block).  HTML4 buttons, <select>s, <input>s, legends, and floating/compact elements do this.
    bool sizesToIntrinsicWidth(WidthType) const;
    virtual bool stretchesToMinIntrinsicWidth() const { return false; }

    int calcWidthUsing(WidthType, int containerWidth);
    int calcHeightUsing(const Length& height);
    int calcReplacedWidthUsing(Length width) const;
    int calcReplacedHeightUsing(Length height) const;

    virtual int calcReplacedWidth(bool includeMaxWidth = true) const;
    virtual int calcReplacedHeight() const;

    int calcPercentageHeight(const Length& height);

    // Block flows subclass availableWidth to handle multi column layout (shrinking the width available to children when laying out.)
    virtual int availableWidth() const { return contentWidth(); } // FIXME: Investigate removing eventually. https://bugs.webkit.org/show_bug.cgi?id=46127
    virtual int availableHeight() const;
    int availableHeightUsing(const Length&) const;
    virtual int availableLogicalWidth() const;

    void calcVerticalMargins();

    virtual int verticalScrollbarWidth() const;
    int horizontalScrollbarHeight() const;
    virtual bool scroll(ScrollDirection, ScrollGranularity, float multiplier = 1, Node** stopNode = 0);
    bool canBeScrolledAndHasScrollableArea() const;
    virtual bool canBeProgramaticallyScrolled(bool) const;
    virtual void autoscroll();
    virtual void stopAutoscroll() { }
    virtual void panScroll(const IntPoint&);
    bool hasAutoVerticalScrollbar() const { return hasOverflowClip() && (style()->overflowY() == OAUTO || style()->overflowY() == OOVERLAY); }
    bool hasAutoHorizontalScrollbar() const { return hasOverflowClip() && (style()->overflowX() == OAUTO || style()->overflowX() == OOVERLAY); }
    bool scrollsOverflow() const { return scrollsOverflowX() || scrollsOverflowY(); }
    bool scrollsOverflowX() const { return hasOverflowClip() && (style()->overflowX() == OSCROLL || hasAutoHorizontalScrollbar()); }
    bool scrollsOverflowY() const { return hasOverflowClip() && (style()->overflowY() == OSCROLL || hasAutoVerticalScrollbar()); }
    
    virtual IntRect localCaretRect(InlineBox*, int caretOffset, int* extraWidthToEndOfLine = 0);

    virtual IntRect overflowClipRect(int tx, int ty);
    IntRect clipRect(int tx, int ty);
    virtual bool hasControlClip() const { return false; }
    virtual IntRect controlClipRect(int /*tx*/, int /*ty*/) const { return IntRect(); }
    bool pushContentsClip(PaintInfo&, int tx, int ty);
    void popContentsClip(PaintInfo&, PaintPhase originalPhase, int tx, int ty);

    virtual void paintObject(PaintInfo&, int /*tx*/, int /*ty*/) { ASSERT_NOT_REACHED(); }
    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);
    virtual void paintMask(PaintInfo&, int tx, int ty);
    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0);

    // Called when a positioned object moves but doesn't necessarily change size.  A simplified layout is attempted
    // that just updates the object's position. If the size does change, the object remains dirty.
    void tryLayoutDoingPositionedMovementOnly()
    {
        int oldWidth = width();
        calcWidth();
        // If we shrink to fit our width may have changed, so we still need full layout.
        if (oldWidth != width())
            return;
        calcHeight();
        setNeedsLayout(false);
    }

    IntRect maskClipRect();

    virtual VisiblePosition positionForPoint(const IntPoint&);

    void removeFloatingOrPositionedChildFromBlockLists();
    
    virtual int firstLineBoxBaseline() const { return -1; }
    virtual int lastLineBoxBaseline() const { return -1; }

    bool shrinkToAvoidFloats() const;
    virtual bool avoidsFloats() const;

    virtual void markDescendantBlocksAndLinesForLayout(bool inLayout = true);
    
protected:
    virtual void styleWillChange(StyleDifference, const RenderStyle* newStyle);
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle);
    virtual void updateBoxModelInfoFromStyle();

    void paintFillLayer(const PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int width, int height, CompositeOperator op, RenderObject* backgroundObject);
    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer*, int tx, int ty, int width, int height, CompositeOperator = CompositeSourceOver, RenderObject* backgroundObject = 0);

    void paintBoxDecorationsWithSize(PaintInfo&, int tx, int ty, int width, int height);
    void paintMaskImages(const PaintInfo&, int tx, int ty, int width, int height);

#if PLATFORM(MAC)
    void paintCustomHighlight(int tx, int ty, const AtomicString& type, bool behindText);
#endif

    void calcAbsoluteHorizontal();
    
    virtual bool shouldCalculateSizeAsReplaced() const { return isReplaced() && !isInlineBlockOrInlineTable(); }

    virtual void mapLocalToContainer(RenderBoxModelObject* repaintContainer, bool fixed, bool useTransforms, TransformState&) const;
    virtual void mapAbsoluteToLocalPoint(bool fixed, bool useTransforms, TransformState&) const;

private:
    bool includeVerticalScrollbarSize() const { return hasOverflowClip() && (style()->overflowY() == OSCROLL || style()->overflowY() == OAUTO); }
    bool includeHorizontalScrollbarSize() const { return hasOverflowClip() && (style()->overflowX() == OSCROLL || style()->overflowX() == OAUTO); }

    void paintRootBoxDecorations(PaintInfo&, int tx, int ty);
    // Returns true if we did a full repaint
    bool repaintLayerRectsForImage(WrappedImagePtr image, const FillLayer* layers, bool drawingBackground);
   
    int containingBlockWidthForPositioned(const RenderBoxModelObject* containingBlock) const;
    int containingBlockHeightForPositioned(const RenderBoxModelObject* containingBlock) const;

    void calcAbsoluteVertical();
    void calcAbsoluteHorizontalValues(Length width, const RenderBoxModelObject* cb, TextDirection containerDirection,
                                      int containerWidth, int bordersPlusPadding,
                                      Length left, Length right, Length marginLeft, Length marginRight,
                                      int& widthValue, int& marginLeftValue, int& marginRightValue, int& xPos);
    void calcAbsoluteVerticalValues(Length height, const RenderBoxModelObject* cb,
                                    int containerHeight, int bordersPlusPadding,
                                    Length top, Length bottom, Length marginTop, Length marginBottom,
                                    int& heightValue, int& marginTopValue, int& marginBottomValue, int& yPos);

    void calcAbsoluteVerticalReplaced();
    void calcAbsoluteHorizontalReplaced();

    // This function calculates the minimum and maximum preferred widths for an object.
    // These values are used in shrink-to-fit layout systems.
    // These include tables, positioned objects, floats and flexible boxes.
    virtual void calcPrefWidths() { setPrefWidthsDirty(false); }

private:
    // The width/height of the contents + borders + padding.  The x/y location is relative to our container (which is not always our parent).
    IntRect m_frameRect;

protected:
    int m_marginLeft;
    int m_marginRight;
    int m_marginTop;
    int m_marginBottom;

    // The preferred width of the element if it were to break its lines at every possible opportunity.
    int m_minPrefWidth;
    
    // The preferred width of the element if it never breaks any lines at all.
    int m_maxPrefWidth;

    // For inline replaced elements, the inline box that owns us.
    InlineBox* m_inlineBoxWrapper;

    // Our overflow information.
    OwnPtr<RenderOverflow> m_overflow;

private:
    // Used to store state between styleWillChange and styleDidChange
    static bool s_hadOverflowClip;
};

inline RenderBox* toRenderBox(RenderObject* object)
{ 
    ASSERT(!object || object->isBox());
    return static_cast<RenderBox*>(object);
}

inline const RenderBox* toRenderBox(const RenderObject* object)
{ 
    ASSERT(!object || object->isBox());
    return static_cast<const RenderBox*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderBox(const RenderBox*);

inline RenderBox* RenderBox::previousSiblingBox() const
{
    return toRenderBox(previousSibling());
}

inline RenderBox* RenderBox::nextSiblingBox() const
{ 
    return toRenderBox(nextSibling());
}

inline RenderBox* RenderBox::parentBox() const
{
    return toRenderBox(parent());
}

inline RenderBox* RenderBox::firstChildBox() const
{
    return toRenderBox(firstChild());
}

inline RenderBox* RenderBox::lastChildBox() const
{
    return toRenderBox(lastChild());
}

} // namespace WebCore

#endif // RenderBox_h
