/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
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
#include "DocumentInlines.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLOptGroupElement.h"
#include "HTMLSelectElement.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
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
#include "UnicodeBidi.h"
#include "WheelEventTestMonitor.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderListBox);
 
const int itemBlockSpacing = 1;

const int optionsSpacingInlineStart = 2;

// Default size when the multiple attribute is present but size attribute is absent.
const int defaultSize = 4;

// FIXME: This hardcoded baselineAdjustment is what we used to do for the old
// widget, but I'm not sure this is right for the new control.
const int baselineAdjustment = 7;

RenderListBox::RenderListBox(HTMLSelectElement& element, RenderStyle&& style)
    : RenderBlockFlow(Type::ListBox, element, WTFMove(style))
{
    view().frameView().addScrollableArea(this);
}

RenderListBox::~RenderListBox()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderListBox::willBeDestroyed()
{
    destroyScrollbar();
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
    FontCascade result(WTFMove(description), font);
    result.update(&document.fontSelector());
    return result;
}

void RenderListBox::updateFromElement()
{
    if (m_optionsChanged) {
        float logicalWidth = 0;
        auto& normalFont = style().fontCascade();
        std::optional<FontCascade> boldFont;
        for (auto& element : selectElement().listItems()) {
            String text;
            Function<const FontCascade&()> selectFont = [&normalFont] () -> const FontCascade& {
                return normalFont;
            };
            if (RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(element.get()))
                text = optionElement->textIndentedToRespectGroupLabel();
            else if (RefPtr optGroupElement = dynamicDowncast<HTMLOptGroupElement>(element.get())) {
                text = optGroupElement->groupLabelText();
                selectFont = [this, &normalFont, &boldFont] () -> const FontCascade& {
                    if (!boldFont)
                        boldFont = bolder(document(), normalFont);
                    return boldFont.value();
                };
            }
            if (text.isEmpty())
                continue;
            text = applyTextTransform(style(), text, ' ');
            auto textRun = constructTextRun(text, style(), ExpansionBehavior::allowRightOnly());
            logicalWidth = std::max(logicalWidth, selectFont().width(textRun));
        }
        // FIXME: Is ceiling right here, or should we be doing some kind of rounding instead?
        m_optionsLogicalWidth = static_cast<int>(std::ceil(logicalWidth));
        m_optionsChanged = false;

        setHasScrollbar(scrollbarOrientationForWritingMode());

        computeFirstIndexesVisibleInPaddingBeforeAfterAreas();

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

    if (m_scrollbar) {
        bool enabled = numVisibleItems() < numItems();
        m_scrollbar->setEnabled(enabled);
        m_scrollbar->setSteps(1, std::max(1, numVisibleItems() - 1), itemLogicalHeight());
        m_scrollbar->setProportion(numVisibleItems(), numItems());
        if (!enabled) {
            scrollToOffsetWithoutAnimation(m_scrollbar->orientation(), 0);
            m_scrollPosition = { };
        }

        if (style().isFlippedBlocksWritingMode()) {
            auto scrollOrigin = IntPoint(0, numItems() - numVisibleItems());
            if (m_scrollbar->orientation() == ScrollbarOrientation::Horizontal)
                scrollOrigin = scrollOrigin.transposedPoint();
            setScrollOrigin(scrollOrigin);
            m_scrollbar->offsetDidChange();
        } else
            setScrollOrigin(IntPoint());
    }

    if (m_scrollToRevealSelectionAfterLayout) {
        LayoutStateDisabler layoutStateDisabler(view().frameView().layoutContext());
        scrollToRevealSelection();
    }
}

void RenderListBox::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (oldStyle && oldStyle->writingMode() != style().writingMode()) {
        if (m_scrollbar)
            setHasScrollbar(scrollbarOrientationForWritingMode());
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
    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto logicalWidth = explicitIntrinsicInnerLogicalWidth())
            maxLogicalWidth = logicalWidth.value();
        else
            maxLogicalWidth = 2 * optionsSpacingInlineStart;
    } else
        maxLogicalWidth = 2 * optionsSpacingInlineStart + m_optionsLogicalWidth;

    if (m_scrollbar)
        maxLogicalWidth += m_scrollbar->orientation() == ScrollbarOrientation::Vertical ? m_scrollbar->width() : m_scrollbar->height();

    if (!style().logicalWidth().isPercentOrCalculated())
        minLogicalWidth = maxLogicalWidth;
}

void RenderListBox::computePreferredLogicalWidths()
{
    // Nested style recal do not fire post recal callbacks. see webkit.org/b/153767
    ASSERT(!m_optionsChanged || Style::postResolutionCallbacksAreSuspended());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style().logicalWidth().isFixed() && style().logicalWidth().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().logicalWidth());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), style().isHorizontalWritingMode() ? horizontalBorderAndPaddingExtent() : verticalBorderAndPaddingExtent());

    setPreferredLogicalWidthsDirty(false);
}

unsigned RenderListBox::size() const
{
    unsigned specifiedSize = selectElement().size();
    if (specifiedSize >= 1)
        return specifiedSize;

    return defaultSize;
}

int RenderListBox::numVisibleItems(ConsiderPadding considerPadding) const
{
    // Only count fully visible rows. But don't return 0 even if only part of a row shows.
    int visibleItemsExcludingPadding = std::max<int>(1, (contentLogicalHeight() + itemBlockSpacing) / itemLogicalHeight());
    if (considerPadding == ConsiderPadding::No)
        return visibleItemsExcludingPadding;

    return numberOfVisibleItemsInPaddingBefore() + visibleItemsExcludingPadding + numberOfVisibleItemsInPaddingAfter();
}

int RenderListBox::numItems() const
{
    return selectElement().listItems().size();
}

LayoutUnit RenderListBox::listLogicalHeight() const
{
    return itemLogicalHeight() * numItems() - itemBlockSpacing;
}

RenderBox::LogicalExtentComputedValues RenderListBox::computeLogicalHeight(LayoutUnit, LayoutUnit logicalTop) const
{
    LayoutUnit logicalHeight = itemLogicalHeight() * size() - itemBlockSpacing;

    if (shouldApplySizeContainment()) {
        if (auto explicitIntrinsicHeight = explicitIntrinsicInnerLogicalHeight())
            logicalHeight = explicitIntrinsicHeight.value();
    }

    cacheIntrinsicContentLogicalHeightForFlexItem(logicalHeight);
    logicalHeight += style().isHorizontalWritingMode() ? verticalBorderAndPaddingExtent() : horizontalBorderAndPaddingExtent();
    return RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
}

LayoutUnit RenderListBox::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode lineDirection, LinePositionMode linePositionMode) const
{
    auto baseline = RenderBox::baselinePosition(baselineType, firstLine, lineDirection, linePositionMode);
    if (!shouldApplyLayoutContainment())
        baseline -= baselineAdjustment;
    return baseline;
}

LayoutRect RenderListBox::itemBoundingBoxRect(const LayoutPoint& additionalOffset, int index) const
{
    LayoutUnit x = additionalOffset.x() + borderLeft() + paddingLeft();
    LayoutUnit y = additionalOffset.y() + borderTop() + paddingTop();

    if (auto* vBar = verticalScrollbar(); vBar && shouldPlaceVerticalScrollbarOnLeft())
        x += vBar->occupiedWidth();

    auto itemOffset = itemLogicalHeight() * (index - indexOffset());
    if (style().isFlippedBlocksWritingMode())
        itemOffset = contentLogicalHeight() - itemLogicalHeight() - itemOffset;

    if (style().isVerticalWritingMode())
        return LayoutRect(x + itemOffset, y, itemLogicalHeight(), contentHeight());

    return LayoutRect(x, y + itemOffset, contentWidth(), itemLogicalHeight());
}

std::optional<int> RenderListBox::optionRowIndex(const HTMLOptionElement& optionElement) const
{
    // We can't use optionElement.index(), because it doesn't account for optgroup items.
    int rowIndex = 0;
    for (auto& item : selectElement().listItems()) {
        if (item == &optionElement)
            return rowIndex;

        ++rowIndex;
    }

    return { };
}

std::optional<LayoutRect> RenderListBox::localBoundsOfOption(const HTMLOptionElement& optionElement) const
{
    auto rowIndex = optionRowIndex(optionElement);
    if (!rowIndex)
        return { };

    return itemBoundingBoxRect({ }, *rowIndex);
}

std::optional<LayoutRect> RenderListBox::localBoundsOfOptGroup(const HTMLOptGroupElement& optGroupElement) const
{
    if (optGroupElement.ownerSelectElement() != &selectElement())
        return { };

    std::optional<LayoutRect> boundingBox;
    int rowIndex = 0;

    for (auto& item : selectElement().listItems()) {
        if (is<HTMLOptGroupElement>(*item)) {
            if (item == &optGroupElement)
                boundingBox = itemBoundingBoxRect({ }, rowIndex);
        } else if (is<HTMLOptionElement>(*item)) {
            if (item->parentNode() != &optGroupElement)
                break;

            boundingBox->setHeight(boundingBox->height() + itemBoundingBoxRect({ }, rowIndex).height());
        }
        ++rowIndex;
    }

    return boundingBox;
}

void RenderListBox::paintItem(PaintInfo& paintInfo, const LayoutPoint& paintOffset, const PaintFunction& paintFunction)
{
    int listItemsSize = numItems();
    int firstVisibleItem = m_indexOfFirstVisibleItemInsidePaddingBeforeArea.value_or(indexOffset());
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
        if (m_scrollbar->isOverlayScrollbar())
            paintScrollbar(paintInfo, paintOffset, *m_scrollbar);
        break;
    case PaintPhase::BlockBackground:
        if (!m_scrollbar->isOverlayScrollbar())
            paintScrollbar(paintInfo, paintOffset, *m_scrollbar);
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

void RenderListBox::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject* paintContainer) const
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
    int indexOfFirstEnabledOption = 0;
    for (auto& item : selectElement().listItems()) {
        if (is<HTMLOptionElement>(item.get()) && !item->isDisabledFormControl()) {
            selectElement().setActiveSelectionEndIndex(indexOfFirstEnabledOption);
            rects.append(itemBoundingBoxRect(additionalOffset, indexOfFirstEnabledOption));
            return;
        }
        indexOfFirstEnabledOption++;
    }
}

void RenderListBox::paintScrollbar(PaintInfo& paintInfo, const LayoutPoint& paintOffset, Scrollbar& scrollbar)
{
    auto scrollRect = rectForScrollbar(scrollbar);
    scrollRect.moveBy(paintOffset);
    scrollbar.setFrameRect(snappedIntRect(scrollRect));
    scrollbar.paint(paintInfo.context(), snappedIntRect(paintInfo.rect));
}

static LayoutSize itemOffsetForAlignment(TextRun textRun, const RenderStyle& elementStyle, const RenderStyle* itemStyle, FontCascade itemFont, LayoutRect itemBoundingBox)
{
    TextAlignMode actualAlignment = itemStyle->textAlign();
    // FIXME: Firefox doesn't respect TextAlignMode::Justify. Should we?
    // FIXME: Handle TextAlignMode::End here
    if (actualAlignment == TextAlignMode::Start || actualAlignment == TextAlignMode::Justify)
        actualAlignment = itemStyle->isLeftToRightDirection() ? TextAlignMode::Left : TextAlignMode::Right;

    bool isHorizontalWritingMode = elementStyle.isHorizontalWritingMode();

    auto itemBoundingBoxLogicalWidth = isHorizontalWritingMode ? itemBoundingBox.width() : itemBoundingBox.height();
    auto offset = LayoutSize(0, itemFont.metricsOfPrimaryFont().ascent());
    if (actualAlignment == TextAlignMode::Right || actualAlignment == TextAlignMode::WebKitRight) {
        float textWidth = itemFont.width(textRun);
        offset.setWidth(itemBoundingBoxLogicalWidth - textWidth - optionsSpacingInlineStart);
    } else if (actualAlignment == TextAlignMode::Center || actualAlignment == TextAlignMode::WebKitCenter) {
        float textWidth = itemFont.width(textRun);
        offset.setWidth((itemBoundingBoxLogicalWidth - textWidth) / 2);
    } else
        offset.setWidth(optionsSpacingInlineStart);

    if (!isHorizontalWritingMode)
        return LayoutSize { -offset.height(), offset.width() };

    return offset;
}

void RenderListBox::paintItemForeground(PaintInfo& paintInfo, const LayoutPoint& paintOffset, int listIndex)
{
    const auto& listItems = selectElement().listItems();
    RefPtr listItemElement = listItems[listIndex].get();

    auto itemStyle = listItemElement->computedStyleForEditability();
    if (!itemStyle)
        return;

    if (itemStyle->visibility() == Visibility::Hidden)
        return;

    String itemText;
    RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(*listItemElement);
    RefPtr optGroupElement = dynamicDowncast<HTMLOptGroupElement>(*listItemElement);
    if (optionElement)
        itemText = optionElement->textIndentedToRespectGroupLabel();
    else if (optGroupElement)
        itemText = optGroupElement->groupLabelText();
    itemText = applyTextTransform(style(), itemText, ' ');

    if (itemText.isNull())
        return;

    Color textColor = itemStyle->visitedDependentColorWithColorFilter(CSSPropertyColor);
    if (optionElement && optionElement->selected()) {
        if (frame().selection().isFocusedAndActive() && document().focusedElement() == &selectElement())
            textColor = theme().activeListBoxSelectionForegroundColor(styleColorOptions());
        // Honor the foreground color for disabled items
        else if (!listItemElement->isDisabledFormControl() && !selectElement().isDisabledFormControl())
            textColor = theme().inactiveListBoxSelectionForegroundColor(styleColorOptions());
    }

    GraphicsContextStateSaver stateSaver(paintInfo.context());

    paintInfo.context().setFillColor(textColor);

    TextRun textRun(itemText, 0, 0, ExpansionBehavior::allowRightOnly(), itemStyle->direction(), isOverride(itemStyle->unicodeBidi()), true);
    FontCascade itemFont = style().fontCascade();
    LayoutRect r = itemBoundingBoxRect(paintOffset, listIndex);
    r.move(itemOffsetForAlignment(textRun, style(), itemStyle, itemFont, r));

    bool isHorizontalWritingMode = style().isHorizontalWritingMode();
    if (!isHorizontalWritingMode) {
        auto rotationOrigin = roundedIntPoint(r.maxXMinYCorner());
        paintInfo.context().translate(rotationOrigin);
        paintInfo.context().rotate(piOverTwoFloat);
        paintInfo.context().translate(-rotationOrigin);
    }

    if (optGroupElement) {
        auto description = itemFont.fontDescription();
        description.setWeight(description.bolderWeight());
        itemFont = FontCascade(WTFMove(description), itemFont);
        itemFont.update(&document().fontSelector());
    }

    // Draw the item text
    paintInfo.context().drawBidiText(itemFont, textRun, roundedIntPoint(isHorizontalWritingMode ? r.location() : r.maxXMinYCorner()));
}

void RenderListBox::paintItemBackground(PaintInfo& paintInfo, const LayoutPoint& paintOffset, int listIndex)
{
    const auto& listItems = selectElement().listItems();
    RefPtr listItemElement = listItems[listIndex].get();
    auto itemStyle = listItemElement->computedStyleForEditability();
    if (!itemStyle)
        return;

    Color backColor;
    if (auto* option = dynamicDowncast<HTMLOptionElement>(*listItemElement); option && option->selected()) {
        if (frame().selection().isFocusedAndActive() && document().focusedElement() == &selectElement())
            backColor = theme().activeListBoxSelectionBackgroundColor(styleColorOptions());
        else
            backColor = theme().inactiveListBoxSelectionBackgroundColor(styleColorOptions());
    } else
        backColor = itemStyle->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);

    // Draw the background for this list box item
    if (itemStyle->visibility() == Visibility::Hidden)
        return;

    LayoutRect itemRect = itemBoundingBoxRect(paintOffset, listIndex);
    itemRect.intersect(controlClipRect(paintOffset));
    paintInfo.context().fillRect(snappedIntRect(itemRect), backColor);
}

bool RenderListBox::isPointInOverflowControl(HitTestResult& result, const LayoutPoint& locationInContainer, const LayoutPoint& accumulatedOffset)
{
    auto* activeScrollbar = verticalScrollbar();
    if (!activeScrollbar)
        activeScrollbar = horizontalScrollbar();

    if (!activeScrollbar || !activeScrollbar->shouldParticipateInHitTesting())
        return false;

    auto scrollbarRect = rectForScrollbar(*activeScrollbar);
    scrollbarRect.moveBy(accumulatedOffset);

    if (!scrollbarRect.contains(locationInContainer))
        return false;

    result.setScrollbar(activeScrollbar);
    return true;
}

int RenderListBox::listIndexAtOffset(const LayoutSize& offset) const
{
    if (!numItems())
        return -1;

    int scrollbarHeight = 0;
    if (auto* hBar = horizontalScrollbar())
        scrollbarHeight = hBar->height();

    if (offset.height() < borderTop() || offset.height() > height() - borderBottom() - scrollbarHeight)
        return -1;

    int scrollbarWidth = 0;
    if (auto* vBar = verticalScrollbar())
        scrollbarWidth = vBar->width();

    if (shouldPlaceVerticalScrollbarOnLeft() && (offset.width() < borderLeft() + paddingLeft() + scrollbarWidth || offset.width() > width() - borderRight() - paddingRight()))
        return -1;
    if (!shouldPlaceVerticalScrollbarOnLeft() && (offset.width() < borderLeft() + paddingLeft() || offset.width() > width() - borderRight() - paddingRight() - scrollbarWidth))
        return -1;

    auto offsetLogicalHeight = style().isHorizontalWritingMode() ? offset.height() : offset.width();

    int newOffset;
    if (style().isFlippedBlocksWritingMode())
        newOffset = (logicalHeight() - borderAndPaddingBefore() - offsetLogicalHeight) / itemLogicalHeight() + indexOffset();
    else
        newOffset = (offsetLogicalHeight - borderAndPaddingBefore()) / itemLogicalHeight() + indexOffset();

    return newOffset < numItems() ? newOffset : -1;
}

void RenderListBox::panScroll(const IntPoint& panStartMousePosition)
{
    // FIXME: This does not support vertical writing mode or flipped block directions.

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
    
    if (std::abs(yDelta) < iconRadius) // at the center we let the space for the icon
        return;

    if (yDelta > 0)
        absOffset.move(0, listLogicalHeight());
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
    int positionOffsetLogicalHeight = style().isHorizontalWritingMode() ? positionOffset.height() : positionOffset.width();

    int rows = numVisibleItems();
    int offset = indexOffset();

    if (style().isFlippedBlocksWritingMode()) {
        if (positionOffsetLogicalHeight < borderAndPaddingAfter() && scrollToRevealElementAtListIndex(offset + rows))
            return offset + rows - 1;

        if (positionOffsetLogicalHeight > logicalHeight() - borderAndPaddingBefore() && scrollToRevealElementAtListIndex(offset - 1))
            return offset - 1;
    } else {
        if (positionOffsetLogicalHeight < borderAndPaddingBefore() && scrollToRevealElementAtListIndex(offset - 1))
            return offset - 1;

        if (positionOffsetLogicalHeight > logicalHeight() - borderAndPaddingAfter() && scrollToRevealElementAtListIndex(offset + rows))
            return offset + rows - 1;
    }

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
    if (index < indexOffset())
        newOffset = index;
    else
        newOffset = index - numVisibleItems() + 1;

    if (style().isFlippedBlocksWritingMode())
        newOffset *= -1;

    scrollToPosition(newOffset);
    return true;
}

bool RenderListBox::listIndexIsVisible(int index)
{
    int firstIndex = m_indexOfFirstVisibleItemInsidePaddingBeforeArea.value_or(indexOffset());
    int endIndex = m_indexOfFirstVisibleItemInsidePaddingAfterArea
        ? m_indexOfFirstVisibleItemInsidePaddingAfterArea.value() + numberOfVisibleItemsInPaddingAfter()
        : indexOffset() + numVisibleItems();

    return index >= firstIndex && index < endIndex;
}

bool RenderListBox::scroll(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount, Element**, RenderBox*, const IntPoint&)
{
    return ScrollableArea::scroll(direction, granularity, stepCount);
}

bool RenderListBox::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, unsigned stepCount, Element**)
{
    return ScrollableArea::scroll(logicalToPhysical(direction, style().isHorizontalWritingMode(), style().isFlippedBlocksWritingMode()), granularity, stepCount);
}

int RenderListBox::indexOffset() const
{
    auto scrollPosition = this->scrollPosition();
    if (!style().isHorizontalWritingMode())
        scrollPosition = scrollPosition.transposedPoint();
    return std::abs(scrollPosition.y());
}

ScrollPosition RenderListBox::scrollPosition() const
{
    return m_scrollPosition;
}

ScrollPosition RenderListBox::minimumScrollPosition() const
{
    return scrollPositionFromOffset(ScrollOffset());
}

ScrollPosition RenderListBox::maximumScrollPosition() const
{
    auto maximumScrollOffset = ScrollOffset(0, numItems() - numVisibleItems());
    if (!style().isHorizontalWritingMode())
        maximumScrollOffset = maximumScrollOffset.transposedPoint();
    return scrollPositionFromOffset(maximumScrollOffset);
}

void RenderListBox::setScrollOffset(const ScrollOffset& offset)
{
    scrollTo(scrollPositionFromOffset(offset));
}

int RenderListBox::maximumNumberOfItemsThatFitInPaddingAfterArea() const
{
    return paddingAfter() / itemLogicalHeight();
}

int RenderListBox::numberOfVisibleItemsInPaddingBefore() const
{
    if (!m_indexOfFirstVisibleItemInsidePaddingBeforeArea)
        return 0;

    return indexOffset() - m_indexOfFirstVisibleItemInsidePaddingBeforeArea.value();
}

int RenderListBox::numberOfVisibleItemsInPaddingAfter() const
{
    if (!m_indexOfFirstVisibleItemInsidePaddingAfterArea)
        return 0;

    return std::min(maximumNumberOfItemsThatFitInPaddingAfterArea(), numItems() - indexOffset() - numVisibleItems());
}

void RenderListBox::computeFirstIndexesVisibleInPaddingBeforeAfterAreas()
{
    m_indexOfFirstVisibleItemInsidePaddingBeforeArea = std::nullopt;
    m_indexOfFirstVisibleItemInsidePaddingAfterArea = std::nullopt;

    int maximumNumberOfItemsThatFitInPaddingBeforeArea = paddingBefore() / itemLogicalHeight();
    if (maximumNumberOfItemsThatFitInPaddingBeforeArea) {
        if (indexOffset())
            m_indexOfFirstVisibleItemInsidePaddingBeforeArea = std::max(0, indexOffset() - maximumNumberOfItemsThatFitInPaddingBeforeArea);
    }

    if (maximumNumberOfItemsThatFitInPaddingAfterArea()) {
        if (numItems() > (indexOffset() + numVisibleItems()))
            m_indexOfFirstVisibleItemInsidePaddingAfterArea = indexOffset() + numVisibleItems();
    }
}

void RenderListBox::scrollTo(const ScrollPosition& position)
{
    if (position == m_scrollPosition)
        return;

    m_scrollPosition = position;

    computeFirstIndexesVisibleInPaddingBeforeAfterAreas();

    repaint();
    document().addPendingScrollEventTarget(selectElement());
}

LayoutUnit RenderListBox::itemLogicalHeight() const
{
    return style().metricsOfPrimaryFont().height() + itemBlockSpacing;
}

int RenderListBox::verticalScrollbarWidth() const
{
    if (auto* vBar = verticalScrollbar())
        return vBar->occupiedWidth();

    return 0;
}

int RenderListBox::horizontalScrollbarHeight() const
{
    if (auto* hBar = horizontalScrollbar())
        return hBar->occupiedHeight();

    return 0;
}

Scrollbar* RenderListBox::verticalScrollbar() const
{
    if (m_scrollbar && m_scrollbar->orientation() == ScrollbarOrientation::Vertical)
        return m_scrollbar.get();

    return nullptr;
}

Scrollbar* RenderListBox::horizontalScrollbar() const
{
    if (m_scrollbar && m_scrollbar->orientation() == ScrollbarOrientation::Horizontal)
        return m_scrollbar.get();

    return nullptr;
}

ScrollbarOrientation RenderListBox::scrollbarOrientationForWritingMode() const
{
    if (style().isHorizontalWritingMode())
        return ScrollbarOrientation::Vertical;
    return ScrollbarOrientation::Horizontal;
}

// FIXME: We ignore padding in the vertical direction as far as these values are concerned, since that's
// how the control currently paints.
int RenderListBox::scrollWidth() const
{
    if (style().isHorizontalWritingMode())
        return roundToInt(clientWidth());

    return roundToInt(std::max(clientWidth(), listLogicalHeight()));
}

int RenderListBox::scrollHeight() const
{
    if (style().isHorizontalWritingMode())
        return roundToInt(std::max(clientHeight(), listLogicalHeight()));

    return roundToInt(clientHeight());
}

int RenderListBox::scrollLeft() const
{
    if (style().isHorizontalWritingMode())
        return 0;
    return logicalScrollTop();
}

void RenderListBox::setScrollLeft(int newLeft, const ScrollPositionChangeOptions&)
{
    if (style().isHorizontalWritingMode())
        return;

    setLogicalScrollTop(newLeft);
}

int RenderListBox::scrollTop() const
{
    if (style().isHorizontalWritingMode())
        return logicalScrollTop();
    return 0;
}

static void setupWheelEventTestMonitor(RenderListBox& renderer)
{
    if (!renderer.page().isMonitoringWheelEvents())
        return;

    renderer.scrollAnimator().setWheelEventTestMonitor(renderer.page().wheelEventTestMonitor());
}

void RenderListBox::setScrollTop(int newTop, const ScrollPositionChangeOptions&)
{
    if (!style().isHorizontalWritingMode())
        return;

    setLogicalScrollTop(newTop);
}

int RenderListBox::logicalScrollTop() const
{
    int logicalTop = indexOffset() * itemLogicalHeight();
    if (style().isFlippedBlocksWritingMode())
        logicalTop *= -1;
    return logicalTop;
}

void RenderListBox::scrollToPosition(int positionIndex)
{
    auto orientation = scrollbarOrientationForWritingMode();
    auto scrollOrigin = this->scrollOrigin();

    int offsetIndex = positionIndex;

    switch (orientation) {
    case ScrollbarOrientation::Vertical:
        offsetIndex = positionIndex + scrollOrigin.y();
        break;
    case ScrollbarOrientation::Horizontal:
        offsetIndex = positionIndex + scrollOrigin.x();
        break;
    }

    scrollToOffsetWithoutAnimation(orientation, offsetIndex);
}

void RenderListBox::setLogicalScrollTop(int newLogicalScrollTop)
{
    bool isFlippedBlocksWritingMode = style().isFlippedBlocksWritingMode();

    int newTop = newLogicalScrollTop;
    if (isFlippedBlocksWritingMode)
        newTop *= -1;

    int index = newTop / itemLogicalHeight();
    index = std::clamp(index, 0, std::max(0, numItems() - 1));
    if (index == indexOffset())
        return;

    if (isFlippedBlocksWritingMode)
        index *= -1;

    setupWheelEventTestMonitor(*this);
    scrollToPosition(index);
}

bool RenderListBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderBlockFlow::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;
    const auto& listItems = selectElement().listItems();
    int size = numItems();
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    for (int i = 0; i < size; ++i) {
        if (!itemBoundingBoxRect(adjustedLocation, i).contains(locationInContainer.point()))
            continue;
        if (RefPtr node = listItems[i].get()) {
            result.setInnerNode(node.get());
            if (!result.innerNonSharedNode())
                result.setInnerNonSharedNode(node.get());
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

LayoutRect RenderListBox::rectForScrollbar(const Scrollbar& scrollbar) const
{
    LayoutUnit left, top, width, height;

    if (scrollbar.orientation() == ScrollbarOrientation::Vertical) {
        left = shouldPlaceVerticalScrollbarOnLeft() ? borderLeft() : this->width() - borderRight() - scrollbar.width();
        top = borderTop();
        width = scrollbar.width();
        height = this->height() - verticalBorderExtent();
    } else {
        left = borderLeft();
        top = this->height() - borderBottom() - scrollbar.height();
        width = this->width() - horizontalBorderExtent();
        height = scrollbar.height();
    }

    return LayoutRect { left, top, width, height };
}

void RenderListBox::invalidateScrollbarRect(Scrollbar& scrollbar, const IntRect& rect)
{
    auto scrollRect = rect;
    auto scrollbarLocation = rectForScrollbar(scrollbar).location();
    scrollRect.move(scrollbarLocation.x(), scrollbarLocation.y());
    repaintRectangle(scrollRect);
}

IntRect RenderListBox::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    auto rect = scrollbarRect;
    auto scrollbarLocation = rectForScrollbar(scrollbar).location();
    rect.move(scrollbarLocation.x(), scrollbarLocation.y());
    return view().frameView().convertFromRendererToContainingView(this, rect);
}

IntRect RenderListBox::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    IntRect rect = view().frameView().convertFromContainingViewToRenderer(this, parentRect);
    auto scrollbarLocation = rectForScrollbar(scrollbar).location();
    rect.move(-scrollbarLocation.x(), -scrollbarLocation.y());
    return rect;
}

IntPoint RenderListBox::convertFromScrollbarToContainingView(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    auto point = scrollbarPoint;
    auto scrollbarLocation = rectForScrollbar(scrollbar).location();
    point.move(scrollbarLocation.x(), scrollbarLocation.y());
    return view().frameView().convertFromRendererToContainingView(this, point);
}

IntPoint RenderListBox::convertFromContainingViewToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    IntPoint point = view().frameView().convertFromContainingViewToRenderer(this, parentPoint);
    auto scrollbarLocation = rectForScrollbar(scrollbar).location();
    point.move(-scrollbarLocation.x(), -scrollbarLocation.y());
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
    return !!m_scrollbar;
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

bool RenderListBox::mockScrollbarsControllerEnabled() const
{
    return settings().mockScrollbarsControllerEnabled();
}

void RenderListBox::logMockScrollbarsControllerMessage(const String& message) const
{
    document().addConsoleMessage(MessageSource::Other, MessageLevel::Debug, "RenderListBox: " + message);
}

String RenderListBox::debugDescription() const
{
    return RenderObject::debugDescription();
}

void RenderListBox::didStartScrollAnimation()
{
    page().scheduleRenderingUpdate({ RenderingUpdateStep::Scroll });
}

Ref<Scrollbar> RenderListBox::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    bool usesLegacyScrollbarStyle = style().usesLegacyScrollbarStyle();
    if (usesLegacyScrollbarStyle)
        widget = RenderScrollbar::createCustomScrollbar(*this, orientation, &selectElement());
    else {
        widget = Scrollbar::createNativeScrollbar(*this, orientation, theme().scrollbarWidthStyleForPart(StyleAppearance::Listbox));
        didAddScrollbar(widget.get(), orientation);
        if (page().isMonitoringWheelEvents())
            scrollAnimator().setWheelEventTestMonitor(page().wheelEventTestMonitor());
    }
    view().frameView().addChild(*widget);
    return widget.releaseNonNull();
}

void RenderListBox::destroyScrollbar()
{
    if (!m_scrollbar)
        return;

    if (!m_scrollbar->isCustomScrollbar())
        ScrollableArea::willRemoveScrollbar(m_scrollbar.get(), m_scrollbar->orientation());
    m_scrollbar->removeFromParent();
    m_scrollbar = nullptr;
}

void RenderListBox::setHasScrollbar(ScrollbarOrientation orientation)
{
    if (verticalScrollbar() && orientation == ScrollbarOrientation::Vertical)
        return;

    if (horizontalScrollbar() && orientation == ScrollbarOrientation::Horizontal)
        return;

    destroyScrollbar();
    m_scrollbar = createScrollbar(orientation);
    m_scrollbar->styleChanged();
}

float RenderListBox::deviceScaleFactor() const
{
    return page().deviceScaleFactor();
}
    
bool RenderListBox::isVisibleToHitTesting() const
{
    return visibleToHitTesting();
}

} // namespace WebCore
