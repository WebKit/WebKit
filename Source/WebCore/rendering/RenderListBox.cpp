/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "CSSFontSelector.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "HitTestResult.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLayoutState.h"
#include "RenderScrollbar.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "ScrollAnimator.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "Settings.h"
#include "SpatialNavigation.h"
#include "StyleResolver.h"
#include "StyleTreeResolver.h"
#include "WheelEventTestMonitor.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderListBox);
 
const int rowSpacing = 1;

const int optionsSpacingHorizontal = 2;

// The minSize constant was originally defined to render scrollbars correctly.
// This might vary for different platforms.
const int minSize = 4;

// Default size when the multiple attribute is present but size attribute is absent.
const int defaultSize = 4;

// FIXME: This hardcoded baselineAdjustment is what we used to do for the old
// widget, but I'm not sure this is right for the new control.
const int baselineAdjustment = 7;

RenderListBox::RenderListBox(HTMLSelectElement& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
    , m_optionsChanged(true)
    , m_scrollToRevealSelectionAfterLayout(false)
    , m_inAutoscroll(false)
    , m_optionsWidth(0)
    , m_indexOffset(0)
{
    view().frameView().addScrollableArea(this);
}

RenderListBox::~RenderListBox()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderListBox::willBeDestroyed()
{
    setHasVerticalScrollbar(false);
    view().frameView().removeScrollableArea(this);
    RenderBlockFlow::willBeDestroyed();
}

HTMLSelectElement& RenderListBox::selectElement() const
{
    return downcast<HTMLSelectElement>(nodeForNonAnonymous());
}

static FontCascade bolder(Document& document, const FontCascade& font)
{
    auto description = font.fontDescription();
    description.setWeight(description.bolderWeight());
    auto result = FontCascade { WTFMove(description), font.letterSpacing(), font.wordSpacing() };
    result.update(&document.fontSelector());
    return result;
}

void RenderListBox::updateFromElement()
{
    if (m_optionsChanged) {
        float width = 0;
        auto& normalFont = style().fontCascade();
        std::optional<FontCascade> boldFont;
        for (auto* element : selectElement().listItems()) {
            String text;
            WTF::Function<const FontCascade&()> selectFont = [&normalFont] () -> const FontCascade& {
                return normalFont;
            };
            if (is<HTMLOptionElement>(*element))
                text = downcast<HTMLOptionElement>(*element).textIndentedToRespectGroupLabel();
            else if (is<HTMLOptGroupElement>(*element)) {
                text = downcast<HTMLOptGroupElement>(*element).groupLabelText();
                selectFont = [this, &normalFont, &boldFont] () -> const FontCascade& {
                    if (!boldFont)
                        boldFont = bolder(document(), normalFont);
                    return boldFont.value();
                };
            }
            if (text.isEmpty())
                continue;
            text = applyTextTransform(style(), text, ' ');
            auto textRun = constructTextRun(text, style(), AllowRightExpansion);
            width = std::max(width, selectFont().width(textRun));
        }
        // FIXME: Is ceiling right here, or should we be doing some kind of rounding instead?
        m_optionsWidth = static_cast<int>(std::ceil(width));
        m_optionsChanged = false;

        setHasVerticalScrollbar(true);

        computeFirstIndexesVisibleInPaddingTopBottomAreas();

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
    
    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->deferSelectedChildrenChangedIfNeeded(selectElement());
}

void RenderListBox::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    RenderBlockFlow::layout();

    if (m_vBar) {
        bool enabled = numVisibleItems() < numItems();
        m_vBar->setEnabled(enabled);
        m_vBar->setSteps(1, std::max(1, numVisibleItems() - 1), itemHeight());
        m_vBar->setProportion(numVisibleItems(), numItems());
        if (!enabled) {
            scrollToOffsetWithoutAnimation(VerticalScrollbar, 0);
            m_indexOffset = 0;
        }
    }

    if (m_scrollToRevealSelectionAfterLayout) {
        LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext());
        scrollToRevealSelection();
    }
}

void RenderListBox::scrollToRevealSelection()
{    
    m_scrollToRevealSelectionAfterLayout = false;

    int firstIndex = selectElement().activeSelectionStartListIndex();
    if (firstIndex >= 0 && !listIndexIsVisible(selectElement().activeSelectionEndListIndex()))
        scrollToRevealElementAtListIndex(firstIndex);
}

void RenderListBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    maxLogicalWidth = shouldApplySizeContainment(*this) ? 2 * optionsSpacingHorizontal : m_optionsWidth + 2 * optionsSpacingHorizontal;
    if (m_vBar)
        maxLogicalWidth += m_vBar->width();
    if (!style().width().isPercentOrCalculated())
        minLogicalWidth = maxLogicalWidth;
}

void RenderListBox::computePreferredLogicalWidths()
{
    // Nested style recal do not fire post recal callbacks. see webkit.org/b/153767
    ASSERT(!m_optionsChanged || Style::postResolutionCallbacksAreSuspended());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style().width().isFixed() && style().width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().width());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().minWidth(), style().maxWidth(), horizontalBorderAndPaddingExtent());

    setPreferredLogicalWidthsDirty(false);
}

int RenderListBox::size() const
{
    int specifiedSize = selectElement().size();
    if (specifiedSize > 1)
        return std::max(minSize, specifiedSize);

    return defaultSize;
}

int RenderListBox::numVisibleItems(ConsiderPadding considerPadding) const
{
    // Only count fully visible rows. But don't return 0 even if only part of a row shows.
    int visibleItemsExcludingPadding = std::max<int>(1, (contentHeight() + rowSpacing) / itemHeight());
    if (considerPadding == ConsiderPadding::No)
        return visibleItemsExcludingPadding;

    return numberOfVisibleItemsInPaddingTop() + visibleItemsExcludingPadding + numberOfVisibleItemsInPaddingBottom();
}

int RenderListBox::numItems() const
{
    return selectElement().listItems().size();
}

LayoutUnit RenderListBox::listHeight() const
{
    return itemHeight() * numItems() - rowSpacing;
}

RenderBox::LogicalExtentComputedValues RenderListBox::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop) const
{
    LayoutUnit height = itemHeight() * size() - rowSpacing;
    cacheIntrinsicContentLogicalHeightForFlexItem(height);
    height += verticalBorderAndPaddingExtent();
    return RenderBox::computeLogicalHeight(height, logicalTop);
}

LayoutUnit RenderListBox::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode lineDirection, LinePositionMode linePositionMode) const
{
    auto baseline = RenderBox::baselinePosition(baselineType, firstLine, lineDirection, linePositionMode);
    if (!shouldApplyLayoutContainment(*this))
        baseline -= baselineAdjustment;
    return baseline;
}

LayoutRect RenderListBox::itemBoundingBoxRect(const LayoutPoint& additionalOffset, int index)
{
    LayoutUnit x = additionalOffset.x() + borderLeft() + paddingLeft();
    if (shouldPlaceVerticalScrollbarOnLeft() && m_vBar)
        x += m_vBar->occupiedWidth();
    LayoutUnit y = additionalOffset.y() + borderTop() + paddingTop() + itemHeight() * (index - m_indexOffset);
    return LayoutRect(x, y, contentWidth(), itemHeight());
}

void RenderListBox::paintItem(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const PaintFunction& paintFunction)
{
    int listItemsSize = numItems();
    int firstVisibleItem = m_indexOfFirstVisibleItemInsidePaddingTopArea.value_or(m_indexOffset);
    int endIndex = firstVisibleItem + numVisibleItems(ConsiderPadding::Yes);
    for (int i = firstVisibleItem; i < listItemsSize && i < endIndex; ++i)
        paintFunction(paintInfo, paintOffset, i);
}

void RenderListBox::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().visibility() != Visibility::Visible)
        return;
    
    if (paintInfo.phase == PaintPhase::Foreground) {
        paintItem(paintInfo, paintOffset, [this](PaintInfo& paintInfo, const LayoutPoint& paintOffset, int listItemIndex) {
            paintItemForeground(paintInfo, paintOffset, listItemIndex);
        });
    }

    // Paint the children.
    RenderBlockFlow::paintObject(paintInfo, paintOffset);

    switch (paintInfo.phase) {
    // Depending on whether we have overlay scrollbars they
    // get rendered in the foreground or background phases
    case PaintPhase::Foreground:
        if (m_vBar->isOverlayScrollbar())
            paintScrollbar(paintInfo, paintOffset);
        break;
    case PaintPhase::BlockBackground:
        if (!m_vBar->isOverlayScrollbar())
            paintScrollbar(paintInfo, paintOffset);
        break;
    case PaintPhase::ChildBlockBackground:
    case PaintPhase::ChildBlockBackgrounds: {
        paintItem(paintInfo, paintOffset, [this](PaintInfo& paintInfo, const LayoutPoint& paintOffset, int listItemIndex) {
            paintItemBackground(paintInfo, paintOffset, listItemIndex);
        });
        break;
    }
    default:
        break;
    }
}

void RenderListBox::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer)
{
    if (!selectElement().allowsNonContiguousSelection())
        return RenderBlockFlow::addFocusRingRects(rects, additionalOffset, paintContainer);

    // Focus the last selected item.
    int selectedItem = selectElement().activeSelectionEndListIndex();
    if (selectedItem >= 0) {
        rects.append(snappedIntRect(itemBoundingBoxRect(additionalOffset, selectedItem)));
        return;
    }

    // No selected items, find the first non-disabled item.
    int size = numItems();
    const Vector<HTMLElement*>& listItems = selectElement().listItems();
    for (int i = 0; i < size; ++i) {
        HTMLElement* element = listItems[i];
        if (is<HTMLOptionElement>(*element) && !element->isDisabledFormControl()) {
            selectElement().setActiveSelectionEndIndex(i);
            rects.append(itemBoundingBoxRect(additionalOffset, i));
            return;
        }
    }
}

void RenderListBox::paintScrollbar(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!m_vBar)
        return;

    LayoutUnit left = paintOffset.x() + (shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : width() - borderRight() - m_vBar->width());
    LayoutUnit top = paintOffset.y() + borderTop();
    LayoutUnit width = m_vBar->width();
    LayoutUnit height = this->height() - (borderTop() + borderBottom());
    IntRect scrollRect = snappedIntRect(left, top, width, height);
    m_vBar->setFrameRect(scrollRect);
    m_vBar->paint(paintInfo.context(), snappedIntRect(paintInfo.rect));
}

static LayoutSize itemOffsetForAlignment(TextRun textRun, const RenderStyle* itemStyle, FontCascade itemFont, LayoutRect itemBoudingBox)
{
    TextAlignMode actualAlignment = itemStyle->textAlign();
    // FIXME: Firefox doesn't respect TextAlignMode::Justify. Should we?
    // FIXME: Handle TextAlignMode::End here
    if (actualAlignment == TextAlignMode::Start || actualAlignment == TextAlignMode::Justify)
        actualAlignment = itemStyle->isLeftToRightDirection() ? TextAlignMode::Left : TextAlignMode::Right;

    LayoutSize offset = LayoutSize(0, itemFont.fontMetrics().ascent());
    if (actualAlignment == TextAlignMode::Right || actualAlignment == TextAlignMode::WebKitRight) {
        float textWidth = itemFont.width(textRun);
        offset.setWidth(itemBoudingBox.width() - textWidth - optionsSpacingHorizontal);
    } else if (actualAlignment == TextAlignMode::Center || actualAlignment == TextAlignMode::WebKitCenter) {
        float textWidth = itemFont.width(textRun);
        offset.setWidth((itemBoudingBox.width() - textWidth) / 2);
    } else
        offset.setWidth(optionsSpacingHorizontal);
    return offset;
}

void RenderListBox::paintItemForeground(PaintInfo& paintInfo, const LayoutPoint& paintOffset, int listIndex)
{
    const Vector<HTMLElement*>& listItems = selectElement().listItems();
    HTMLElement* listItemElement = listItems[listIndex];

    auto& itemStyle = *listItemElement->computedStyle();

    if (itemStyle.visibility() == Visibility::Hidden)
        return;

    String itemText;
    bool isOptionElement = is<HTMLOptionElement>(*listItemElement);
    if (isOptionElement)
        itemText = downcast<HTMLOptionElement>(*listItemElement).textIndentedToRespectGroupLabel();
    else if (is<HTMLOptGroupElement>(*listItemElement))
        itemText = downcast<HTMLOptGroupElement>(*listItemElement).groupLabelText();
    itemText = applyTextTransform(style(), itemText, ' ');

    if (itemText.isNull())
        return;

    Color textColor = itemStyle.visitedDependentColorWithColorFilter(CSSPropertyColor);
    if (isOptionElement && downcast<HTMLOptionElement>(*listItemElement).selected()) {
        if (frame().selection().isFocusedAndActive() && document().focusedElement() == &selectElement())
            textColor = theme().activeListBoxSelectionForegroundColor(styleColorOptions());
        // Honor the foreground color for disabled items
        else if (!listItemElement->isDisabledFormControl() && !selectElement().isDisabledFormControl())
            textColor = theme().inactiveListBoxSelectionForegroundColor(styleColorOptions());
    }

    paintInfo.context().setFillColor(textColor);

    TextRun textRun(itemText, 0, 0, AllowRightExpansion, itemStyle.direction(), isOverride(itemStyle.unicodeBidi()), true);
    FontCascade itemFont = style().fontCascade();
    LayoutRect r = itemBoundingBoxRect(paintOffset, listIndex);
    r.move(itemOffsetForAlignment(textRun, &itemStyle, itemFont, r));

    if (is<HTMLOptGroupElement>(*listItemElement)) {
        auto description = itemFont.fontDescription();
        description.setWeight(description.bolderWeight());
        itemFont = FontCascade(WTFMove(description), itemFont.letterSpacing(), itemFont.wordSpacing());
        itemFont.update(&document().fontSelector());
    }

    // Draw the item text
    paintInfo.context().drawBidiText(itemFont, textRun, roundedIntPoint(r.location()));
}

void RenderListBox::paintItemBackground(PaintInfo& paintInfo, const LayoutPoint& paintOffset, int listIndex)
{
    const Vector<HTMLElement*>& listItems = selectElement().listItems();
    HTMLElement* listItemElement = listItems[listIndex];
    auto& itemStyle = *listItemElement->computedStyle();

    Color backColor;
    if (is<HTMLOptionElement>(*listItemElement) && downcast<HTMLOptionElement>(*listItemElement).selected()) {
        if (frame().selection().isFocusedAndActive() && document().focusedElement() == &selectElement())
            backColor = theme().activeListBoxSelectionBackgroundColor(styleColorOptions());
        else
            backColor = theme().inactiveListBoxSelectionBackgroundColor(styleColorOptions());
    } else
        backColor = itemStyle.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

    // Draw the background for this list box item
    if (itemStyle.visibility() == Visibility::Hidden)
        return;

    LayoutRect itemRect = itemBoundingBoxRect(paintOffset, listIndex);
    itemRect.intersect(controlClipRect(paintOffset));
    paintInfo.context().fillRect(snappedIntRect(itemRect), backColor);
}

bool RenderListBox::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    if (!m_vBar || !m_vBar->shouldParticipateInHitTesting())
        return false;

    LayoutUnit x = accumulatedOffset.x() + (shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : width() - borderRight() - m_vBar->width());
    LayoutUnit y = accumulatedOffset.y() + borderTop();
    LayoutUnit width = m_vBar->width();
    LayoutUnit height = this->height() - borderTop() - borderBottom();
    LayoutRect vertRect(x, y, width, height);

    if (!vertRect.contains(locationInContainer))
        return false;

    result.setScrollbar(m_vBar.get());
    return true;
}

int RenderListBox::listIndexAtOffset(const LayoutSize& offset)
{
    if (!numItems())
        return -1;

    if (offset.height() < borderTop() || offset.height() > height() - borderBottom())
        return -1;

    int scrollbarWidth = m_vBar ? m_vBar->width() : 0;
    if (shouldPlaceVerticalScrollbarOnLeft() && (offset.width() < borderLeft() + paddingLeft() + scrollbarWidth || offset.width() > width() - borderRight() - paddingRight()))
        return -1;
    if (!shouldPlaceVerticalScrollbarOnLeft() && (offset.width() < borderLeft() + paddingLeft() || offset.width() > width() - borderRight() - paddingRight() - scrollbarWidth))
        return -1;

    int newOffset = (offset.height() - borderTop() - paddingTop()) / itemHeight() + m_indexOffset;
    return newOffset < numItems() ? newOffset : -1;
}

void RenderListBox::panScroll(const IntPoint& panStartMousePosition)
{
    const int maxSpeed = 20;
    const int iconRadius = 7;
    const int speedReducer = 4;

    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absOffset = localToAbsolute();

    IntPoint lastKnownMousePosition = frame().eventHandler().lastKnownMousePosition();
    // We need to check if the last known mouse position is out of the window. When the mouse is out of the window, the position is incoherent
    static IntPoint previousMousePosition;
    if (lastKnownMousePosition.y() < 0)
        lastKnownMousePosition = previousMousePosition;
    else
        previousMousePosition = lastKnownMousePosition;

    int yDelta = lastKnownMousePosition.y() - panStartMousePosition.y();

    // If the point is too far from the center we limit the speed
    yDelta = std::max<int>(std::min<int>(yDelta, maxSpeed), -maxSpeed);
    
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
    selectElement().updateListBoxSelection(!selectElement().multiple());
    m_inAutoscroll = false;
}

int RenderListBox::scrollToward(const IntPoint& destination)
{
    // FIXME: This doesn't work correctly with transforms.
    FloatPoint absPos = localToAbsolute();
    IntSize positionOffset = roundedIntSize(destination - absPos);

    int rows = numVisibleItems();
    int offset = m_indexOffset;
    
    if (positionOffset.height() < borderTop() + paddingTop() && scrollToRevealElementAtListIndex(offset - 1))
        return offset - 1;
    
    if (positionOffset.height() > height() - paddingBottom() - borderBottom() && scrollToRevealElementAtListIndex(offset + rows))
        return offset + rows - 1;
    
    return listIndexAtOffset(positionOffset);
}

void RenderListBox::autoscroll(const IntPoint&)
{
    IntPoint pos = frame().view()->windowToContents(frame().eventHandler().lastKnownMousePosition());

    int endIndex = scrollToward(pos);
    if (selectElement().isDisabledFormControl())
        return;

    if (endIndex >= 0) {
        m_inAutoscroll = true;

        if (!selectElement().multiple())
            selectElement().setActiveSelectionAnchorIndex(endIndex);

        selectElement().setActiveSelectionEndIndex(endIndex);
        selectElement().updateListBoxSelection(!selectElement().multiple());
        m_inAutoscroll = false;
    }
}

void RenderListBox::stopAutoscroll()
{
    if (selectElement().isDisabledFormControl())
        return;

    selectElement().listBoxOnChange();
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

    scrollToOffsetWithoutAnimation(VerticalScrollbar, newOffset);

    return true;
}

bool RenderListBox::listIndexIsVisible(int index)
{
    int firstIndex = m_indexOfFirstVisibleItemInsidePaddingTopArea.value_or(m_indexOffset);
    int endIndex = m_indexOfFirstVisibleItemInsidePaddingBottomArea
        ? m_indexOfFirstVisibleItemInsidePaddingBottomArea.value() + numberOfVisibleItemsInPaddingBottom()
        : m_indexOffset + numVisibleItems();

    return index >= firstIndex && index < endIndex;
}

bool RenderListBox::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier, Element**, RenderBox*, const IntPoint&)
{
    return ScrollableArea::scroll(direction, granularity, multiplier);
}

bool RenderListBox::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, float multiplier, Element**)
{
    return ScrollableArea::scroll(logicalToPhysical(direction, style().isHorizontalWritingMode(), style().isFlippedBlocksWritingMode()), granularity, multiplier);
}

void RenderListBox::valueChanged(unsigned listIndex)
{
    selectElement().setSelectedIndex(selectElement().listToOptionIndex(listIndex));
    selectElement().dispatchFormControlChangeEvent();
}

ScrollPosition RenderListBox::scrollPosition() const
{
    return { 0, m_indexOffset };
}

ScrollPosition RenderListBox::minimumScrollPosition() const
{
    return { 0, 0 };
}

ScrollPosition RenderListBox::maximumScrollPosition() const
{
    return { 0, numItems() - numVisibleItems() };
}

void RenderListBox::setScrollOffset(const ScrollOffset& offset)
{
    scrollTo(offset.y());
}

int RenderListBox::maximumNumberOfItemsThatFitInPaddingBottomArea() const
{
    return paddingBottom() / itemHeight();
}

int RenderListBox::numberOfVisibleItemsInPaddingTop() const
{
    if (!m_indexOfFirstVisibleItemInsidePaddingTopArea)
        return 0;

    return m_indexOffset - m_indexOfFirstVisibleItemInsidePaddingTopArea.value();
}

int RenderListBox::numberOfVisibleItemsInPaddingBottom() const
{
    if (!m_indexOfFirstVisibleItemInsidePaddingBottomArea)
        return 0;

    return std::min(maximumNumberOfItemsThatFitInPaddingBottomArea(), numItems() - m_indexOffset - numVisibleItems());
}

void RenderListBox::computeFirstIndexesVisibleInPaddingTopBottomAreas()
{
    m_indexOfFirstVisibleItemInsidePaddingTopArea = std::nullopt;
    m_indexOfFirstVisibleItemInsidePaddingBottomArea = std::nullopt;

    int maximumNumberOfItemsThatFitInPaddingTopArea = paddingTop() / itemHeight();
    if (maximumNumberOfItemsThatFitInPaddingTopArea) {
        if (m_indexOffset)
            m_indexOfFirstVisibleItemInsidePaddingTopArea = std::max(0, m_indexOffset - maximumNumberOfItemsThatFitInPaddingTopArea);
    }

    if (maximumNumberOfItemsThatFitInPaddingBottomArea()) {
        if (numItems() > (m_indexOffset + numVisibleItems()))
            m_indexOfFirstVisibleItemInsidePaddingBottomArea = m_indexOffset + numVisibleItems();
    }
}

void RenderListBox::scrollTo(int newOffset)
{
    if (newOffset == m_indexOffset)
        return;

    m_indexOffset = newOffset;

    computeFirstIndexesVisibleInPaddingTopBottomAreas();

    repaint();
    document().addPendingScrollEventTarget(selectElement());
}

LayoutUnit RenderListBox::itemHeight() const
{
    return style().fontMetrics().height() + rowSpacing;
}

int RenderListBox::verticalScrollbarWidth() const
{
    return m_vBar ? m_vBar->occupiedWidth() : 0;
}

// FIXME: We ignore padding in the vertical direction as far as these values are concerned, since that's
// how the control currently paints.
int RenderListBox::scrollWidth() const
{
    // There is no horizontal scrolling allowed.
    return roundToInt(clientWidth());
}

int RenderListBox::scrollHeight() const
{
    return roundToInt(std::max(clientHeight(), listHeight()));
}

int RenderListBox::scrollLeft() const
{
    return 0;
}

void RenderListBox::setScrollLeft(int, const ScrollPositionChangeOptions&)
{
}

int RenderListBox::scrollTop() const
{
    return m_indexOffset * itemHeight();
}

static void setupWheelEventTestMonitor(RenderListBox& renderer)
{
    if (!renderer.page().isMonitoringWheelEvents())
        return;

    renderer.scrollAnimator().setWheelEventTestMonitor(renderer.page().wheelEventTestMonitor());
}

void RenderListBox::setScrollTop(int newTop, const ScrollPositionChangeOptions&)
{
    // Determine an index and scroll to it.    
    int index = newTop / itemHeight();
    if (index < 0 || index >= numItems() || index == m_indexOffset)
        return;

    setupWheelEventTestMonitor(*this);
    scrollToOffsetWithoutAnimation(VerticalScrollbar, index);
}

bool RenderListBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;
    const Vector<HTMLElement*>& listItems = selectElement().listItems();
    int size = numItems();
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    for (int i = 0; i < size; ++i) {
        if (!itemBoundingBoxRect(adjustedLocation, i).contains(locationInContainer.point()))
            continue;
        if (Element* node = listItems[i]) {
            result.setInnerNode(node);
            if (!result.innerNonSharedNode())
                result.setInnerNonSharedNode(node);
            result.setLocalPoint(locationInContainer.point() - toLayoutSize(adjustedLocation));
            break;
        }
    }

    return true;
}

LayoutRect RenderListBox::controlClipRect(const LayoutPoint& additionalOffset) const
{
    // Clip against the padding box, to give <option>s and overlay scrollbar some extra space
    // to get painted.
    LayoutRect clipRect = paddingBoxRect();
    clipRect.moveBy(additionalOffset);
    return clipRect;
}

bool RenderListBox::isActive() const
{
    return page().focusController().isActive();
}

void RenderListBox::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    IntRect scrollRect = rect;
    scrollRect.move(shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : width() - borderRight() - scrollbar.width(), borderTop());
    repaintRectangle(scrollRect);
}

IntRect RenderListBox::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    IntRect rect = scrollbarRect;
    int scrollbarLeft = shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : width() - borderRight() - scrollbar.width();
    int scrollbarTop = borderTop();
    rect.move(scrollbarLeft, scrollbarTop);
    return view().frameView().convertFromRendererToContainingView(this, rect);
}

IntRect RenderListBox::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    IntRect rect = view().frameView().convertFromContainingViewToRenderer(this, parentRect);
    int scrollbarLeft = shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : width() - borderRight() - scrollbar.width();
    int scrollbarTop = borderTop();
    rect.move(-scrollbarLeft, -scrollbarTop);
    return rect;
}

IntPoint RenderListBox::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    IntPoint point = scrollbarPoint;
    int scrollbarLeft = shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : width() - borderRight() - scrollbar.width();
    int scrollbarTop = borderTop();
    point.move(scrollbarLeft, scrollbarTop);
    return view().frameView().convertFromRendererToContainingView(this, point);
}

IntPoint RenderListBox::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    IntPoint point = view().frameView().convertFromContainingViewToRenderer(this, parentPoint);
    int scrollbarLeft = shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : width() - borderRight() - scrollbar.width();
    int scrollbarTop = borderTop();
    point.move(-scrollbarLeft, -scrollbarTop);
    return point;
}

IntSize RenderListBox::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntPoint RenderListBox::lastKnownMousePositionInView() const
{
    return view().frameView().lastKnownMousePositionInView();
}

bool RenderListBox::isHandlingWheelEvent() const
{
    return view().frameView().isHandlingWheelEvent();
}

bool RenderListBox::shouldSuspendScrollAnimations() const
{
    return view().frameView().shouldSuspendScrollAnimations();
}

bool RenderListBox::forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const
{
    return settings().scrollingPerformanceTestingEnabled();
}

ScrollableArea* RenderListBox::enclosingScrollableArea() const
{
    auto* layer = enclosingLayer();
    if (!layer)
        return nullptr;

    auto* enclosingScrollableLayer = layer->enclosingScrollableLayer(IncludeSelfOrNot::ExcludeSelf, CrossFrameBoundaries::No);
    if (!enclosingScrollableLayer)
        return nullptr;

    return enclosingScrollableLayer->scrollableArea();
}

bool RenderListBox::isScrollableOrRubberbandable()
{
    return m_vBar;
}

bool RenderListBox::hasScrollableOrRubberbandableAncestor()
{
    if (auto* scrollableArea = enclosingLayer() ? enclosingLayer()->scrollableArea() : nullptr)
        return scrollableArea->hasScrollableOrRubberbandableAncestor();
    return false;
}

IntRect RenderListBox::scrollableAreaBoundingBox(bool*) const
{
    return absoluteBoundingBoxRect();
}

bool RenderListBox::usesMockScrollAnimator() const
{
    return DeprecatedGlobalSettings::usesMockScrollAnimator();
}

void RenderListBox::logMockScrollAnimatorMessage(const String& message) const
{
    document().addConsoleMessage(MessageSource::Other, MessageLevel::Debug, "RenderListBox: " + message);
}

String RenderListBox::debugDescription() const
{
    return RenderObject::debugDescription();
}

Ref<Scrollbar> RenderListBox::createScrollbar()
{
    RefPtr<Scrollbar> widget;
    bool hasCustomScrollbarStyle = style().hasPseudoStyle(PseudoId::Scrollbar);
    if (hasCustomScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(*this, VerticalScrollbar, &selectElement());
    else {
        widget = Scrollbar::createNativeScrollbar(*this, VerticalScrollbar, theme().scrollbarControlSizeForPart(ListboxPart));
        didAddScrollbar(widget.get(), VerticalScrollbar);
        if (page().isMonitoringWheelEvents())
            scrollAnimator().setWheelEventTestMonitor(page().wheelEventTestMonitor());
    }
    view().frameView().addChild(*widget);
    return widget.releaseNonNull();
}

void RenderListBox::destroyScrollbar()
{
    if (!m_vBar)
        return;

    if (!m_vBar->isCustomScrollbar())
        ScrollableArea::willRemoveScrollbar(m_vBar.get(), VerticalScrollbar);
    m_vBar->removeFromParent();
    m_vBar = nullptr;
}

void RenderListBox::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == (m_vBar != nullptr))
        return;

    if (hasScrollbar)
        m_vBar = createScrollbar();
    else
        destroyScrollbar();

    if (m_vBar)
        m_vBar->styleChanged();
}

bool RenderListBox::scrolledToTop() const
{
    if (Scrollbar* vbar = verticalScrollbar())
    return vbar->value() <= 0;

    return true;
}

bool RenderListBox::scrolledToBottom() const
{
    Scrollbar* vbar = verticalScrollbar();
    if (!vbar)
        return true;

    return vbar->value() >= vbar->maximum();
}

bool RenderListBox::scrolledToLeft() const
{
    // We do not scroll horizontally in a select element, so always report
    // that we are at the full extent of the scroll.
    return true;
}

bool RenderListBox::scrolledToRight() const
{
    // We do not scroll horizontally in a select element, so always report
    // that we are at the full extent of the scroll.
    return true;
}
    
} // namespace WebCore
