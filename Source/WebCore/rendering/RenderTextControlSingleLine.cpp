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
#include "ContainerNodeInlines.h"
#include "Font.h"
#include "FontCascadeInlines.h"
#include "FrameSelection.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "InlineIteratorLineBox.h"
#include "LayoutScope.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "PlatformRenderTheme.h"
#include "RenderBlockFlowInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderScrollbar.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleResolver.h"
#include "TextControlInnerElements.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTextControlSingleLine);
WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTextControlInnerBlock);

RenderTextControlSingleLine::RenderTextControlSingleLine(Type type, HTMLInputElement& element, UniqueRef<Style::ComputedStyle>&& style)
    : RenderTextControl(type, element, WTF::move(style))
{
    ASSERT(isRenderTextControlSingleLine());
}

RenderTextControlSingleLine::~RenderTextControlSingleLine() = default;

inline HTMLElement* RenderTextControlSingleLine::innerSpinButtonElement() const
{
    return protect(inputElement())->innerSpinButtonElement();
}

void RenderTextControlSingleLine::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    if (simplifiedLayout())
        return;

    CheckedPtr innerTextRenderer = this->innerTextRenderer();
    RefPtr container = containerElement();
    CheckedPtr containerRenderer = container ? container->renderBox() : nullptr;

    // Save the old size of the inner text (if we have one) as we will need to layout the placeholder
    // and update selection if it changes. One way the size may change is if text decorations are
    // toggled. For example, hiding and showing the caps lock indicator will cause a size change.
    LayoutSize oldInnerTextSize;
    if (innerTextRenderer)
        oldInnerTextSize = innerTextRenderer->borderBoxSize();

    {
        auto scope = LayoutScope { *this };
        RenderFlexibleBox::layoutBlock(RelayoutChildren::No);
    }

    bool innerTextSizeChanged = innerTextRenderer && innerTextRenderer->borderBoxSize() != oldInnerTextSize;

    RefPtr placeholderElement = protect(inputElement())->placeholderElement();
    if (CheckedPtr placeholderBox = placeholderElement ? placeholderElement->renderBox() : nullptr) {
        auto innerTextWidth = LayoutUnit { };
        auto usedZoomForLength = placeholderBox->style().usedZoomForLength().value;
        if (innerTextRenderer)
            innerTextWidth = innerTextRenderer->logicalWidth();
        protect(placeholderBox->mutableStyle())->setWidth(Style::PreferredSize::Fixed { (innerTextWidth - placeholderBox->horizontalBorderAndPaddingExtent()) / usedZoomForLength });
        bool neededLayout = placeholderBox->needsLayout();
        bool placeholderBoxEverHadLayout = placeholderBox->everHadLayout();
        if (innerTextSizeChanged) {
            // The caps lock indicator was hidden. Layout the placeholder. Its layout does not affect its parent.
            placeholderBox->setChildNeedsLayout(MarkingBehavior::OnlyThis);
        }
        placeholderBox->layoutIfNeeded();
        auto placeholderTopLeft = containerRenderer ? containerRenderer->location() : LayoutPoint { };
        CheckedPtr innerBlockRenderer = innerBlockElement() ? innerBlockElement()->renderBox() : nullptr;
        if (innerBlockRenderer)
            placeholderTopLeft += toLayoutSize(innerBlockRenderer->location());
        if (innerTextRenderer)
            placeholderTopLeft += toLayoutSize(innerTextRenderer->location());
        auto oldPlaceholderLocation = placeholderBox->location();
        placeholderBox->setLogicalLeft(placeholderTopLeft.x());
        // Here the container box indicates the renderer that the placeholder content is aligned with (no parent and/or containing block relationship).
        CheckedPtr<RenderBox> containerBox = innerTextRenderer ? innerTextRenderer.get() : innerBlockRenderer ? innerBlockRenderer.get() : containerRenderer.get();
        if (containerBox) {
            auto placeholderHeight = [&] {
                if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(*placeholderBox)) {
                    if (auto placeholderLineBox = InlineIterator::firstLineBoxFor(*blockFlow))
                        return LayoutUnit { std::max(placeholderLineBox->logicalHeight(), placeholderLineBox->contentLogicalHeight()) };
                }
                return placeholderBox->logicalHeight();
            };
            // Center vertical align the placeholder content.
            auto logicalTop = placeholderTopLeft.y() + (containerBox->logicalHeight() / 2 - placeholderHeight() / 2);
            placeholderBox->setLogicalTop(logicalTop);
        }
        if (!placeholderBoxEverHadLayout && placeholderBox->checkForRepaintDuringLayout()) {
            // This assumes a shadow tree without floats. If floats are added, the logic should be shared with RenderBlock::layoutBlockChild.
            placeholderBox->repaint();
        }
        // The placeholder gets layout last, after the parent text control and its other children,
        // so in order to get the correct overflow from the placeholder we need to recompute it now.
        if (neededLayout) {
            computeInFlowOverflow(blockStartRelativeContentBoxRect());
            addOverflowFromOutOfFlowBoxes();
        } else if (auto* placeholderLayer = placeholderBox->layer(); placeholderLayer && placeholderBox->location() != oldPlaceholderLocation)
            placeholderLayer->setNeedsPositionUpdate();
    }

    if (innerTextSizeChanged && frame().selection().isFocusedAndActive() && document().focusedElement() == &inputElement()) {
        // The caps lock indicator was hidden or shown. If it is now visible then it may be occluding
        // the current selection (say, the caret was after the last character in the text field).
        // Schedule an update and reveal of the current selection.
        protect(frame().selection())->setNeedsSelectionUpdate(FrameSelection::RevealSelectionAfterUpdate::Forced);
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
    RefPtr container = containerElement();
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

void RenderTextControlSingleLine::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderTextControl::styleDidChange(diff, oldStyle);

    if (diff == Style::DifferenceResult::Layout) {
        if (CheckedPtr innerTextRenderer = this->innerTextRenderer())
            innerTextRenderer->setNeedsLayout(MarkingBehavior::ContainingBlockChain);
        if (RefPtr placeholder = protect(inputElement())->placeholderElement()) {
            if (CheckedPtr placeholderRenderer = placeholder->renderer())
                placeholderRenderer->setNeedsLayout(MarkingBehavior::ContainingBlockChain);
        }
    }
    setHasNonVisibleOverflow(false);
}

bool RenderTextControlSingleLine::hasControlClip() const
{
    auto shouldClipInnerContent = [&] {
        // Apply control clip for text fields with decorations.
        CheckedPtr<RenderBox> innerRenderBox;
        if (auto* containerElement = protect(inputElement())->containerElement())
            innerRenderBox = containerElement->renderBox();

        if (!innerRenderBox && protect(inputElement())->placeholderElement())
            innerRenderBox = protect(inputElement())->placeholderElement()->renderBox();

        if (!innerRenderBox && protect(inputElement())->innerTextElement())
            innerRenderBox = protect(inputElement())->innerTextElement()->renderBox();

        return !!innerRenderBox;
    };
    return shouldClipInnerContent();
}

LayoutRect RenderTextControlSingleLine::controlClipRect(const LayoutPoint& additionalOffset) const
{
    ASSERT(hasControlClip());
    auto clipRect = paddingBoxRect();
    if (CheckedPtr containerElementRenderer = containerElement() ? containerElement()->renderBox() : nullptr)
        clipRect = unionRect(clipRect, containerElementRenderer->borderBoxRectInContainer());
    clipRect.moveBy(additionalOffset);
    return clipRect;
}

bool RenderTextControlSingleLine::innerTextElementHasNonVisibleOverflow() const
{
    if (CheckedPtr innerTextRenderer = this->innerTextRenderer())
        return innerTextRenderer->hasNonVisibleOverflow();
    return false;
}

float RenderTextControlSingleLine::getAverageCharWidth()
{
#if !PLATFORM(IOS_FAMILY)
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 901 is the avgCharWidth value in the OS/2 table for MS Shell Dlg.
    if (style().fontCascade().firstFamily().name == "Lucida Grande"_s)
        return scaleEmToUnits(901);
#endif

    return RenderTextControl::getAverageCharWidth();
}

LayoutUnit RenderTextControlSingleLine::preferredContentLogicalWidth(float charWidth) const
{
    int factor = 0;
    bool includesDecoration = protect(inputElement())->sizeShouldIncludeDecoration(factor);
    if (factor <= 0)
        factor = 20;

    LayoutUnit result = LayoutUnit::fromFloatCeil(charWidth * factor);

    float maxCharWidth = 0.f;

#if !PLATFORM(IOS_FAMILY)
    CheckedRef fontCascade = style().fontCascade();
    const auto& family = fontCascade->firstFamily().name;
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 4027 is the (xMax - xMin) value in the "head" font table for MS Shell Dlg.
    if (family == "Lucida Grande"_s)
        maxCharWidth = scaleEmToUnits(4027);
    else if (fontCascade->hasValidAverageCharWidth())
        maxCharWidth = roundf(fontCascade->primaryFont().maxCharWidth());
#endif

    // For text inputs, IE adds some extra width.
    if (maxCharWidth > 0.f)
        result += maxCharWidth - charWidth;

    if (includesDecoration)
        result += protect(inputElement())->decorationWidth(result);

    if (CheckedPtr innerTextRenderer = this->innerTextRenderer())
        result += innerTextRenderer->endPaddingWidthForCaret();

    return result;
}

LayoutUnit RenderTextControlSingleLine::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    return lineHeight + nonContentHeight;
}

void RenderTextControlSingleLine::autoscroll(const IntPoint& position)
{
    CheckedPtr innerTextRenderer = this->innerTextRenderer();
    if (!innerTextRenderer)
        return;
    if (CheckedPtr layer = innerTextRenderer->layer())
        layer->autoscroll(position);
}

int RenderTextControlSingleLine::scrollWidth() const
{
    if (CheckedPtr innerTextRenderer = this->innerTextRenderer()) {
        // Adjust scrollWidth to inculde input element horizontal paddings and decoration width.
        auto adjustment = paddingBoxWidth() - innerTextRenderer->paddingBoxWidth();
        return innerTextRenderer->scrollWidth() + adjustment;
    }
    return RenderFlexibleBox::scrollWidth();
}

int RenderTextControlSingleLine::scrollHeight() const
{
    if (CheckedPtr innerTextRenderer = this->innerTextRenderer()) {
        // Adjust scrollHeight to inculde input element vertical paddings and decoration height.
        auto adjustment = paddingBoxHeight() - innerTextRenderer->paddingBoxHeight();
        return innerTextRenderer->scrollHeight() + adjustment;
    }
    return RenderFlexibleBox::scrollHeight();
}

int RenderTextControlSingleLine::scrollLeft() const
{
    if (CheckedPtr innerTextRenderer = this->innerTextRenderer())
        return innerTextRenderer->scrollLeft();
    return RenderFlexibleBox::scrollLeft();
}

int RenderTextControlSingleLine::scrollTop() const
{
    if (CheckedPtr innerTextRenderer = this->innerTextRenderer())
        return innerTextRenderer->scrollTop();
    return RenderFlexibleBox::scrollTop();
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

void RenderTextControlSingleLine::setScrollPosition(const ScrollPosition& position, const ScrollPositionChangeOptions& options)
{
    if (CheckedPtr innerTextRenderer = this->innerTextRenderer())
        innerTextRenderer->setScrollPosition(position, options);
}

bool RenderTextControlSingleLine::scroll(ScrollDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement, RenderBox* startBox, const IntPoint& wheelEventAbsolutePoint)
{
    CheckedPtr innerTextRenderer = this->innerTextRenderer();
    if (!innerTextRenderer)
        return false;
    CheckedPtr scrollableArea = innerTextRenderer->layer() ? innerTextRenderer->layer()->scrollableArea() : nullptr;
    if (scrollableArea && scrollableArea->scroll(direction, granularity, stepCount))
        return true;
    return RenderFlexibleBox::scroll(direction, granularity, stepCount, stopElement, startBox, wheelEventAbsolutePoint);
}

bool RenderTextControlSingleLine::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, unsigned stepCount, Element** stopElement)
{
    CheckedPtr innerTextRenderer = this->innerTextRenderer();
    if (!innerTextRenderer)
        return false;
    CheckedPtr layer = innerTextRenderer->layer();
    CheckedPtr scrollableArea = layer ? layer->scrollableArea() : nullptr;
    if (scrollableArea && scrollableArea->scroll(logicalToPhysical(direction, writingMode().isHorizontal(), writingMode().isBlockFlipped()), granularity, stepCount))
        return true;
    return RenderFlexibleBox::logicalScroll(direction, granularity, stepCount, stopElement);
}

HTMLInputElement& RenderTextControlSingleLine::inputElement() const
{
    return downcast<HTMLInputElement>(RenderTextControl::textFormControlElement());
}

RenderTextControlInnerBlock* RenderTextControlSingleLine::innerTextRenderer() const
{
    return innerTextElement() ? innerTextElement()->renderer() : nullptr;
}

RenderTextControlInnerBlock::RenderTextControlInnerBlock(Element& element, UniqueRef<Style::ComputedStyle>&& style)
    : RenderBlockFlow(Type::TextControlInnerBlock, element, WTF::move(style))
{
    ASSERT(isRenderTextControlInnerBlock());
}

RenderTextControlInnerBlock::~RenderTextControlInnerBlock() = default;

bool RenderTextControlInnerBlock::requiresLayerForScreenCaptureProtection() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(element()->shadowHost());
    return input && input->requiresLayerForScreenCaptureProtection();
}

bool RenderTextControlInnerBlock::shouldHideFromScreenCapture() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(element()->shadowHost());
    return input && input->shouldHideFromScreenCapture();
}

bool RenderTextControlInnerBlock::canBeProgramaticallyScrolled() const
{
    if (auto* shadowHost = dynamicDowncast<HTMLInputElement>(element()->shadowHost()))
        return !shadowHost->hasAutofillStrongPasswordButton();
    return true;
}

}
