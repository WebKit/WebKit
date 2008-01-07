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

#include "RenderObject.h"

namespace WebCore {

    enum WidthType { Width, MinWidth, MaxWidth };

class RenderBox : public RenderObject {
public:
    RenderBox(Node*);
    virtual ~RenderBox();

    virtual const char* renderName() const { return "RenderBox"; }

    virtual void setStyle(RenderStyle*);
    virtual void paint(PaintInfo&, int tx, int ty);
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);

    virtual void destroy();

    virtual int minPrefWidth() const;
    virtual int maxPrefWidth() const;

    virtual int overrideSize() const;
    virtual int overrideWidth() const;
    virtual int overrideHeight() const;
    virtual void setOverrideSize(int);

    virtual bool absolutePosition(int& x, int& y, bool fixed = false) const;

    virtual int xPos() const { return m_x; }
    virtual int yPos() const { return m_y; }
    virtual void setPos(int x, int y);

    virtual int width() const { return m_width; }
    virtual int height() const { return m_height; }
    virtual void setWidth(int width) { m_width = width; }
    virtual void setHeight(int height) { m_height = height; }

    virtual int marginTop() const { return m_marginTop; }
    virtual int marginBottom() const { return m_marginBottom; }
    virtual int marginLeft() const { return m_marginLeft; }
    virtual int marginRight() const { return m_marginRight; }

    virtual IntRect borderBox() const { return IntRect(0, -borderTopExtra(), width(), height() + borderTopExtra() + borderBottomExtra()); }

    int calcBorderBoxWidth(int width) const;
    int calcBorderBoxHeight(int height) const;
    int calcContentBoxWidth(int width) const;
    int calcContentBoxHeight(int height) const;

    virtual void borderFitAdjust(int& x, int& w) const {}; // Shrink the box in which the border paints if border-fit is set.

    // This method is now public so that centered objects like tables that are
    // shifted right by left-aligned floats can recompute their left and
    // right margins (so that they can remain centered after being
    // shifted. -dwh
    void calcHorizontalMargins(const Length& marginLeft, const Length& marginRight, int containerWidth);

    virtual void position(InlineBox*);

    virtual void dirtyLineBoxes(bool fullLayout, bool isRootLineBox = false);

    // For inline replaced elements, this function returns the inline box that owns us.  Enables
    // the replaced RenderObject to quickly determine what line it is contained on and to easily
    // iterate over structures on the line.
    virtual InlineBox* inlineBoxWrapper() const { return m_inlineBoxWrapper; }
    virtual void setInlineBoxWrapper(InlineBox* boxWrapper) { m_inlineBoxWrapper = boxWrapper; }
    virtual void deleteLineBoxWrapper();

    virtual int lowestPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int rightmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;
    virtual int leftmostPosition(bool includeOverflowInterior = true, bool includeSelf = true) const;

    virtual IntRect absoluteClippedOverflowRect();
    virtual void computeAbsoluteRepaintRect(IntRect&, bool fixed = false);
    IntSize offsetForPositionedInContainer(RenderObject*) const;

    virtual void repaintDuringLayoutIfMoved(const IntRect&);

    virtual int containingBlockWidth() const;

    virtual void calcWidth();
    virtual void calcHeight();

    bool stretchesToViewHeight() const
    {
        return style()->htmlHacks() && style()->height().isAuto() && !isFloatingOrPositioned() && (isRoot() || isBody());
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

    virtual int calcReplacedWidth() const;
    virtual int calcReplacedHeight() const;

    int calcPercentageHeight(const Length& height);

    virtual int availableHeight() const;
    int availableHeightUsing(const Length&) const;

    void calcVerticalMargins();

    int relativePositionOffsetX() const;
    int relativePositionOffsetY() const;

    virtual RenderLayer* layer() const { return m_layer; }

    virtual IntRect caretRect(int offset, EAffinity = UPSTREAM, int* extraWidthToEndOfLine = 0);

    virtual void paintBackgroundExtended(GraphicsContext*, const Color&, const BackgroundLayer*, int clipY, int clipHeight,
                                         int tx, int ty, int width, int height, bool includeLeftEdge = true, bool includeRightEdge = true);
    IntSize calculateBackgroundSize(const BackgroundLayer*, int scaledWidth, int scaledHeight) const;

    virtual int staticX() const;
    virtual int staticY() const;
    virtual void setStaticX(int staticX);
    virtual void setStaticY(int staticY);

    virtual IntRect getOverflowClipRect(int tx, int ty);
    virtual IntRect getClipRect(int tx, int ty);

    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);
    virtual void imageChanged(CachedImage*);

protected:
    void paintBackground(GraphicsContext*, const Color&, const BackgroundLayer*, int clipY, int clipHeight, int tx, int ty, int width, int height);
    void paintBackgrounds(GraphicsContext*, const Color&, const BackgroundLayer*, int clipY, int clipHeight, int tx, int ty, int width, int height);

#if PLATFORM(MAC)
    void paintCustomHighlight(int tx, int ty, const AtomicString& type, bool behindText);
#endif

    void calcAbsoluteHorizontal();
    
    virtual bool shouldCalculateSizeAsReplaced() const { return isReplaced() && !isInlineBlockOrInlineTable(); }

private:
    void paintRootBoxDecorations(PaintInfo&, int tx, int ty);

    void calculateBackgroundImageGeometry(const BackgroundLayer*, int tx, int ty, int w, int h, IntRect& destRect, IntPoint& phase, IntSize& tileSize);
    
    int containingBlockWidthForPositioned(const RenderObject* containingBlock) const;
    int containingBlockHeightForPositioned(const RenderObject* containingBlock) const;

    void calcAbsoluteVertical();
    void calcAbsoluteHorizontalValues(Length width, const RenderObject* cb, TextDirection containerDirection,
                                      int containerWidth, int bordersPlusPadding,
                                      Length left, Length right, Length marginLeft, Length marginRight,
                                      int& widthValue, int& marginLeftValue, int& marginRightValue, int& xPos);
    void calcAbsoluteVerticalValues(Length height, const RenderObject* cb,
                                    int containerHeight, int bordersPlusPadding,
                                    Length top, Length bottom, Length marginTop, Length marginBottom,
                                    int& heightValue, int& marginTopValue, int& marginBottomValue, int& yPos);

    void calcAbsoluteVerticalReplaced();
    void calcAbsoluteHorizontalReplaced();

    // This function calculates the minimum and maximum preferred widths for an object.
    // These values are used in shrink-to-fit layout systems.
    // These include tables, positioned objects, floats and flexible boxes.
    virtual void calcPrefWidths() = 0;

protected:
    // The width/height of the contents + borders + padding.
    int m_width;
    int m_height;

    int m_x;
    int m_y;

    int m_marginLeft;
    int m_marginRight;
    int m_marginTop;
    int m_marginBottom;

    // The preferred width of the element if it were to break its lines at every possible opportunity.
    int m_minPrefWidth;
    
    // The preferred width of the element if it never breaks any lines at all.
    int m_maxPrefWidth;

    // A pointer to our layer if we have one.
    RenderLayer* m_layer;

    // For inline replaced elements, the inline box that owns us.
    InlineBox* m_inlineBoxWrapper;
};

} // namespace WebCore

#endif // RenderBox_h
