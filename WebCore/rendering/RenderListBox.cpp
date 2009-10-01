/*
 * This file is part of the select element renderer in WebCore.
 *
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *               2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#include "AXObjectCache.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "OptionGroupElement.h"
#include "OptionElement.h"
#include "Page.h"
#include "RenderScrollbar.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Scrollbar.h"
#include "SelectElement.h"
#include "SelectionController.h"
#include "NodeRenderStyle.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;
 
const int rowSpacing = 1;

const int optionsSpacingHorizontal = 2;

const int minSize = 4;
const int maxDefaultSize = 10;

// FIXME: This hardcoded baselineAdjustment is what we used to do for the old
// widget, but I'm not sure this is right for the new control.
const int baselineAdjustment = 7;

RenderListBox::RenderListBox(Element* element)
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
    setHasVerticalScrollbar(false);
}

void RenderListBox::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    setReplaced(isInline());
}

void RenderListBox::updateFromElement()
{
    if (m_optionsChanged) {
        const Vector<Element*>& listItems = toSelectElement(static_cast<Element*>(node()))->listItems();
        int size = numItems();
        
        float width = 0;
        for (int i = 0; i < size; ++i) {
            Element* element = listItems[i];
            String text;
            Font itemFont = style()->font();
            if (OptionElement* optionElement = toOptionElement(element))
                text = optionElement->textIndentedToRespectGroupLabel();
            else if (OptionGroupElement* optionGroupElement = toOptionGroupElement(element)) {
                text = optionGroupElement->groupLabelText();
                FontDescription d = itemFont.fontDescription();
                d.setWeight(d.bolderWeight());
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
        
        setHasVerticalScrollbar(true);

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
    
    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->selectedChildrenChanged(this);
}

void RenderListBox::layout()
{
    RenderBlock::layout();
    if (m_scrollToRevealSelectionAfterLayout)
        scrollToRevealSelection();
}

void RenderListBox::scrollToRevealSelection()
{    
    SelectElement* select = toSelectElement(static_cast<Element*>(node()));

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
    int specifiedSize = toSelectElement(static_cast<Element*>(node()))->size();
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
    return toSelectElement(static_cast<Element*>(node()))->listItems().size();
}

int RenderListBox::listHeight() const
{
    return itemHeight() * numItems() - rowSpacing;
}

void RenderListBox::calcHeight()
{
    int toAdd = paddingTop() + paddingBottom() + borderTop() + borderBottom();
 
    int itemHeight = RenderListBox::itemHeight();
    setHeight(itemHeight * size() - rowSpacing + toAdd);
    
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

int RenderListBox::baselinePosition(bool, bool) const
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
        paintScrollbar(paintInfo, tx, ty);
    else if (paintInfo.phase == PaintPhaseChildBlockBackground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        int index = m_indexOffset;
        while (index < listItemsSize && index <= m_indexOffset + numVisibleItems()) {
            paintItemBackground(paintInfo, tx, ty, index);
            index++;
        }
    }
}

void RenderListBox::paintScrollbar(PaintInfo& paintInfo, int tx, int ty)
{
    if (m_vBar) {
        IntRect scrollRect(tx + width() - borderRight() - m_vBar->width(),
                           ty + borderTop(),
                           m_vBar->width(),
                           height() - (borderTop() + borderBottom()));
        m_vBar->setFrameRect(scrollRect);
        m_vBar->paint(paintInfo.context, paintInfo.rect);
    }
}

void RenderListBox::paintItemForeground(PaintInfo& paintInfo, int tx, int ty, int listIndex)
{
    SelectElement* select = toSelectElement(static_cast<Element*>(node()));
    const Vector<Element*>& listItems = select->listItems();
    Element* element = listItems[listIndex];
    OptionElement* optionElement = toOptionElement(element);

    String itemText;
    if (optionElement)
        itemText = optionElement->textIndentedToRespectGroupLabel();
    else if (OptionGroupElement* optionGroupElement = toOptionGroupElement(element))
        itemText = optionGroupElement->groupLabelText();      

    // Determine where the item text should be placed
    IntRect r = itemBoundingBoxRect(tx, ty, listIndex);
    r.move(optionsSpacingHorizontal, style()->font().ascent());

    RenderStyle* itemStyle = element->renderStyle();
    if (!itemStyle)
        itemStyle = style();
    
    Color textColor = element->renderStyle() ? element->renderStyle()->color() : style()->color();
    if (optionElement && optionElement->selected()) {
        if (document()->frame()->selection()->isFocusedAndActive() && document()->focusedNode() == node())
            textColor = theme()->activeListBoxSelectionForegroundColor();
        // Honor the foreground color for disabled items
        else if (!element->disabled())
            textColor = theme()->inactiveListBoxSelectionForegroundColor();
    }

    paintInfo.context->setFillColor(textColor);

    Font itemFont = style()->font();
    if (isOptionGroupElement(element)) {
        FontDescription d = itemFont.fontDescription();
        d.setWeight(d.bolderWeight());
        itemFont = Font(d, itemFont.letterSpacing(), itemFont.wordSpacing());
        itemFont.update(document()->styleSelector()->fontSelector());
    }

    unsigned length = itemText.length();
    const UChar* string = itemText.characters();
    TextRun textRun(string, length, 0, 0, 0, itemStyle->direction() == RTL, itemStyle->unicodeBidi() == Override, false, false);

    // Draw the item text
    if (itemStyle->visibility() != HIDDEN)
        paintInfo.context->drawBidiText(itemFont, textRun, r.location());
}

void RenderListBox::paintItemBackground(PaintInfo& paintInfo, int tx, int ty, int listIndex)
{
    SelectElement* select = toSelectElement(static_cast<Element*>(node()));
    const Vector<Element*>& listItems = select->listItems();
    Element* element = listItems[listIndex];
    OptionElement* optionElement = toOptionElement(element);

    Color backColor;
    if (optionElement && optionElement->selected()) {
        if (document()->frame()->selection()->isFocusedAndActive() && document()->focusedNode() == node())
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
                     _ty + borderTop(),
                     m_vBar->width(),
                     height() - borderTop() - borderBottom());

    if (vertRect.contains(_x, _y)) {
        result.setScrollbar(m_vBar.get());
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

void RenderListBox::panScroll(const IntPoint& panStartMousePosition)
{
    const int maxSpeed = 20;
    const int iconRadius = 7;
    const int speedReducer = 4;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absOffset = localToAbsolute();

    IntPoint currentMousePosition = document()->frame()->eventHandler()->currentMousePosition();
    // We need to check if the current mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (currentMousePosition.y() < 0)
        currentMousePosition = previousMousePosition;
    else
        previousMousePosition = currentMousePosition;

    int yDelta = currentMousePosition.y() - panStartMousePosition.y();

    // If the point is too far from the center we limit the speed
    yDelta = max(min(yDelta, maxSpeed), -maxSpeed);
    
    if (abs(yDelta) < iconRadius) // at the center we let the space for the icon
        return;

    if (yDelta > 0)
        //offsetY = view()->viewHeight();
        absOffset.move(0, listHeight());
    else if (yDelta < 0)
        yDelta--;

    // Let's attenuate the speed
    yDelta /= speedReducer;

    IntPoint scrollPoint(0, 0);
    scrollPoint.setY(absOffset.y() + yDelta);
    int newOffset = scrollToward(scrollPoint);
    if (newOffset < 0) 
        return;

    m_inAutoscroll = true;
    SelectElement* select = toSelectElement(static_cast<Element*>(node()));
    select->updateListBoxSelection(!select->multiple());
    m_inAutoscroll = false;
}

int RenderListBox::scrollToward(const IntPoint& destination)
{
    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = localToAbsolute();
    int offsetX = destination.x() - absPos.x();
    int offsetY = destination.y() - absPos.y();

    int rows = numVisibleItems();
    int offset = m_indexOffset;
    
    if (offsetY < borderTop() + paddingTop() && scrollToRevealElementAtListIndex(offset - 1))
        return offset - 1;
    
    if (offsetY > height() - paddingBottom() - borderBottom() && scrollToRevealElementAtListIndex(offset + rows))
        return offset + rows - 1;
    
    return listIndexAtOffset(offsetX, offsetY);
}

void RenderListBox::autoscroll()
{
    IntPoint pos = document()->frame()->view()->windowToContents(document()->frame()->eventHandler()->currentMousePosition());

    int endIndex = scrollToward(pos);
    if (endIndex >= 0) {
        SelectElement* select = toSelectElement(static_cast<Element*>(node()));
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
    toSelectElement(static_cast<Element*>(node()))->listBoxOnChange();
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

bool RenderListBox::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier, Node**)
{
    return m_vBar && m_vBar->scroll(direction, granularity, multiplier);
}

void RenderListBox::valueChanged(unsigned listIndex)
{
    Element* element = static_cast<Element*>(node());
    SelectElement* select = toSelectElement(element);
    select->setSelectedIndex(select->listToOptionIndex(listIndex));
    element->dispatchFormControlChangeEvent();
}

void RenderListBox::valueChanged(Scrollbar*)
{
    int newOffset = m_vBar->value();
    if (newOffset != m_indexOffset) {
        m_indexOffset = newOffset;
        repaint();
        node()->dispatchEvent(Event::create(eventNames().scrollEvent, false, false));
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

bool RenderListBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    if (!RenderBlock::nodeAtPoint(request, result, x, y, tx, ty, hitTestAction))
        return false;
    const Vector<Element*>& listItems = toSelectElement(static_cast<Element*>(node()))->listItems();
    int size = numItems();
    tx += this->x();
    ty += this->y();
    for (int i = 0; i < size; ++i) {
        if (itemBoundingBoxRect(tx, ty, i).contains(x, y)) {
            if (Element* node = listItems[i]) {
                result.setInnerNode(node);
                if (!result.innerNonSharedNode())
                    result.setInnerNonSharedNode(node);
                result.setLocalPoint(IntPoint(x - tx, y - ty));
                break;
            }
        }
    }

    return true;
}

IntRect RenderListBox::controlClipRect(int tx, int ty) const
{
    IntRect clipRect = contentBoxRect();
    clipRect.move(tx, ty);
    return clipRect;
}

bool RenderListBox::isActive() const
{
    Page* page = document()->frame()->page();
    return page && page->focusController()->isActive();
}

void RenderListBox::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    IntRect scrollRect = rect;
    scrollRect.move(width() - borderRight() - scrollbar->width(), borderTop());
    repaintRectangle(scrollRect);
}

IntRect RenderListBox::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    RenderView* view = this->view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    rect.move(scrollbarLeft, scrollbarTop);

    return view->frameView()->convertFromRenderer(this, rect);
}

IntRect RenderListBox::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    RenderView* view = this->view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToRenderer(this, parentRect);

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    rect.move(-scrollbarLeft, -scrollbarTop);
    return rect;
}

IntPoint RenderListBox::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    RenderView* view = this->view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    point.move(scrollbarLeft, scrollbarTop);

    return view->frameView()->convertFromRenderer(this, point);
}

IntPoint RenderListBox::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    RenderView* view = this->view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToRenderer(this, parentPoint);

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    point.move(-scrollbarLeft, -scrollbarTop);
    return point;
}

PassRefPtr<Scrollbar> RenderListBox::createScrollbar()
{
    RefPtr<Scrollbar> widget;
    bool hasCustomScrollbarStyle = style()->hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(this, VerticalScrollbar, this);
    else
        widget = Scrollbar::createNativeScrollbar(this, VerticalScrollbar, theme()->scrollbarControlSizeForPart(ListboxPart));
    document()->view()->addChild(widget.get());        
    return widget.release();
}

void RenderListBox::destroyScrollbar()
{
    if (!m_vBar)
        return;
    
    m_vBar->removeFromParent();
    m_vBar->setClient(0);
    m_vBar = 0;
}

void RenderListBox::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == (m_vBar != 0))
        return;

    if (hasScrollbar)
        m_vBar = createScrollbar();
    else
        destroyScrollbar();

    if (m_vBar)
        m_vBar->styleChanged();

#if ENABLE(DASHBOARD_SUPPORT)
    // Force an update since we know the scrollbars have changed things.
    if (document()->hasDashboardRegions())
        document()->setDashboardRegionsDirty(true);
#endif
}

} // namespace WebCore
