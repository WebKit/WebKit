/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderListBox.h"

#include "CSSStyleSelector.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HitTestResult.h"
#include "Page.h"
#include "PlatformScrollBar.h" 
#include "RenderTheme.h"
#include "RenderView.h"
#include "SelectionController.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;
 
const int rowSpacing = 1;

const int optionsSpacingHorizontal = 2;

const int minSize = 4;
const int maxDefaultSize = 10;

// FIXME: This hardcoded baselineAdjustment is what we used to do for the old
// widget, but I'm not sure this is right for the new control.
const int baselineAdjustment = 7;

RenderListBox::RenderListBox(HTMLSelectElement* element)
    : RenderBlock(element)
    , m_optionsChanged(true)
    , m_scrollToRevealSelectionAfterLayout(false)
    , m_inAutoscroll(false)
    , m_optionsWidth(0)
    , m_indexOffset(0)
{
}

RenderListBox::~RenderListBox()
{
    if (m_vBar) {
        if (m_vBar->isWidget()) {
            if (FrameView* view = node()->document()->view())
                view->removeChild(static_cast<PlatformScrollbar*>(m_vBar.get()));
        }
        m_vBar->setClient(0);
    }
}

void RenderListBox::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    setReplaced(isInline());
}

void RenderListBox::updateFromElement()
{
    if (m_optionsChanged) {
        const Vector<HTMLElement*>& listItems = static_cast<HTMLSelectElement*>(node())->listItems();
        int size = numItems();
        
        float width = 0;
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
                itemFont.update(document()->styleSelector()->fontSelector());
            }
                
            if (!text.isEmpty()) {
                float textWidth = itemFont.floatWidth(TextRun(text.impl(), 0, 0, 0, false, false, false, false));
                width = max(width, textWidth);
            }
        }
        m_optionsWidth = static_cast<int>(ceilf(width));
        m_optionsChanged = false;
        
        if (!m_vBar && Scrollbar::hasPlatformScrollbars())
            if (FrameView* view = node()->document()->view()) {
                RefPtr<PlatformScrollbar> widget = new PlatformScrollbar(this, VerticalScrollbar, SmallScrollbar);
                view->addChild(widget.get());
                m_vBar = widget.release();
            }
        
        setNeedsLayoutAndPrefWidthsRecalc();
    }
}

void RenderListBox::selectionChanged()
{
    repaint();
    if (!m_inAutoscroll) {
        if (m_optionsChanged || needsLayout())
            m_scrollToRevealSelectionAfterLayout = true;
        else
            scrollToRevealSelection();
    }
}

void RenderListBox::layout()
{
    RenderBlock::layout();
    if (m_scrollToRevealSelectionAfterLayout)
        scrollToRevealSelection();
}

void RenderListBox::scrollToRevealSelection()
{    
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());

    m_scrollToRevealSelectionAfterLayout = false;

    int firstIndex = select->activeSelectionStartListIndex();
    if (firstIndex >= 0 && !listIndexIsVisible(select->activeSelectionEndListIndex()))
        scrollToRevealElementAtListIndex(firstIndex);
}

void RenderListBox::calcPrefWidths()
{
    ASSERT(!m_optionsChanged);

    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else {
        m_maxPrefWidth = m_optionsWidth + 2 * optionsSpacingHorizontal;
        if (m_vBar)
            m_maxPrefWidth += m_vBar->width();
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;
                                
    setPrefWidthsDirty(false);
}

int RenderListBox::size() const
{
    int specifiedSize = static_cast<HTMLSelectElement*>(node())->size();
    if (specifiedSize > 1)
        return max(minSize, specifiedSize);
    return min(max(minSize, numItems()), maxDefaultSize);
}

int RenderListBox::numVisibleItems() const
{
    // Only count fully visible rows. But don't return 0 even if only part of a row shows.
    return max(1, (contentHeight() + rowSpacing) / itemHeight());
}

int RenderListBox::numItems() const
{
    return static_cast<HTMLSelectElement*>(node())->listItems().size();
}

int RenderListBox::listHeight() const
{
    return itemHeight() * numItems() - rowSpacing;
}

void RenderListBox::calcHeight()
{
    int toAdd = paddingTop() + paddingBottom() + borderTop() + borderBottom();
 
    int itemHeight = RenderListBox::itemHeight();
    m_height = itemHeight * size() - rowSpacing + toAdd;
    
    RenderBlock::calcHeight();
    
    if (m_vBar) {
        bool enabled = numVisibleItems() < numItems();
        m_vBar->setEnabled(enabled);
        m_vBar->setSteps(1, min(1, numVisibleItems() - 1), itemHeight);
        m_vBar->setProportion(numVisibleItems(), numItems());
        if (!enabled)
            m_indexOffset = 0;
    }
}

short RenderListBox::baselinePosition(bool b, bool isRootLineBox) const
{
    return height() + marginTop() + marginBottom() - baselineAdjustment;
}

IntRect RenderListBox::itemBoundingBoxRect(int tx, int ty, int index)
{
    return IntRect(tx + borderLeft() + paddingLeft(),
                   ty + borderTop() + paddingTop() + itemHeight() * (index - m_indexOffset),
                   contentWidth(), itemHeight());
}
    
void RenderListBox::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    if (style()->visibility() != VISIBLE)
        return;
    
    int listItemsSize = numItems();

    if (paintInfo.phase == PaintPhaseForeground) {
        int index = m_indexOffset;
        while (index < listItemsSize && index <= m_indexOffset + numVisibleItems()) {
            paintItemForeground(paintInfo, tx, ty, index);
            index++;
        }
    }

    // Paint the children.
    RenderBlock::paintObject(paintInfo, tx, ty);

    if (paintInfo.phase == PaintPhaseBlockBackground)
        paintScrollbar(paintInfo);
    else if (paintInfo.phase == PaintPhaseChildBlockBackground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        int index = m_indexOffset;
        while (index < listItemsSize && index <= m_indexOffset + numVisibleItems()) {
            paintItemBackground(paintInfo, tx, ty, index);
            index++;
        }
    }
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
       
    // Determine where the item text should be placed
    IntRect r = itemBoundingBoxRect(tx, ty, listIndex);
    r.move(optionsSpacingHorizontal, style()->font().ascent());

    RenderStyle* itemStyle = element->renderStyle();
    if (!itemStyle)
        itemStyle = style();
    
    Color textColor = element->renderStyle() ? element->renderStyle()->color() : style()->color();
    if (element->hasTagName(optionTag) && static_cast<HTMLOptionElement*>(element)->selected()) {
        if (document()->frame()->selectionController()->isFocusedAndActive() && document()->focusedNode() == node())
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
        itemFont.update(document()->styleSelector()->fontSelector());
    }
    paintInfo.context->setFont(itemFont);
    
    unsigned length = itemText.length();
    const UChar* string = itemText.characters();
    TextRun textRun(string, length, 0, 0, 0, itemStyle->direction() == RTL, itemStyle->unicodeBidi() == Override, false, false);

    // Draw the item text
    if (itemStyle->visibility() != HIDDEN)
        paintInfo.context->drawBidiText(textRun, r.location());
}

void RenderListBox::paintItemBackground(PaintInfo& paintInfo, int tx, int ty, int listIndex)
{
    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
    const Vector<HTMLElement*>& listItems = select->listItems();
    HTMLElement* element = listItems[listIndex];

    Color backColor;
    if (element->hasTagName(optionTag) && static_cast<HTMLOptionElement*>(element)->selected()) {
        if (document()->frame()->selectionController()->isFocusedAndActive() && document()->focusedNode() == node())
            backColor = theme()->activeListBoxSelectionBackgroundColor();
        else
            backColor = theme()->inactiveListBoxSelectionBackgroundColor();
    } else
        backColor = element->renderStyle() ? element->renderStyle()->backgroundColor() : style()->backgroundColor();

    // Draw the background for this list box item
    if (!element->renderStyle() || element->renderStyle()->visibility() != HIDDEN) {
        IntRect itemRect = itemBoundingBoxRect(tx, ty, listIndex);
        itemRect.intersect(controlClipRect(tx, ty));
        paintInfo.context->fillRect(itemRect, backColor);
    }
}

bool RenderListBox::isPointInOverflowControl(HitTestResult& result, int _x, int _y, int _tx, int _ty)
{
    if (!m_vBar)
        return false;

    IntRect vertRect(_tx + width() - borderRight() - m_vBar->width(),
                   _ty + borderTop() - borderTopExtra(),
                   m_vBar->width(),
                   height() + borderTopExtra() + borderBottomExtra() - borderTop() - borderBottom());

    if (vertRect.contains(_x, _y)) {
        result.setScrollbar(m_vBar->isWidget() ? static_cast<PlatformScrollbar*>(m_vBar.get()) : 0);
        return true;
    }
    return false;
}

int RenderListBox::listIndexAtOffset(int offsetX, int offsetY)
{
    if (!numItems())
        return -1;

    if (offsetY < borderTop() + paddingTop() || offsetY > height() - paddingBottom() - borderBottom())
        return -1;

    int scrollbarWidth = m_vBar ? m_vBar->width() : 0;
    if (offsetX < borderLeft() + paddingLeft() || offsetX > width() - borderRight() - paddingRight() - scrollbarWidth)
        return -1;

    int newOffset = (offsetY - borderTop() - paddingTop()) / itemHeight() + m_indexOffset;
    return newOffset < numItems() ? newOffset : -1;
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
    int rows = numVisibleItems();
    int offset = m_indexOffset;
    if (offsetY < borderTop() + paddingTop() && scrollToRevealElementAtListIndex(offset - 1))
        endIndex = offset - 1;
    else if (offsetY > height() - paddingBottom() - borderBottom() && scrollToRevealElementAtListIndex(offset + rows))
        endIndex = offset + rows - 1;
    else
        endIndex = listIndexAtOffset(offsetX, offsetY);

    if (endIndex >= 0) {
        HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node());
        m_inAutoscroll = true;
        if (!select->multiple())
            select->setActiveSelectionAnchorIndex(endIndex);
        select->setActiveSelectionEndIndex(endIndex);
        select->updateListBoxSelection(!select->multiple());
        m_inAutoscroll = false;
    }
}

void RenderListBox::stopAutoscroll()
{
    static_cast<HTMLSelectElement*>(node())->listBoxOnChange();
}

bool RenderListBox::scrollToRevealElementAtListIndex(int index)
{
    if (index < 0 || index >= numItems() || listIndexIsVisible(index))
        return false;

    int newOffset;
    if (index < m_indexOffset)
        newOffset = index;
    else
        newOffset = index - numVisibleItems() + 1;

    m_indexOffset = newOffset;
    if (m_vBar)
        m_vBar->setValue(m_indexOffset);

    return true;
}

bool RenderListBox::listIndexIsVisible(int index)
{    
    return index >= m_indexOffset && index < m_indexOffset + numVisibleItems();
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
    int newOffset = m_vBar->value();
    if (newOffset != m_indexOffset) {
        m_indexOffset = newOffset;
        repaint();
        // Fire the scroll DOM event.
        EventTargetNodeCast(node())->dispatchHTMLEvent(scrollEvent, true, false);
    }
}

int RenderListBox::itemHeight() const
{
    return style()->font().height() + rowSpacing;
}

int RenderListBox::verticalScrollbarWidth() const
{
    return m_vBar ? m_vBar->width() : 0;
}

// FIXME: We ignore padding in the vertical direction as far as these values are concerned, since that's
// how the control currently paints.
int RenderListBox::scrollWidth() const
{
    // There is no horizontal scrolling allowed.
    return clientWidth();
}

int RenderListBox::scrollHeight() const
{
    return max(clientHeight(), listHeight());
}

int RenderListBox::scrollLeft() const
{
    return 0;
}

void RenderListBox::setScrollLeft(int)
{
}

int RenderListBox::scrollTop() const
{
    return m_indexOffset * itemHeight();
}

void RenderListBox::setScrollTop(int newTop)
{
    // Determine an index and scroll to it.    
    int index = newTop / itemHeight();
    if (index < 0 || index >= numItems() || index == m_indexOffset)
        return;
    m_indexOffset = index;
    if (m_vBar)
        m_vBar->setValue(index);
}

IntRect RenderListBox::controlClipRect(int tx, int ty) const
{
    IntRect clipRect = contentBox();
    clipRect.move(tx, ty);
    return clipRect;
}

IntRect RenderListBox::windowClipRect() const
{
    FrameView* frameView = view()->frameView();
    if (!frameView)
        return IntRect();

    return frameView->windowClipRectForLayer(enclosingLayer(), true);
}

bool RenderListBox::isActive() const
{
    Page* page = document()->frame()->page();
    return page && page->focusController()->isActive();
}

bool RenderListBox::isScrollable() const
{
    if (numVisibleItems() < numItems())
        return true;
    return RenderObject::isScrollable();
}

} // namespace WebCore
