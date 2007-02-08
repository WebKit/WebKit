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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

    virtual int minWidth() const { return m_minWidth; }
    virtual int maxWidth() const { return m_maxWidth; }

    virtual int overrideSize() const { return m_overrideSize; }
    virtual int overrideWidth() const;
    virtual int overrideHeight() const;
    virtual void setOverrideSize(int s) { m_overrideSize = s; }

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

    virtual IntRect getAbsoluteRepaintRect();
    virtual void computeAbsoluteRepaintRect(IntRect&, bool fixed = false);

    virtual void repaintDuringLayoutIfMoved(const IntRect&);

    virtual int containingBlockWidth() const;

    virtual void calcWidth();
    virtual void calcHeight();

    bool stretchesToViewHeight() const
    {
        return style()->htmlHacks() && style()->height().isAuto() && !isFloatingOrPositioned() && (isRoot() || isBody());
    }

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

    virtual int staticX() const { return m_staticX; }
    virtual int staticY() const { return m_staticY; }
    virtual void setStaticX(int staticX) { m_staticX = staticX; }
    virtual void setStaticY(int staticY);

    virtual IntRect getOverflowClipRect(int tx, int ty);
    virtual IntRect getClipRect(int tx, int ty);

    virtual void paintBoxDecorations(PaintInfo&, int tx, int ty);
    virtual void imageChanged(CachedImage*);

protected:
    void paintBackground(GraphicsContext*, const Color&, const BackgroundLayer*, int clipY, int clipHeight, int tx, int ty, int width, int height);
#if PLATFORM(MAC)
    void paintCustomHighlight(int tx, int ty, const AtomicString& type, bool behindText);
#endif

    void calcAbsoluteHorizontal();

private:
    void paintRootBoxDecorations(PaintInfo&, int tx, int ty);

    void calculateBackgroundImageGeometry(const BackgroundLayer*, int tx, int ty, int w, int h, IntRect& destRect, IntPoint& phase, IntSize& tileSize);
    void paintBackgrounds(GraphicsContext*, const Color&, const BackgroundLayer*, int clipY, int clipHeight, int tx, int ty, int width, int height);

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

    // The minimum width the element needs to be able to render
    // its content without clipping.
    int m_minWidth;
    // The maximum width the element can fill horizontally
    // (the width of the element with line breaking disabled).
    int m_maxWidth;

    // A pointer to our layer if we have one.  Currently only positioned elements
    // and floaters have layers.
    RenderLayer* m_layer;

    // For inline replaced elements, the inline box that owns us.
    InlineBox* m_inlineBoxWrapper;

private:
    // Used by flexible boxes when flexing this element.
    int m_overrideSize;

    // Cached normal flow values for absolute positioned elements with static left/top values.
    int m_staticX;
    int m_staticY;
};

} // namespace WebCore

#endif // RenderBox_h
