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
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "PlatformScrollBar.h" 
#include "RenderBR.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "TextStyle.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;
using namespace EventNames;
 
const int optionsSpacingMiddle = 1;
const int optionsSpacingLeft = 2;

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
    const Vector<HTMLElement*>& listItems = select->listItems();
    int size = listItems.size();
    if (m_optionsChanged) {  
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
    }
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
        m_maxWidth = m_optionsWidth;
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
    
void RenderListBox::paintObject(PaintInfo& i, int tx, int ty)
{
    // Push a clip.
    IntRect clipRect(tx + borderLeft(), ty + borderTop(),
         width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop());
    if (i.phase == PaintPhaseForeground || i.phase == PaintPhaseChildBlockBackgrounds) {
        if (clipRect.width() == 0 || clipRect.height() == 0)
            return;
        i.p->save();
        i.p->addClip(clipRect);
    }
    
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    int listItemsSize = select->listItems().size();

    if (i.phase == PaintPhaseForeground) {
        int index = m_indexOffset;
        while (index < listItemsSize && index < m_indexOffset + size()) {
            paintItemForeground(i, tx, ty, index);
            index++;
        }
    }
    
    // Paint the children.
    RenderBlock::paintObject(i, tx, ty);
    
    if (i.phase == PaintPhaseBlockBackground) {
        int index = m_indexOffset;
        while (index < listItemsSize && index < m_indexOffset + size()) {
            paintItemBackground(i, tx, ty, index);
            index++;
        }
        paintScrollbar(i);
    }
    // Pop the clip.
    if (i.phase == PaintPhaseForeground || i.phase == PaintPhaseChildBlockBackgrounds)
        i.p->restore();
}

void RenderListBox::paintScrollbar(PaintInfo& i)
{
    if (m_vBar) {
        IntRect absBounds = absoluteBoundingBoxRect();
        IntRect scrollRect(absBounds.right() - borderRight() - m_vBar->width(),
                absBounds.y() + borderTop(),
                m_vBar->width(),
                absBounds.height() - (borderTop() + borderBottom()));
        m_vBar->setRect(scrollRect);
        m_vBar->paint(i.p, scrollRect);
    }
}

void RenderListBox::paintItemForeground(PaintInfo& i, int tx, int ty, int listIndex)
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
    r.move(optionsSpacingLeft, style()->font().ascent());

    RenderStyle* itemStyle = element->renderStyle();
    if (!itemStyle)
        itemStyle = style();
    
    Color textColor = element->renderStyle() ? element->renderStyle()->color() : style()->color();
    if (element->hasTagName(optionTag) && static_cast<HTMLOptionElement*>(element)->selected()) {
        if (document()->frame()->isActive() && document()->focusNode() == node())
            textColor = theme()->activeListBoxSelectionForegroundColor();
 /*
    FIXME: Decide what the desired behavior is for inactive foreground color.  
    For example, disabled items have a dark grey foreground color defined in CSS.  
    If we don't honor that, the selected disabled items will have a weird black-on-grey look.
        else
            textColor = theme()->inactiveListBoxSelectionForegroundColor();
 */
          
    }
        
    i.p->setPen(textColor);
    
    Font itemFont = style()->font();
    if (element->hasTagName(optgroupTag)) {
        FontDescription d = itemFont.fontDescription();
        d.setBold(true);
        itemFont = Font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
        itemFont.update();
    }
    i.p->setFont(itemFont);
    
    // Draw the item text
    if (itemStyle->visibility() != HIDDEN)
        i.p->drawText(textRun, r.location());
}

void RenderListBox::paintItemBackground(PaintInfo& i, int tx, int ty, int listIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    HTMLElement* element = listItems[listIndex];

    Color backColor;
    if (element->hasTagName(optionTag) && static_cast<HTMLOptionElement*>(element)->selected()) {
        if (document()->frame()->isActive() && document()->focusNode() == node())
            backColor = theme()->activeListBoxSelectionBackgroundColor();
        else
            backColor = theme()->inactiveListBoxSelectionBackgroundColor();
    } else
        backColor = element->renderStyle() ? element->renderStyle()->backgroundColor() : style()->backgroundColor();

    // Draw the background for this list box item
    if (!element->renderStyle() || element->renderStyle()->visibility() != HIDDEN)
        i.p->fillRect(itemBoundingBoxRect(tx, ty, listIndex), backColor);
}

bool RenderListBox::isPointInScrollbar(NodeInfo& info, int _x, int _y, int _tx, int _ty)
{
    if (!m_vBar)
        return false;

    IntRect vertRect(_tx + width() - borderRight() - m_vBar->width(),
                   _ty + borderTop() - borderTopExtra(),
                   m_vBar->width(),
                   height() + borderTopExtra() + borderBottomExtra() - borderTop() - borderBottom());

    if (vertRect.contains(_x, _y)) {
        info.setScrollbar(m_vBar->isWidget() ? static_cast<PlatformScrollbar*>(m_vBar) : 0);
        return true;
    }
    return false;
}

HTMLOptionElement* RenderListBox::optionAtPoint(int x, int y)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    int yOffset = y - absoluteBoundingBoxRect().y();
    int newOffset = max(0, yOffset / (style()->font().height() + optionsSpacingMiddle)) + m_indexOffset;
    newOffset = max(0, min((int)listItems.size() - 1, newOffset));
    int scrollbarWidth = m_vBar ? m_vBar->width() : 0;
    if (x >= absoluteBoundingBoxRect().x() + borderLeft() + paddingLeft() && x < absoluteBoundingBoxRect().right() - borderRight() - paddingRight() - scrollbarWidth)
        return static_cast<HTMLOptionElement*>(listItems[newOffset]);
    return 0;
}

void RenderListBox::autoscroll()
{
    int mouseX = document()->frame()->view()->currentMousePosition().x();
    int mouseY = document()->frame()->view()->currentMousePosition().y();
    IntRect bounds = absoluteBoundingBoxRect();

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& items = select->listItems();
    HTMLOptionElement* element = 0;
    int rows = size();
    int offset = m_indexOffset;
    if (mouseY < bounds.y() && scrollToRevealElementAtListIndex(offset - 1) && items[offset - 1]->hasTagName(optionTag))
        element = static_cast<HTMLOptionElement*>(items[offset - 1]);
    else if (mouseY > bounds.bottom() && scrollToRevealElementAtListIndex(offset + rows) && items[offset + rows - 1]->hasTagName(optionTag))
        element = static_cast<HTMLOptionElement*>(items[offset + rows - 1]);
    else
        element = optionAtPoint(mouseX, mouseY);
        
    if (element) {
        select->setSelectedIndex(element->index(), !select->multiple());
        repaint();
    }
}

bool RenderListBox::scrollToRevealElementAtListIndex(int index)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    
    if (index < 0 || index > (int)listItems.size() - 1 || (index >= m_indexOffset && index < m_indexOffset + size()))
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
        //    printf("value changed: new offset index: %d\n", newOffset);
            repaint();
            // Fire the scroll DOM event.
            EventTargetNodeCast(node())->dispatchHTMLEvent(scrollEvent, true, false);
        }
    }
}

}
