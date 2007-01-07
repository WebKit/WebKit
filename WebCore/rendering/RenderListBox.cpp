/**
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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
#include "RenderListBox.h"

#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HitTestResult.h"
#include "PlatformScrollBar.h" 
#include "RenderBR.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "TextStyle.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;
 
const int optionsSpacingMiddle = 1;
const int optionsSpacingHorizontal = 2;

RenderListBox::RenderListBox(HTMLSelectElement* element)
    : RenderBlock(element)
    , m_optionsChanged(true)
    , m_optionsWidth(0)
    , m_indexOffset(0)
    , m_selectionChanged(true)
    , m_vBar(0)
{
}

RenderListBox::~RenderListBox()
{
    if (m_vBar && m_vBar->isWidget()) {
        element()->document()->view()->removeChild(static_cast<PlatformScrollbar*>(m_vBar));
        m_vBar->deref();
    }
}


void RenderListBox::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    setReplaced(isInline());
}

void RenderListBox::updateFromElement()
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());

    if (m_optionsChanged) {
        const Vector<HTMLElement*>& listItems = select->listItems();
        int size = listItems.size();
        
        float width = 0;
        TextStyle textStyle(0, 0, 0, false, false, false, false);
        for (int i = 0; i < size; ++i) {
            HTMLElement* element = listItems[i];
            String text;
            Font itemFont = style()->font();
            if (element->hasTagName(optionTag))
                text = static_cast<HTMLOptionElement*>(element)->optionText();
            else if (element->hasTagName(optgroupTag)) {
                text = static_cast<HTMLOptGroupElement*>(element)->groupLabelText();
                FontDescription d = itemFont.fontDescription();
                d.setBold(true);
                itemFont = Font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
                itemFont.update();
            }
                
            if (!text.isEmpty()) {
                float textWidth = itemFont.floatWidth(TextRun(text.impl()), textStyle);
                width = max(width, textWidth);
            }
        }
        m_optionsWidth = static_cast<int>(ceilf(width));
        m_optionsChanged = false;
        setNeedsLayoutAndMinMaxRecalc();
    }
    
    int firstIndex = select->optionToListIndex(select->selectedIndex());
    int lastIndex = select->lastSelectedListIndex();
    if (firstIndex >= 0 && !listIndexIsVisible(firstIndex) && !listIndexIsVisible(lastIndex))
        scrollToRevealElementAtListIndex(firstIndex);
}

void RenderListBox::calcMinMaxWidth()
{
    if (!m_vBar) {
        if (Scrollbar::hasPlatformScrollbars()) {
            PlatformScrollbar* widget = new PlatformScrollbar(this, VerticalScrollbar, SmallScrollbar);
            widget->ref();
            node()->document()->view()->addChild(widget);
            m_vBar = widget;
        }
    }
    if (m_optionsChanged)
        updateFromElement();

    m_minWidth = 0;
    m_maxWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value());
    else {
        m_maxWidth = m_optionsWidth + 2 * optionsSpacingHorizontal;
        if (m_vBar)
           m_maxWidth += m_vBar->width();
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxWidth = max(m_maxWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minWidth = max(m_minWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minWidth = 0;
    else
        m_minWidth = m_maxWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxWidth = min(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minWidth = min(m_minWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minWidth += toAdd;
    m_maxWidth += toAdd;
                                
    setMinMaxKnown();
}

const int minSize = 4;
const int minDefaultSize = 10;
int RenderListBox::size() const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    int specifiedSize = select->size();
    if (specifiedSize > 1)
        return max(minSize, specifiedSize);

    return min(max(numItems(), minSize), minDefaultSize);
}

int RenderListBox::numItems() const
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    return select->listItems().size();
}

void RenderListBox::calcHeight()
{
    int toAdd = paddingTop() + paddingBottom() + borderTop() + borderBottom();

    int itemHeight = style()->font().height() + optionsSpacingMiddle;
    m_height = itemHeight * size() + toAdd;
    
    RenderBlock::calcHeight();
    
    if (m_vBar) {
        m_vBar->setEnabled(size() < numItems());
        m_vBar->setSteps(itemHeight, itemHeight);
        m_vBar->setProportion(m_height - toAdd, itemHeight * numItems());
    }
}

const int baselineAdjustment = 7;
short RenderListBox::baselinePosition(bool b, bool isRootLineBox) const
{
    // FIXME: This hardcoded baselineAdjustment is what we used to do for the old widget, but I'm not sure this is right for the new control.
    return height() + marginTop() + marginBottom() - baselineAdjustment;
}

IntRect RenderListBox::itemBoundingBoxRect(int tx, int ty, int index)
{
    return IntRect (tx + borderLeft() + paddingLeft(),
                   ty + borderTop() + paddingTop() + ((style()->font().height() + optionsSpacingMiddle) * (index - m_indexOffset)),
                   absoluteBoundingBoxRect().width() - borderLeft() - borderRight() - paddingLeft() - paddingRight(),
                   style()->font().height() + optionsSpacingMiddle);
}
    
void RenderListBox::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    // Push a clip.
    IntRect clipRect(tx + borderLeft(), ty + borderTop(),
         width() - borderLeft() - borderRight() - (m_vBar ? m_vBar->width() : 0), height() - borderBottom() - borderTop());
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        if (clipRect.width() == 0 || clipRect.height() == 0)
            return;
        paintInfo.context->save();
        paintInfo.context->clip(clipRect);
    }
    
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    int listItemsSize = select->listItems().size();

    if (paintInfo.phase == PaintPhaseForeground) {
        int index = m_indexOffset;
        while (index < listItemsSize && index < m_indexOffset + size()) {
            paintItemForeground(paintInfo, tx, ty, index);
            index++;
        }
    }

    // Paint the children.
    RenderBlock::paintObject(paintInfo, tx, ty);

    if (paintInfo.phase == PaintPhaseBlockBackground) {
        int index = m_indexOffset;
        while (index < listItemsSize && index < m_indexOffset + size()) {
            paintItemBackground(paintInfo, tx, ty, index);
            index++;
        }
        paintScrollbar(paintInfo);
    }
    // Pop the clip.
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds)
        paintInfo.context->restore();
}

void RenderListBox::paintScrollbar(PaintInfo& paintInfo)
{
    if (m_vBar) {
        IntRect absBounds = absoluteBoundingBoxRect();
        IntRect scrollRect(absBounds.right() - borderRight() - m_vBar->width(),
                           absBounds.y() + borderTop(),
                           m_vBar->width(),
                           absBounds.height() - (borderTop() + borderBottom()));
        m_vBar->setRect(scrollRect);
        m_vBar->paint(paintInfo.context, scrollRect);
    }
}

void RenderListBox::paintItemForeground(PaintInfo& paintInfo, int tx, int ty, int listIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    HTMLElement* element = listItems[listIndex];

    String itemText;
    if (element->hasTagName(optionTag))
        itemText = static_cast<HTMLOptionElement*>(element)->optionText();
    else if (element->hasTagName(optgroupTag))
        itemText = static_cast<HTMLOptGroupElement*>(element)->groupLabelText();
   
    TextRun textRun(itemText.characters(), itemText.length());
    
    // Determine where the item text should be placed
    IntRect r = itemBoundingBoxRect(tx, ty, listIndex);
    r.move(optionsSpacingHorizontal, style()->font().ascent());

    RenderStyle* itemStyle = element->renderStyle();
    if (!itemStyle)
        itemStyle = style();
    
    Color textColor = element->renderStyle() ? element->renderStyle()->color() : style()->color();
    if (element->hasTagName(optionTag) && static_cast<HTMLOptionElement*>(element)->selected()) {
        if (document()->frame()->isActive() && document()->focusedNode() == node())
            textColor = theme()->activeListBoxSelectionForegroundColor();
        // Honor the foreground color for disabled items
        else if (!element->disabled())
            textColor = theme()->inactiveListBoxSelectionForegroundColor();
    }

    paintInfo.context->setFillColor(textColor);

    Font itemFont = style()->font();
    if (element->hasTagName(optgroupTag)) {
        FontDescription d = itemFont.fontDescription();
        d.setBold(true);
        itemFont = Font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
        itemFont.update();
    }
    paintInfo.context->setFont(itemFont);
    
    // Draw the item text
    if (itemStyle->visibility() != HIDDEN)
        paintInfo.context->drawText(textRun, r.location());
}

void RenderListBox::paintItemBackground(PaintInfo& paintInfo, int tx, int ty, int listIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    HTMLElement* element = listItems[listIndex];

    Color backColor;
    if (element->hasTagName(optionTag) && static_cast<HTMLOptionElement*>(element)->selected()) {
        if (document()->frame()->isActive() && document()->focusedNode() == node())
            backColor = theme()->activeListBoxSelectionBackgroundColor();
        else
            backColor = theme()->inactiveListBoxSelectionBackgroundColor();
    } else
        backColor = element->renderStyle() ? element->renderStyle()->backgroundColor() : style()->backgroundColor();

    // Draw the background for this list box item
    if (!element->renderStyle() || element->renderStyle()->visibility() != HIDDEN)
        paintInfo.context->fillRect(itemBoundingBoxRect(tx, ty, listIndex), backColor);
}

bool RenderListBox::isPointInScrollbar(HitTestResult& result, int _x, int _y, int _tx, int _ty)
{
    if (!m_vBar)
        return false;

    IntRect vertRect(_tx + width() - borderRight() - m_vBar->width(),
                   _ty + borderTop() - borderTopExtra(),
                   m_vBar->width(),
                   height() + borderTopExtra() + borderBottomExtra() - borderTop() - borderBottom());

    if (vertRect.contains(_x, _y)) {
        result.setScrollbar(m_vBar->isWidget() ? static_cast<PlatformScrollbar*>(m_vBar) : 0);
        return true;
    }
    return false;
}

int RenderListBox::listIndexAtOffset(int offsetX, int offsetY)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();

    if ((int)listItems.size() > 0) {
        int newOffset = max(0, offsetY / (style()->font().height() + optionsSpacingMiddle)) + m_indexOffset;
        newOffset = max(0, min((int)listItems.size() - 1, newOffset));
        int scrollbarWidth = m_vBar ? m_vBar->width() : 0;
        if (offsetX >= borderLeft() + paddingLeft() && offsetX < absoluteBoundingBoxRect().width() - borderRight() - paddingRight() - scrollbarWidth)
            return newOffset;
    }
            
    return -1;
}

void RenderListBox::autoscroll()
{
    IntPoint pos = document()->frame()->view()->windowToContents(document()->frame()->eventHandler()->currentMousePosition());

    int rx = 0;
    int ry = 0;
    absolutePosition(rx, ry);
    int offsetX = pos.x() - rx;
    int offsetY = pos.y() - ry;
    
    int endIndex = -1;
    int rows = size();
    int offset = m_indexOffset;
    if (offsetY <  0 && scrollToRevealElementAtListIndex(offset - 1))
        endIndex = offset - 1;
    else if (offsetY > absoluteBoundingBoxRect().height() && scrollToRevealElementAtListIndex(offset + rows))
        endIndex = offset + rows - 1;
    else
        endIndex = listIndexAtOffset(offsetX, offsetY);

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    if (endIndex >= 0 && select) {
        if (!select->multiple())
            select->setActiveSelectionAnchorIndex(endIndex);
        select->setActiveSelectionEndIndex(endIndex);
        select->updateListBoxSelection(!select->multiple());
        repaint();
    }
}

void RenderListBox::stopAutoscroll()
{
    if ( HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node()))
        select->listBoxOnChange();
}

bool RenderListBox::scrollToRevealElementAtListIndex(int index)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    
    if (index < 0 || index > (int)listItems.size() - 1 || listIndexIsVisible(index))
        return false;

    int newOffset;
    if (index < m_indexOffset)
        newOffset = index;
    else
        newOffset = index - size() + 1;

    if (m_vBar) {
        IntRect rect = absoluteBoundingBoxRect();
        m_vBar->setValue(itemBoundingBoxRect(rect.x(), rect.y(), newOffset + m_indexOffset).y() - rect.y());
    }
    m_indexOffset = newOffset;
    
    return true;
}

bool RenderListBox::listIndexIsVisible(int index)
{    
    return index >= m_indexOffset && index < m_indexOffset + size();
}

bool RenderListBox::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    return m_vBar && m_vBar->scroll(direction, granularity, multiplier);
}

void RenderListBox::valueChanged(unsigned listIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    select->setSelectedIndex(select->listToOptionIndex(listIndex));
    select->onChange();
}

void RenderListBox::valueChanged(Scrollbar*)
{
    if (m_vBar) {
        int newOffset = max(0, m_vBar->value() / (style()->font().height() + optionsSpacingMiddle));
        if (newOffset != m_indexOffset) {
            m_indexOffset = newOffset;
            repaint();
            // Fire the scroll DOM event.
            EventTargetNodeCast(node())->dispatchHTMLEvent(scrollEvent, true, false);
        }
    }
}

IntRect RenderListBox::windowClipRect() const
{
    return view()->frameView()->windowClipRectForLayer(enclosingLayer(), true);
}

} // namespace WebCore
