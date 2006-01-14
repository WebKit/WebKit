/*
 * This file is part of the render object implementation for KHTML.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#include "config.h"
#include "render_flexbox.h"

using namespace DOM;

namespace khtml {

class FlexBoxIterator {
public:
    FlexBoxIterator(RenderFlexibleBox* parent) {
        box = parent;
        if (box->style()->boxOrient() == HORIZONTAL && box->style()->direction() == RTL)
            forward = box->style()->boxDirection() != BNORMAL;
        else
            forward = box->style()->boxDirection() == BNORMAL;
        lastOrdinal = 1; 
        if (!forward) {
            // No choice, since we're going backwards, we have to find out the highest ordinal up front.
            RenderObject* child = box->firstChild();
            while (child) {
                if (child->style()->boxOrdinalGroup() > lastOrdinal)
                    lastOrdinal = child->style()->boxOrdinalGroup();
                child = child->nextSibling();
            }
        }
        
        reset();
    }

    void reset() {
        current = 0;
        currentOrdinal = forward ? 0 : lastOrdinal+1;
    }

    RenderObject* first() {
        reset();
        return next();
    }
    
    RenderObject* next() {

        do { 
            if (!current) {
                if (forward) {
                    currentOrdinal++; 
                    if (currentOrdinal > lastOrdinal)
                        return 0;
                    current = box->firstChild();
                }
                else {
                    currentOrdinal--;
                    if (currentOrdinal == 0)
                        return 0;
                    current = box->lastChild();
                }
            }
            else
                current = forward ? current->nextSibling() : current->previousSibling();
            if (current && current->style()->boxOrdinalGroup() > lastOrdinal)
                lastOrdinal = current->style()->boxOrdinalGroup();
        } while (!current || current->style()->boxOrdinalGroup() != currentOrdinal ||
                 current->style()->visibility() == COLLAPSE);
        return current;
    }

private:
    RenderFlexibleBox* box;
    RenderObject* current;
    bool forward;
    unsigned int currentOrdinal;
    unsigned int lastOrdinal;
};
    
RenderFlexibleBox::RenderFlexibleBox(DOM::NodeImpl* node)
:RenderBlock(node)
{
    setChildrenInline(false); // All of our children must be block-level
    m_flexingChildren = m_stretchingChildren = false;
}

RenderFlexibleBox::~RenderFlexibleBox()
{
}

void RenderFlexibleBox::calcHorizontalMinMaxWidth()
{
    RenderObject *child = firstChild();
    while (child) {
        // positioned children don't affect the minmaxwidth
        if (child->isPositioned() || child->style()->visibility() == COLLAPSE)
        {
            child = child->nextSibling();
            continue;
        }

        int margin=0;
        //  auto margins don't affect minwidth

        Length ml = child->style()->marginLeft();
        Length mr = child->style()->marginRight();

        // Call calcWidth on the child to ensure that our margins are
        // up to date.  This method can be called before the child has actually
        // calculated its margins (which are computed inside calcWidth).
        child->calcWidth();

        if (!(ml.type==Auto) && !(mr.type==Auto))
        {
            if (!(child->style()->width().type==Auto))
            {
                if (child->style()->direction()==LTR)
                    margin = child->marginLeft();
                else
                    margin = child->marginRight();
            }
            else
                margin = child->marginLeft()+child->marginRight();

        }
        else if (!(ml.type == Auto))
            margin = child->marginLeft();
        else if (!(mr.type == Auto))
            margin = child->marginRight();

        if (margin < 0) margin = 0;

        m_minWidth += child->minWidth() + margin;
        m_maxWidth += child->maxWidth() + margin;

        child = child->nextSibling();
    }    
}

void RenderFlexibleBox::calcVerticalMinMaxWidth()
{
    RenderObject *child = firstChild();
    while(child != 0)
    {
        // Positioned children and collapsed children don't affect the min/max width
        if (child->isPositioned() || child->style()->visibility() == COLLAPSE) {
            child = child->nextSibling();
            continue;
        }

        Length ml = child->style()->marginLeft();
        Length mr = child->style()->marginRight();

        // Call calcWidth on the child to ensure that our margins are
        // up to date.  This method can be called before the child has actually
        // calculated its margins (which are computed inside calcWidth).
        if (ml.type == Percent || mr.type == Percent)
            calcWidth();

        // A margin basically has three types: fixed, percentage, and auto (variable).
        // Auto margins simply become 0 when computing min/max width.
        // Fixed margins can be added in as is.
        // Percentage margins are computed as a percentage of the width we calculated in
        // the calcWidth call above.  In this case we use the actual cached margin values on
        // the RenderObject itself.
        int margin = 0;
        if (ml.type == Fixed)
            margin += ml.value;
        else if (ml.type == Percent)
            margin += child->marginLeft();

        if (mr.type == Fixed)
            margin += mr.value;
        else if (mr.type == Percent)
            margin += child->marginRight();

        if (margin < 0) margin = 0;
        
        int w = child->minWidth() + margin;
        if(m_minWidth < w) m_minWidth = w;
        
        w = child->maxWidth() + margin;

        if(m_maxWidth < w) m_maxWidth = w;

        child = child->nextSibling();
    }    
}

void RenderFlexibleBox::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown() );

    m_minWidth = 0;
    m_maxWidth = 0;

    if (hasMultipleLines() || isVertical())
        calcVerticalMinMaxWidth();
    else
        calcHorizontalMinMaxWidth();

    if(m_maxWidth < m_minWidth) m_maxWidth = m_minWidth;

    if (style()->width().isFixed() && style()->width().value > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value);
   
    if (style()->minWidth().isFixed() && style()->minWidth().value > 0) {
        m_maxWidth = kMax(m_maxWidth, calcContentBoxWidth(style()->minWidth().value));
        m_minWidth = kMax(m_minWidth, calcContentBoxWidth(style()->minWidth().value));
    }
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value != UNDEFINED) {
        m_maxWidth = kMin(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value));
        m_minWidth = kMin(m_minWidth, calcContentBoxWidth(style()->maxWidth().value));
    }

    int toAdd = borderLeft() + borderRight() + paddingLeft() + paddingRight();
    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();
}

void RenderFlexibleBox::layoutBlock(bool relayoutChildren)
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    if (!relayoutChildren && posChildNeedsLayout() && !normalChildNeedsLayout() && !selfNeedsLayout()) {
        // All we have to is lay out our positioned objects.
        layoutPositionedObjects(relayoutChildren);
        if (hasOverflowClip())
            m_layer->updateScrollInfoAfterLayout();
        setNeedsLayout(false);
        return;
    }

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = getAbsoluteRepaintRect();
    
    int oldWidth = m_width;
    int oldHeight = m_height;
    
    calcWidth();
    calcHeight();
    m_overflowWidth = m_width;

    if (oldWidth != m_width || oldHeight != m_height ||
        (parent()->isFlexibleBox() && parent()->style()->boxOrient() == HORIZONTAL &&
         parent()->style()->boxAlign() == BSTRETCH))
        relayoutChildren = true;

    m_height = 0;
    m_overflowHeight = 0;
    m_flexingChildren = m_stretchingChildren = false;

    initMaxMarginValues();

    if (scrollsOverflow()) {
        // For overflow:scroll blocks, ensure we have both scrollbars in place always.
        if (style()->overflow() == OSCROLL) {
            m_layer->setHasHorizontalScrollbar(true);
            m_layer->setHasVerticalScrollbar(true);
        }

        // Move the scrollbars aside during layout.  The layer will move them back when it
        // does painting or event handling.
        m_layer->moveScrollbarsAside();
    }

    if (isHorizontal())
        layoutHorizontalBox(relayoutChildren);
    else
        layoutVerticalBox(relayoutChildren);
    
    oldHeight = m_height;
    calcHeight();
    if (oldHeight != m_height) {
        relayoutChildren = true;

        // If the block got expanded in size, then increase our overflowheight to match.
        if (m_overflowHeight > m_height)
            m_overflowHeight -= (borderBottom()+paddingBottom());
        if (m_overflowHeight < m_height)
            m_overflowHeight = m_height;
    }

    layoutPositionedObjects( relayoutChildren );

    //kdDebug() << renderName() << " layout width=" << m_width << " height=" << m_height << endl;

    if (!isFloatingOrPositioned() && m_height == 0) {
        // We are a block with no border and padding and a computed height
        // of 0.  The CSS spec states that zero-height blocks collapse their margins
        // together.
        // When blocks are self-collapsing, we just use the top margin values and set the
        // bottom margin max values to 0.  This way we don't factor in the values
        // twice when we collapse with our previous vertically adjacent and
        // following vertically adjacent blocks.
        if (m_maxBottomPosMargin > m_maxTopPosMargin)
            m_maxTopPosMargin = m_maxBottomPosMargin;
        if (m_maxBottomNegMargin > m_maxTopNegMargin)
            m_maxTopNegMargin = m_maxBottomNegMargin;
        m_maxBottomNegMargin = m_maxBottomPosMargin = 0;
    }

    // Always ensure our overflow width is at least as large as our width.
    if (m_overflowWidth < m_width)
        m_overflowWidth = m_width;

    // Update our scrollbars if we're overflow:auto/scroll/hidden now that we know if
    // we overflow or not.
    if (hasOverflowClip())
        m_layer->updateScrollInfoAfterLayout();

    // Repaint with our new bounds if they are different from our old bounds.
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

void RenderFlexibleBox::layoutHorizontalBox(bool relayoutChildren)
{
    int toAdd = borderBottom() + paddingBottom();
    int yPos = borderTop() + paddingTop();
    int xPos = borderLeft() + paddingLeft();
    bool heightSpecified = false;
    int oldHeight = 0;
    
    unsigned int highestFlexGroup = 0;
    unsigned int lowestFlexGroup = 0;
    bool haveFlex = false;
    int remainingSpace = 0;
    m_overflowHeight = m_height;

    // The first walk over our kids is to find out if we have any flexible children.
    FlexBoxIterator iterator(this);
    RenderObject* child = iterator.next();
    while (child) {
        // Check to see if this child flexes.
        if (!child->isPositioned() && child->style()->boxFlex() > 0.0f) {
            // We always have to lay out flexible objects again, since the flex distribution
            // may have changed, and we need to reallocate space.
            child->setOverrideSize(-1);
            if (!relayoutChildren)
                child->setChildNeedsLayout(true, false);
            haveFlex = true;
            unsigned int flexGroup = child->style()->boxFlexGroup();
            if (lowestFlexGroup == 0)
                lowestFlexGroup = flexGroup;
            if (flexGroup < lowestFlexGroup)
                lowestFlexGroup = flexGroup;
            if (flexGroup > highestFlexGroup)
                highestFlexGroup = flexGroup;
        }
        child = iterator.next();
    }
    
    // We do 2 passes.  The first pass is simply to lay everyone out at
    // their preferred widths.  The second pass handles flexing the children.
    do {
        // Reset our height.
        m_height = yPos;
        m_overflowHeight = m_height;
        xPos = borderLeft() + paddingLeft();
                
        // Our first pass is done without flexing.  We simply lay the children
        // out within the box.  We have to do a layout first in order to determine
        // our box's intrinsic height.
        int maxAscent = 0, maxDescent = 0;
        child = iterator.first();
        while (child) {
            // make sure we relayout children if we need it.
            if (relayoutChildren || (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent())))
                child->setChildNeedsLayout(true, false);
            
            if (child->isPositioned()) {
                child = iterator.next();
                continue;
            }
    
            // Compute the child's vertical margins.
            child->calcVerticalMargins();
    
            // Now do the layout.
            child->layoutIfNeeded();
    
            // Update our height and overflow height.
            if (style()->boxAlign() == BBASELINE) {
                int ascent = child->marginTop() + child->getBaselineOfFirstLineBox();
                if (ascent == -1)
                    ascent = child->marginTop() + child->height() + child->marginBottom();
                int descent = (child->marginTop() + child->height() + child->marginBottom()) - ascent;
                
                // Update our maximum ascent.
                maxAscent = kMax(maxAscent, ascent);
                
                // Update our maximum descent.
                maxDescent = kMax(maxDescent, descent);
                
                // Now update our height.
                m_height = kMax(yPos + maxAscent + maxDescent, m_height);
            }
            else
                m_height = kMax(m_height, yPos + child->marginTop() + child->height() + child->marginBottom());

            child = iterator.next();
        }
        m_height += toAdd;

        // Always make sure our overflowheight is at least our height.
        if (m_overflowHeight < m_height)
            m_overflowHeight = m_height;
        
        oldHeight = m_height;
        calcHeight();

        relayoutChildren = false;
        if (oldHeight != m_height) {
            heightSpecified = true;
    
            // If the block got expanded in size, then increase our overflowheight to match.
            if (m_overflowHeight > m_height)
                m_overflowHeight -= (borderBottom() + paddingBottom());
            if (m_overflowHeight < m_height)
                m_overflowHeight = m_height;
        }
        
        // Now that our height is actually known, we can place our boxes.
        m_stretchingChildren = (style()->boxAlign() == BSTRETCH);
        child = iterator.first();
        while (child) {
            if (child->isPositioned()) {
                child->containingBlock()->insertPositionedObject(child);
                if (child->hasStaticX()) {
                    if (style()->direction() == LTR)
                        child->setStaticX(xPos);
                    else child->setStaticX(width() - xPos);
                }
                if (child->hasStaticY())
                    child->setStaticY(yPos);
                child = iterator.next();
                continue;
            }
    
            // We need to see if this child's height has changed, since we make block elements
            // fill the height of a containing box by default.
            // Now do a layout.
            int oldChildHeight = child->height();
            static_cast<RenderBox*>(child)->calcHeight();
            if (oldChildHeight != child->height())
                child->setChildNeedsLayout(true, false);
            child->layoutIfNeeded();
    
            // We can place the child now, using our value of box-align.
            xPos += child->marginLeft();
            int childY = yPos;
            switch (style()->boxAlign()) {
                case BCENTER:
                    childY += child->marginTop() + kMax(0, (contentHeight() - (child->height() + child->marginTop() + child->marginBottom()))/2);
                    break;
                case BBASELINE: {
                    int ascent = child->marginTop() + child->getBaselineOfFirstLineBox();
                    if (ascent == -1)
                        ascent = child->marginTop() + child->height() + child->marginBottom();
                    childY += child->marginTop() + (maxAscent - ascent);
                    break;
                }
                case BEND:
                    childY += contentHeight() - child->marginBottom() - child->height();
                    break;
                default: // BSTART
                    childY += child->marginTop();
                    break;
            }

            placeChild(child, xPos, childY);
            m_overflowHeight = kMax(m_overflowHeight, childY + child->overflowHeight(false));
            
            xPos += child->width() + child->marginRight();
    
            child = iterator.next();
        }

        remainingSpace = borderLeft() + paddingLeft() + contentWidth() - xPos;
        
        m_stretchingChildren = false;
        if (m_flexingChildren)
            haveFlex = false; // We're done.
        else if (haveFlex) {
            // We have some flexible objects.  See if we need to grow/shrink them at all.
            if (!remainingSpace)
                break;

            // Allocate the remaining space among the flexible objects.  If we are trying to
            // grow, then we go from the lowest flex group to the highest flex group.  For shrinking,
            // we go from the highest flex group to the lowest group.
            bool expanding = remainingSpace > 0;
            unsigned int start = expanding ? lowestFlexGroup : highestFlexGroup;
            unsigned int end = expanding? highestFlexGroup : lowestFlexGroup;
            for (unsigned int i = start; i <= end && remainingSpace; i++) {
                // Always start off by assuming the group can get all the remaining space.
                int groupRemainingSpace = remainingSpace;
                do {
                    // Flexing consists of multiple passes, since we have to change ratios every time an object hits its max/min-width
                    // For a given pass, we always start off by computing the totalFlex of all objects that can grow/shrink at all, and
                    // computing the allowed growth before an object hits its min/max width (and thus
                    // forces a totalFlex recomputation).
                    float totalFlex = 0.0f;
                    child = iterator.first();
                    while (child) {
                        if (allowedChildFlex(child, expanding, i))
                            totalFlex += child->style()->boxFlex();
                        child = iterator.next();
                    }
                    child = iterator.first();
                    int spaceAvailableThisPass = groupRemainingSpace;
                    while (child) {
                        int allowedFlex = allowedChildFlex(child, expanding, i);
                        if (allowedFlex) {
                            int projectedFlex = (allowedFlex == INT_MAX) ? allowedFlex : (int)(allowedFlex * (totalFlex / child->style()->boxFlex()));
                            spaceAvailableThisPass = expanding ? kMin(spaceAvailableThisPass, projectedFlex) : kMax(spaceAvailableThisPass, projectedFlex);
                        }
                        child = iterator.next();
                    }

                    // The flex groups may not have any flexible objects this time around. 
                    if (!spaceAvailableThisPass || totalFlex == 0.0f) {
                        // If we just couldn't grow/shrink any more, then it's time to transition to the next flex group.
                        groupRemainingSpace = 0;
                        continue;
                    }

                    // Now distribute the space to objects.
                    child = iterator.first();
                    while (child && spaceAvailableThisPass && totalFlex) {
                        if (allowedChildFlex(child, expanding, i)) {
                            int spaceAdd = (int)(spaceAvailableThisPass * (child->style()->boxFlex()/totalFlex));
                            if (spaceAdd) {
                                child->setOverrideSize(child->overrideWidth() + spaceAdd);
                                m_flexingChildren = true;
                                relayoutChildren = true;
                            }

                            spaceAvailableThisPass -= spaceAdd;
                            remainingSpace -= spaceAdd;
                            groupRemainingSpace -= spaceAdd;
                            
                            totalFlex -= child->style()->boxFlex();
                        }
                        child = iterator.next();
                    }
                } while (groupRemainingSpace);
            }

            // We didn't find any children that could grow.
            if (haveFlex && !m_flexingChildren)
                haveFlex = false;
        }
    } while (haveFlex);

    m_flexingChildren = false;
    
    if (xPos > m_overflowWidth)
        m_overflowWidth = xPos;

    if (remainingSpace > 0 && ((style()->direction() == LTR && style()->boxPack() != BSTART) ||
                               (style()->direction() == RTL && style()->boxPack() != BEND))) {
        // Children must be repositioned.
        int offset = 0;
        if (style()->boxPack() == BJUSTIFY) {
            // Determine the total number of children.
            int totalChildren = 0;
            child = iterator.first();
            while (child) {
                if (child->isPositioned()) {
                    child = iterator.next();
                    continue;
                }
                totalChildren++;
                child = iterator.next();
            }

            // Iterate over the children and space them out according to the
            // justification level.
            if (totalChildren > 1) {
                totalChildren--;
                bool firstChild = true;
                child = iterator.first();
                while (child) {
                    if (child->isPositioned()) {
                        child = iterator.next();
                        continue;
                    }

                    if (firstChild) {
                        firstChild = false;
                        child = iterator.next();
                        continue;
                    }

                    offset += remainingSpace/totalChildren;
                    remainingSpace -= (remainingSpace/totalChildren);
                    totalChildren--;

                    placeChild(child, child->xPos()+offset, child->yPos());
                    child = iterator.next();
                }
            }
        }
        else {
            if (style()->boxPack() == BCENTER)
                offset += remainingSpace/2;
            else // END for LTR, START for RTL
                offset += remainingSpace;
            child = iterator.first();
            while (child) {
                if (child->isPositioned()) {
                    child = iterator.next();
                    continue;
                }
                placeChild(child, child->xPos()+offset, child->yPos());
                child = iterator.next();
            }
        }
    }
    
    // So that the calcHeight in layoutBlock() knows to relayout positioned objects because of
    // a height change, we revert our height back to the intrinsic height before returning.
    if (heightSpecified)
        m_height = oldHeight;
}

void RenderFlexibleBox::layoutVerticalBox(bool relayoutChildren)
{
    int xPos = borderLeft() + paddingLeft();
    int yPos = borderTop() + paddingTop();
    if( style()->direction() == RTL )
        xPos = m_width - paddingRight() - borderRight();
    int toAdd = borderBottom() + paddingBottom();
    bool heightSpecified = false;
    int oldHeight = 0;
    
    unsigned int highestFlexGroup = 0;
    unsigned int lowestFlexGroup = 0;
    bool haveFlex = false;
    int remainingSpace = 0;
    
    // The first walk over our kids is to find out if we have any flexible children.
    FlexBoxIterator iterator(this);
    RenderObject *child = iterator.next();
    while (child) {
        // Check to see if this child flexes.
        if (!child->isPositioned() && child->style()->boxFlex() > 0.0f) {
            // We always have to lay out flexible objects again, since the flex distribution
            // may have changed, and we need to reallocate space.
            child->setOverrideSize(-1);
            if (!relayoutChildren)
                child->setChildNeedsLayout(true);
            haveFlex = true;
            unsigned int flexGroup = child->style()->boxFlexGroup();
            if (lowestFlexGroup == 0)
                lowestFlexGroup = flexGroup;
            if (flexGroup < lowestFlexGroup)
                lowestFlexGroup = flexGroup;
            if (flexGroup > highestFlexGroup)
                highestFlexGroup = flexGroup;
        }
        child = iterator.next();
    }

    // We confine the line clamp ugliness to vertical flexible boxes (thus keeping it out of
    // mainstream block layout); this is not really part of the XUL box model.
    bool haveLineClamp = style()->lineClamp() >= 0 && style()->lineClamp() <= 100;
    if (haveLineClamp) {
        int maxLineCount = 0;
        child = iterator.first();
        while (child) {
            if (!child->isPositioned()) {
                if (relayoutChildren || (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent())) ||
                    (child->style()->height().isAuto() && child->isBlockFlow() && !child->needsLayout())) {
                    child->setChildNeedsLayout(true);
                    
                    // Dirty all the positioned objects.
                    static_cast<RenderBlock*>(child)->markPositionedObjectsForLayout();
                    static_cast<RenderBlock*>(child)->clearTruncation();
                }
                child->layoutIfNeeded();
                if (child->style()->height().isAuto() && child->isBlockFlow())
                    maxLineCount = kMax(maxLineCount, static_cast<RenderBlock*>(child)->lineCount());
            }
            child = iterator.next();
        }
        
        // Get the # of lines and then alter all block flow children with auto height to use the
        // specified height.
        int numVisibleLines = int((maxLineCount+1)*style()->lineClamp()/100.0);
        if (numVisibleLines < maxLineCount) {
            for (child = iterator.first(); child; child = iterator.next()) {
                if (child->isPositioned() || !child->style()->height().isAuto() || !child->isBlockFlow())
                    continue;
                
                RenderBlock* blockChild = static_cast<RenderBlock*>(child);
                int lineCount = blockChild->lineCount();
                if (lineCount <= numVisibleLines) continue;
                
                int newHeight = blockChild->heightForLineCount(numVisibleLines);
                if (newHeight == child->height()) continue;
                
                child->setChildNeedsLayout(true);
                child->setOverrideSize(newHeight);
                m_flexingChildren = true;
                child->layoutIfNeeded();
                m_flexingChildren = false;
                child->setOverrideSize(-1);
                
                // FIXME: For now don't support RTL.
                if (style()->direction() != LTR) continue;
                
                // Get the last line
                RootInlineBox* lastLine = blockChild->lineAtIndex(lineCount-1);
                if (!lastLine) continue;
                
                // See if the last item is an anchor
                InlineBox* anchorBox = lastLine->lastChild();
                if (!anchorBox) continue;
                if (!anchorBox->object()->element()) continue;
                if (!anchorBox->object()->element()->isLink()) continue;
                
                RootInlineBox* lastVisibleLine = blockChild->lineAtIndex(numVisibleLines-1);
                if (!lastVisibleLine) continue;

                const unsigned short ellipsisAndSpace[2] = { 0x2026, ' ' };
                static AtomicString ellipsisAndSpaceStr(ellipsisAndSpace, 2);
                const Font& font = style(numVisibleLines == 1)->htmlFont();
                int ellipsisAndSpaceWidth = font.width(const_cast<QChar*>(ellipsisAndSpaceStr.unicode()), 2, 0, 2, 0, 0);

                // Get ellipsis width + " " + anchor width
                int totalWidth = ellipsisAndSpaceWidth + anchorBox->width();
                
                // See if this width can be accommodated on the last visible line
                RenderBlock* destBlock = static_cast<RenderBlock*>(lastVisibleLine->object());
                RenderBlock* srcBlock = static_cast<RenderBlock*>(lastLine->object());
                
                // FIXME: Directions of src/destBlock could be different from our direction and from one another.
                if (srcBlock->style()->direction() != LTR) continue;
                if (destBlock->style()->direction() != LTR) continue;

                int blockEdge = destBlock->rightOffset(lastVisibleLine->yPos());
                if (!lastVisibleLine->canAccommodateEllipsis(true, blockEdge, 
                                                             lastVisibleLine->xPos() + lastVisibleLine->width(),
                                                             totalWidth))
                    continue;

                // Let the truncation code kick in.
                lastVisibleLine->placeEllipsis(ellipsisAndSpaceStr, true, blockEdge, totalWidth, anchorBox);
                destBlock->setHasMarkupTruncation(true);
            }
        }
    }

    // We do 2 passes.  The first pass is simply to lay everyone out at
    // their preferred widths.  The second pass handles flexing the children.
    // Our first pass is done without flexing.  We simply lay the children
    // out within the box.
    do {
        m_height = borderTop() + paddingTop();
        int minHeight = m_height + toAdd;
        m_overflowHeight = m_height;

        child = iterator.first();
        while (child) {
            // make sure we relayout children if we need it.
            if (!haveLineClamp && (relayoutChildren || (child->isReplaced() && (child->style()->width().isPercent() || child->style()->height().isPercent()))))
                child->setChildNeedsLayout(true);
    
            if (child->isPositioned())
            {
                child->containingBlock()->insertPositionedObject(child);
                if (child->hasStaticX()) {
                    if (style()->direction() == LTR)
                        child->setStaticX(borderLeft()+paddingLeft());
                    else
                        child->setStaticX(borderRight()+paddingRight());
                }
                if (child->hasStaticY())
                    child->setStaticY(m_height);
                child = iterator.next();
                continue;
            } 
    
            // Compute the child's vertical margins.
            child->calcVerticalMargins();
    
            // Add in the child's marginTop to our height.
            m_height += child->marginTop();
    
            // Now do a layout.
            child->layoutIfNeeded();
    
            // We can place the child now, using our value of box-align.
            int childX = borderLeft() + paddingLeft();
            switch (style()->boxAlign()) {
                case BCENTER:
                case BBASELINE: // Baseline just maps to center for vertical boxes
                    childX += child->marginLeft() + kMax(0, (contentWidth() - (child->width() + child->marginLeft() + child->marginRight()))/2);
                    break;
                case BEND:
                    if (style()->direction() == RTL)
                        childX += child->marginLeft();
                    else
                        childX += contentWidth() - child->marginRight() - child->width();
                    break;
                default: // BSTART/BSTRETCH
                    if (style()->direction() == LTR)
                        childX += child->marginLeft();
                    else
                        childX += contentWidth() - child->marginRight() - child->width();
                    break;
            }
    
            // Place the child.
            placeChild(child, childX, m_height);
            m_height += child->height() + child->marginBottom();
    
            // See if this child has made our overflow need to grow.
            // XXXdwh Work with left overflow as well as right overflow.
            int rightChildPos = child->xPos() + kMax(child->overflowWidth(false), child->width());
            if (rightChildPos > m_overflowWidth)
                m_overflowWidth = rightChildPos;
            
            child = iterator.next();
        }

        yPos = m_height;
        m_height += toAdd;

        // Negative margins can cause our height to shrink below our minimal height (border/padding).
        // If this happens, ensure that the computed height is increased to the minimal height.
        if (m_height < minHeight)
            m_height = minHeight;

        // Always make sure our overflowheight is at least our height.
        if (m_overflowHeight < m_height)
            m_overflowHeight = m_height;

        // Now we have to calc our height, so we know how much space we have remaining.
        oldHeight = m_height;
        calcHeight();
        if (oldHeight != m_height)
            heightSpecified = true;

        remainingSpace = borderTop() + paddingTop() + contentHeight() - yPos;
        
        if (m_flexingChildren)
            haveFlex = false; // We're done.
        else if (haveFlex) {
            // We have some flexible objects.  See if we need to grow/shrink them at all.
            if (!remainingSpace)
                break;

            // Allocate the remaining space among the flexible objects.  If we are trying to
            // grow, then we go from the lowest flex group to the highest flex group.  For shrinking,
            // we go from the highest flex group to the lowest group.
            bool expanding = remainingSpace > 0;
            unsigned int start = expanding ? lowestFlexGroup : highestFlexGroup;
            unsigned int end = expanding? highestFlexGroup : lowestFlexGroup;
            for (unsigned int i = start; i <= end && remainingSpace; i++) {
                // Always start off by assuming the group can get all the remaining space.
                int groupRemainingSpace = remainingSpace;
                do {
                    // Flexing consists of multiple passes, since we have to change ratios every time an object hits its max/min-width
                    // For a given pass, we always start off by computing the totalFlex of all objects that can grow/shrink at all, and
                    // computing the allowed growth before an object hits its min/max width (and thus
                    // forces a totalFlex recomputation).
                    float totalFlex = 0.0f;
                    child = iterator.first();
                    while (child) {
                        if (allowedChildFlex(child, expanding, i))
                            totalFlex += child->style()->boxFlex();
                        child = iterator.next();
                    }
                    child = iterator.first();
                    int spaceAvailableThisPass = groupRemainingSpace;
                    while (child) {
                        int allowedFlex = allowedChildFlex(child, expanding, i);
                        if (allowedFlex) {
                            int projectedFlex = (allowedFlex == INT_MAX) ? allowedFlex : (int)(allowedFlex * (totalFlex / child->style()->boxFlex()));
                            spaceAvailableThisPass = expanding ? kMin(spaceAvailableThisPass, projectedFlex) : kMax(spaceAvailableThisPass, projectedFlex);
                        }
                        child = iterator.next();
                    }

                    // The flex groups may not have any flexible objects this time around. 
                    if (!spaceAvailableThisPass || totalFlex == 0.0f) {
                        // If we just couldn't grow/shrink any more, then it's time to transition to the next flex group.
                        groupRemainingSpace = 0;
                        continue;
                    }
            
                    // Now distribute the space to objects.
                    child = iterator.first();
                    while (child && spaceAvailableThisPass && totalFlex) {
                        if (allowedChildFlex(child, expanding, i)) {
                            int spaceAdd = (int)(spaceAvailableThisPass * (child->style()->boxFlex()/totalFlex));
                            if (spaceAdd) {
                                child->setOverrideSize(child->overrideHeight() + spaceAdd);
                                m_flexingChildren = true;
                                relayoutChildren = true;
                            }

                            spaceAvailableThisPass -= spaceAdd;
                            remainingSpace -= spaceAdd;
                            groupRemainingSpace -= spaceAdd;
                            
                            totalFlex -= child->style()->boxFlex();
                        }
                        child = iterator.next();
                    }
                } while (groupRemainingSpace);
            }

            // We didn't find any children that could grow.
            if (haveFlex && !m_flexingChildren)
                haveFlex = false;
        }        
    } while (haveFlex);

    if (style()->boxPack() != BSTART && remainingSpace > 0) {
        // Children must be repositioned.
        int offset = 0;
        if (style()->boxPack() == BJUSTIFY) {
            // Determine the total number of children.
            int totalChildren = 0;
            child = iterator.first();
            while (child) {
                if (child->isPositioned()) {
                    child = iterator.next();
                    continue;
                }
                totalChildren++;
                child = iterator.next();
            }
            
            // Iterate over the children and space them out according to the
            // justification level.
            if (totalChildren > 1) {
                totalChildren--;
                bool firstChild = true;
                child = iterator.first();
                while (child) {
                    if (child->isPositioned()) {
                        child = iterator.next();
                        continue;
                    }
                    
                    if (firstChild) {
                        firstChild = false;
                        child = iterator.next();
                        continue;
                    }

                    offset += remainingSpace/totalChildren;
                    remainingSpace -= (remainingSpace/totalChildren);
                    totalChildren--;
                    placeChild(child, child->xPos(), child->yPos()+offset);
                    child = iterator.next();
                }
            }
        }
        else {
            if (style()->boxPack() == BCENTER)
                offset += remainingSpace/2;
            else // END
                offset += remainingSpace;
            child = iterator.first();
            while (child) {
                if (child->isPositioned()) {
                    child = iterator.next();
                    continue;
                }
                placeChild(child, child->xPos(), child->yPos()+offset);
                child = iterator.next();
            }
        }
    }
    
    // So that the calcHeight in layoutBlock() knows to relayout positioned objects because of
    // a height change, we revert our height back to the intrinsic height before returning.
    if (heightSpecified)
        m_height = oldHeight;    
}

void RenderFlexibleBox::placeChild(RenderObject* child, int x, int y)
{
    int oldChildX = child->xPos();
    int oldChildY = child->yPos();

    // Place the child.
    child->setPos(x, y);

    // If the child moved, we have to repaint it as well as any floating/positioned
    // descendants.  An exception is if we need a layout.  In this case, we know we're going to
    // repaint ourselves (and the child) anyway.
    if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
        child->repaintDuringLayoutIfMoved(oldChildX, oldChildY);
}

int RenderFlexibleBox::allowedChildFlex(RenderObject* child, bool expanding, unsigned int group)
{
    if (child->isPositioned() || child->style()->boxFlex() == 0.0f || child->style()->boxFlexGroup() != group)
        return 0;
                        
    if (expanding) {
        if (isHorizontal()) {
            // FIXME: For now just handle fixed values.
            int maxW = INT_MAX;
            int w = child->overrideWidth() - (child->borderLeft() + child->borderRight() + child->paddingLeft() + child->paddingRight());
            if (child->style()->maxWidth().value != UNDEFINED &&
                child->style()->maxWidth().isFixed())
                maxW = child->style()->maxWidth().value;
            else if (child->style()->maxWidth().type == Intrinsic)
                maxW = child->maxWidth();
            else if (child->style()->maxWidth().type == MinIntrinsic)
                maxW = child->minWidth();
            int allowedGrowth = kMax(0, maxW - w);
            return allowedGrowth;
        }
        else {
            // FIXME: For now just handle fixed values.
            int maxH = INT_MAX;
            int h = child->overrideHeight() - (child->borderTop() + child->borderBottom() + child->paddingTop() + child->paddingBottom());
            if (child->style()->maxHeight().value != UNDEFINED &&
                child->style()->maxHeight().isFixed())
                maxH = child->style()->maxHeight().value;
            int allowedGrowth = kMax(0, maxH - h);
            return allowedGrowth;
        }
    }

    // FIXME: For now just handle fixed values.
    if (isHorizontal()) {
        int minW = child->minWidth();
        int w = child->contentWidth();
        if (child->style()->minWidth().isFixed())
            minW = child->style()->minWidth().value;
        else if (child->style()->minWidth().type == Intrinsic)
            minW = child->maxWidth();
        else if (child->style()->minWidth().type == MinIntrinsic)
            minW = child->minWidth();
            
        int allowedShrinkage = kMin(0, minW - w);
        return allowedShrinkage;
    }
    else {
        if (child->style()->minHeight().isFixed()) {
            int minH = child->style()->minHeight().value;
            int h = child->contentHeight();
            int allowedShrinkage = kMin(0, minH - h);
            return allowedShrinkage;
        }
    }
    
    return 0;
}

const char *RenderFlexibleBox::renderName() const
{
    if (isFloating())
        return "RenderFlexibleBox (floating)";
    if (isPositioned())
        return "RenderFlexibleBox (positioned)";
    if (isRelPositioned())
        return "RenderFlexibleBox (relative positioned)";
    return "RenderFlexibleBox";
}


} // namespace khtml

