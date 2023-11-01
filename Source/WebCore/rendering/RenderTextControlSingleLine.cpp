/**
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/) 
 * Copyright (C) 2010-2014 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "config.h"
#include "RenderTextControlSingleLine.h"

#include "CSSFontSelector.h"
#include "CSSValueKeywords.h"
#include "Font.h"
#include "FrameSelection.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InlineIteratorLineBox.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderScrollbar.h"
#include "RenderStyleSetters.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "StyleResolver.h"
#include "TextControlInnerElements.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

#if PLATFORM(IOS_FAMILY)
#include "RenderThemeIOS.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTextControlSingleLine);
WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTextControlInnerBlock);

RenderTextControlSingleLine::RenderTextControlSingleLine(HTMLInputElement& element, RenderStyle&& style)
    : RenderTextControl(element, WTFMove(style))
{
}

RenderTextControlSingleLine::~RenderTextControlSingleLine() = default;

inline HTMLElement* RenderTextControlSingleLine::innerSpinButtonElement() const
{
    return inputElement().innerSpinButtonElement();
}

static void resetOverriddenHeight(RenderBox* box, const RenderObject* ancestor)
{
    ASSERT(box != ancestor);
    if (!box || box->style().logicalHeight().isAuto())
        return; // Null box or its height was not overridden.
    box->mutableStyle().setLogicalHeight(Length { LengthType::Auto });
    for (RenderObject* renderer = box; renderer != ancestor; renderer = renderer->parent()) {
        ASSERT(renderer);
        renderer->setNeedsLayout(MarkOnlyThis);
    }
}

void RenderTextControlSingleLine::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    // FIXME: We should remove the height-related hacks in layout() and
    // styleDidChange(). We need them because we want to:
    // - Center the inner elements vertically if the input height is taller than
    //   the intrinsic height of the inner elements.
    // - Shrink the heights of the inner elements if the input height is smaller
    //   than the intrinsic heights of the inner elements.
    // - Make the height of the container element equal to the intrinsic height of
    //   the inner elements when the field has a strong password button.
    //
    // We don't honor padding and borders for textfields without decorations
    // and type=search if the text height is taller than the contentHeight()
    // because of compability.

    RenderTextControlInnerBlock* innerTextRenderer = innerTextElement() ? innerTextElement()->renderer() : nullptr;
    RenderBox* innerBlockRenderer = innerBlockElement() ? innerBlockElement()->renderBox() : nullptr;
    HTMLElement* container = containerElement();
    RenderBox* containerRenderer = container ? container->renderBox() : nullptr;

    // To ensure consistency between layouts, we need to reset any conditionally overridden height.
    resetOverriddenHeight(innerTextRenderer, this);
    resetOverriddenHeight(innerBlockRenderer, this);
    resetOverriddenHeight(containerRenderer, this);

    // Save the old size of the inner text (if we have one) as we will need to layout the placeholder
    // and update selection if it changes. One way the size may change is if text decorations are
    // toggled. For example, hiding and showing the caps lock indicator will cause a size change.
    LayoutSize oldInnerTextSize;
    if (innerTextRenderer)
        oldInnerTextSize = innerTextRenderer->size();

    RenderBlockFlow::layoutBlock(false);

    // Set the text block height
    LayoutUnit inputContentBoxLogicalHeight = logicalHeight() - borderAndPaddingLogicalHeight();
    LayoutUnit logicalHeightLimit = logicalHeight();
    LayoutUnit innerTextLogicalHeight;
    if (innerTextRenderer)
        innerTextLogicalHeight = innerTextRenderer->logicalHeight();

    auto shrinkInnerTextRendererIfNeeded = [&] {
        if (!innerTextRenderer || innerTextLogicalHeight <= logicalHeightLimit)
            return;
        // Let the simple (non-container based) inner text overflow (and clip) to be able to center it.
        if (!containerRenderer)
            return;

        if (inputContentBoxLogicalHeight != innerTextLogicalHeight)
            setNeedsLayout(MarkOnlyThis);

        innerTextRenderer->mutableStyle().setLogicalHeight(Length(inputContentBoxLogicalHeight, LengthType::Fixed));
        innerTextRenderer->setNeedsLayout(MarkOnlyThis);
        if (innerBlockRenderer) {
            innerBlockRenderer->mutableStyle().setLogicalHeight(Length(inputContentBoxLogicalHeight, LengthType::Fixed));
            innerBlockRenderer->setNeedsLayout(MarkOnlyThis);
        }
        innerTextLogicalHeight = inputContentBoxLogicalHeight;
    };
    shrinkInnerTextRendererIfNeeded();

    // The container might be taller because of decoration elements.
    LayoutUnit oldContainerLogicalTop;
    if (containerRenderer) {
        containerRenderer->layoutIfNeeded();
        oldContainerLogicalTop = containerRenderer->logicalTop();
        LayoutUnit containerLogicalHeight = containerRenderer->logicalHeight();

        auto* autoFillStrongPasswordButtonRenderer = [&]() -> RenderBox* {
            if (!inputElement().hasAutoFillStrongPasswordButton())
                return nullptr;

            auto* autoFillButtonElement = inputElement().autoFillButtonElement();
            if (!autoFillButtonElement)
                return nullptr;

            return autoFillButtonElement->renderBox();
        }();

        if (autoFillStrongPasswordButtonRenderer && innerTextRenderer && innerBlockRenderer) {
            auto newContainerHeight = innerTextLogicalHeight;

            // Don't expand the container height if the AutoFill button is wrapped onto a new line.
            if (autoFillStrongPasswordButtonRenderer->logicalTop() < innerBlockRenderer->logicalBottom())
                newContainerHeight = std::max<LayoutUnit>(newContainerHeight, autoFillStrongPasswordButtonRenderer->logicalHeight());

            containerRenderer->mutableStyle().setLogicalHeight(Length { newContainerHeight, LengthType::Fixed });
            setNeedsLayout(MarkOnlyThis);
        } else if (containerLogicalHeight > logicalHeightLimit) {
            containerRenderer->mutableStyle().setLogicalHeight(Length(logicalHeightLimit, LengthType::Fixed));
            setNeedsLayout(MarkOnlyThis);
        } else if (containerRenderer->logicalHeight() < contentLogicalHeight()) {
            containerRenderer->mutableStyle().setLogicalHeight(Length(contentLogicalHeight(), LengthType::Fixed));
            setNeedsLayout(MarkOnlyThis);
        } else
            containerRenderer->mutableStyle().setLogicalHeight(Length(containerLogicalHeight, LengthType::Fixed));
    }

    // If we need another layout pass, we have changed one of children's height so we need to relayout them.
    if (needsLayout())
        RenderBlockFlow::layoutBlock(true);

    // Fix up the y-position of the container as it may have been flexed when the strong password or strong
    // confirmation password button wraps to the next line.
    if (inputElement().hasAutoFillStrongPasswordButton() && containerRenderer)
        containerRenderer->setLogicalTop(oldContainerLogicalTop);

    // Center the child block in the block progression direction (vertical centering for horizontal text fields).
    auto centerRendererIfNeeded = [&](RenderBox& renderer) {
        auto height = [&] {
            if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(renderer)) {
                if (auto lineBox = InlineIterator::firstLineBoxFor(*blockFlow))
                    return LayoutUnit { std::max(lineBox->logicalHeight(), lineBox->contentLogicalHeight()) };
            }
            return renderer.logicalHeight();
        }();
        auto contentBoxHeight = contentLogicalHeight();
        if (contentBoxHeight == height)
            return;
        renderer.setLogicalTop(renderer.logicalTop() + (contentBoxHeight / 2 - height / 2));
    };
    if (!container && innerTextRenderer)
        centerRendererIfNeeded(*innerTextRenderer);
    else if (container && containerRenderer)
        centerRendererIfNeeded(*containerRenderer);

    bool innerTextSizeChanged = innerTextRenderer && innerTextRenderer->size() != oldInnerTextSize;

    HTMLElement* placeholderElement = inputElement().placeholderElement();
    if (RenderBox* placeholderBox = placeholderElement ? placeholderElement->renderBox() : 0) {
        auto innerTextWidth = LayoutUnit { };
        if (innerTextRenderer)
            innerTextWidth = innerTextRenderer->logicalWidth();
        placeholderBox->mutableStyle().setWidth(Length(innerTextWidth - placeholderBox->horizontalBorderAndPaddingExtent(), LengthType::Fixed));
        bool neededLayout = placeholderBox->needsLayout();
        bool placeholderBoxHadLayout = placeholderBox->everHadLayout();
        if (innerTextSizeChanged) {
            // The caps lock indicator was hidden. Layout the placeholder. Its layout does not affect its parent.
            placeholderBox->setChildNeedsLayout(MarkOnlyThis);
        }
        placeholderBox->layoutIfNeeded();
        auto placeholderTopLeft = containerRenderer ? containerRenderer->location() : LayoutPoint { };
        auto* innerBlockRenderer = innerBlockElement() ? innerBlockElement()->renderBox() : nullptr;
        if (innerBlockRenderer)
            placeholderTopLeft += toLayoutSize(innerBlockRenderer->location());
        if (innerTextRenderer)
            placeholderTopLeft += toLayoutSize(innerTextRenderer->location());
        placeholderBox->setLogicalLeft(placeholderTopLeft.x());
        // Here the container box indicates the renderer that the placeholder content is aligned with (no parent and/or containing block relationship).
        auto* containerBox = innerTextRenderer ? innerTextRenderer : innerBlockRenderer ? innerBlockRenderer : containerRenderer;
        if (containerBox) {
            auto placeholderHeight = [&] {
                if (auto* blockFlow = dynamicDowncast<RenderBlockFlow>(*placeholderBox)) {
                    if (auto placeholderLineBox = InlineIterator::firstLineBoxFor(*blockFlow))
                        return LayoutUnit { std::max(placeholderLineBox->logicalHeight(), placeholderLineBox->contentLogicalHeight()) };
                }
                return placeholderBox->logicalHeight();
            };
            // Center vertical align the placeholder content.
            auto logicalTop = placeholderTopLeft.y() + (containerBox->logicalHeight() / 2 - placeholderHeight() / 2);
            placeholderBox->setLogicalTop(logicalTop);
        }
        if (!placeholderBoxHadLayout && placeholderBox->checkForRepaintDuringLayout()) {
            // This assumes a shadow tree without floats. If floats are added, the
            // logic should be shared with RenderBlock::layoutBlockChild.
            placeholderBox->repaint();
        }
        // The placeholder gets layout last, after the parent text control and its other children,
        // so in order to get the correct overflow from the placeholder we need to recompute it now.
        if (neededLayout)
            computeOverflow(clientLogicalBottom());
    }

#if PLATFORM(IOS_FAMILY)
    // FIXME: We should not be adjusting styles during layout. <rdar://problem/7675493>
    if (inputElement().isSearchField())
        RenderThemeIOS::adjustRoundBorderRadius(mutableStyle(), *this);
#endif
    if (innerTextSizeChanged && frame().selection().isFocusedAndActive() && document().focusedElement() == &inputElement()) {
        // The caps lock indicator was hidden or shown. If it is now visible then it may be occluding
        // the current selection (say, the caret was after the last character in the text field).
        // Schedule an update and reveal of the current selection.
        frame().selection().setNeedsSelectionUpdate(FrameSelection::RevealSelectionAfterUpdate::Forced);
    }
}

bool RenderTextControlSingleLine::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderTextControl::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;

    // Say that we hit the inner text element if
    //  - we hit a node inside the inner text element,
    //  - we hit the <input> element (e.g. we're over the border or padding), or
    //  - we hit regions not in any decoration buttons.
    HTMLElement* container = containerElement();
    if (result.innerNode()->isDescendantOf(innerTextElement().get()) || result.innerNode() == &inputElement() || (container && container == result.innerNode())) {
        LayoutPoint pointInParent = locationInContainer.point();
        if (container && innerBlockElement()) {
            if (innerBlockElement()->renderBox())
                pointInParent -= toLayoutSize(innerBlockElement()->renderBox()->location());
            if (container->renderBox())
                pointInParent -= toLayoutSize(container->renderBox()->location());
        }
        hitInnerTextElement(result, pointInParent, accumulatedOffset);
    }
    return true;
}

void RenderTextControlSingleLine::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderTextControl::styleDidChange(diff, oldStyle);

    // We may have set the width and the height in the old style in layout().
    // Reset them now to avoid getting a spurious layout hint.
    HTMLElement* innerBlock = innerBlockElement();
    if (auto* innerBlockRenderer = innerBlock ? innerBlock->renderer() : nullptr) {
        innerBlockRenderer->mutableStyle().setHeight(Length());
        innerBlockRenderer->mutableStyle().setWidth(Length());
    }
    HTMLElement* container = containerElement();
    if (auto* containerRenderer = container ? container->renderer() : nullptr) {
        containerRenderer->mutableStyle().setHeight(Length());
        containerRenderer->mutableStyle().setWidth(Length());
    }
    if (diff == StyleDifference::Layout) {
        if (auto innerTextRenderer = innerTextElement()->renderer())
            innerTextRenderer->setNeedsLayout(MarkContainingBlockChain);
        if (auto* placeholder = inputElement().placeholderElement()) {
            if (placeholder->renderer())
                placeholder->renderer()->setNeedsLayout(MarkContainingBlockChain);
        }
    }
    setHasNonVisibleOverflow(false);
}

bool RenderTextControlSingleLine::hasControlClip() const
{
    auto shouldClipInnerContent = [&] {
        // Apply control clip for text fields with decorations.
        RenderBox* innerRenderBox = nullptr;
        if (auto* containerElement = inputElement().containerElement())
            innerRenderBox = containerElement->renderBox();

        if (!innerRenderBox && inputElement().placeholderElement())
            innerRenderBox = inputElement().placeholderElement()->renderBox();

        if (!innerRenderBox && inputElement().innerTextElement())
            innerRenderBox = inputElement().innerTextElement()->renderBox();

        return !!innerRenderBox;
    };
    return shouldClipInnerContent();
}

LayoutRect RenderTextControlSingleLine::controlClipRect(const LayoutPoint& additionalOffset) const
{
    ASSERT(hasControlClip());
    auto clipRect = paddingBoxRect();
    if (auto* containerElementRenderer = containerElement() ? containerElement()->renderBox() : nullptr)
        clipRect = unionRect(clipRect, containerElementRenderer->frameRect());
    clipRect.moveBy(additionalOffset);
    return clipRect;
}

float RenderTextControlSingleLine::getAverageCharWidth()
{
#if !PLATFORM(IOS_FAMILY)
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 901 is the avgCharWidth value in the OS/2 table for MS Shell Dlg.
    if (style().fontCascade().firstFamily() == "Lucida Grande"_s)
        return scaleEmToUnits(901);
#endif

    return RenderTextControl::getAverageCharWidth();
}

LayoutUnit RenderTextControlSingleLine::preferredContentLogicalWidth(float charWidth) const
{
    int factor = 0;
    bool includesDecoration = inputElement().sizeShouldIncludeDecoration(factor);
    if (factor <= 0)
        factor = 20;

    LayoutUnit result = LayoutUnit::fromFloatCeil(charWidth * factor);

    float maxCharWidth = 0.f;

#if !PLATFORM(IOS_FAMILY)
    const AtomString& family = style().fontCascade().firstFamily();
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 4027 is the (xMax - xMin) value in the "head" font table for MS Shell Dlg.
    if (family == "Lucida Grande"_s)
        maxCharWidth = scaleEmToUnits(4027);
    else if (style().fontCascade().hasValidAverageCharWidth())
        maxCharWidth = roundf(style().fontCascade().primaryFont().maxCharWidth());
#endif

    // For text inputs, IE adds some extra width.
    if (maxCharWidth > 0.f)
        result += maxCharWidth - charWidth;

    if (includesDecoration)
        result += inputElement().decorationWidth();

    if (auto* innerRenderer = innerTextElement() ? innerTextElement()->renderer() : nullptr)
        result += innerRenderer->endPaddingWidthForCaret();

    return result;
}

LayoutUnit RenderTextControlSingleLine::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    return lineHeight + nonContentHeight;
}

void RenderTextControlSingleLine::autoscroll(const IntPoint& position)
{
    RenderTextControlInnerBlock* renderer = innerTextElement()->renderer();
    if (!renderer)
        return;
    RenderLayer* layer = renderer->layer();
    if (layer)
        layer->autoscroll(position);
}

int RenderTextControlSingleLine::scrollWidth() const
{
    if (auto* innerTextRenderer = innerTextElement() ? innerTextElement()->renderer() : nullptr) {
        // Adjust scrollWidth to inculde input element horizontal paddings and decoration width.
        auto adjustment = clientWidth() - innerTextRenderer->clientWidth();
        return innerTextRenderer->scrollWidth() + adjustment;
    }
    return RenderBlockFlow::scrollWidth();
}

int RenderTextControlSingleLine::scrollHeight() const
{
    if (auto* innerTextRenderer = innerTextElement() ? innerTextElement()->renderer() : nullptr) {
        // Adjust scrollHeight to inculde input element vertical paddings and decoration height.
        auto adjustment = clientHeight() - innerTextRenderer->clientHeight();
        return innerTextRenderer->scrollHeight() + adjustment;
    }
    return RenderBlockFlow::scrollHeight();
}

int RenderTextControlSingleLine::scrollLeft() const
{
    if (auto innerTextElement = this->innerTextElement(); innerTextElement && innerTextElement->renderer())
        return innerTextElement->renderer()->scrollLeft();
    return RenderBlockFlow::scrollLeft();
}

int RenderTextControlSingleLine::scrollTop() const
{
    if (auto innerTextElement = this->innerTextElement(); innerTextElement && innerTextElement->renderer())
        return innerTextElement->renderer()->scrollTop();
    return RenderBlockFlow::scrollTop();
}

void RenderTextControlSingleLine::setScrollLeft(int newLeft, const ScrollPositionChangeOptions&)
{
    if (innerTextElement())
        innerTextElement()->setScrollLeft(newLeft);
}

void RenderTextControlSingleLine::setScrollTop(int newTop, const ScrollPositionChangeOptions&)
{
    if (innerTextElement())
        innerTextElement()->setScrollTop(newTop);
}

bool RenderTextControlSingleLine::scroll(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement, RenderBox* startBox, const IntPoint& wheelEventAbsolutePoint)
{
    auto* renderer = innerTextElement()->renderer();
    if (!renderer)
        return false;
    auto* scrollableArea = renderer->layer() ? renderer->layer()->scrollableArea() : nullptr;
    if (scrollableArea && scrollableArea->scroll(direction, granularity, stepCount))
        return true;
    return RenderBlockFlow::scroll(direction, granularity, stepCount, stopElement, startBox, wheelEventAbsolutePoint);
}

bool RenderTextControlSingleLine::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement)
{
    auto* layer = innerTextElement()->renderer()->layer();
    auto* scrollableArea = layer ? layer->scrollableArea() : nullptr;
    if (scrollableArea && scrollableArea->scroll(logicalToPhysical(direction, style().isHorizontalWritingMode(), style().isFlippedBlocksWritingMode()), granularity, stepCount))
        return true;
    return RenderBlockFlow::logicalScroll(direction, granularity, stepCount, stopElement);
}

HTMLInputElement& RenderTextControlSingleLine::inputElement() const
{
    return downcast<HTMLInputElement>(RenderTextControl::textFormControlElement());
}

RenderTextControlInnerBlock::RenderTextControlInnerBlock(Element& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
}

}
