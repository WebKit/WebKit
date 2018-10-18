/**
 * Copyright (C) 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/) 
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "RenderLayer.h"
#include "RenderScrollbar.h"
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

void RenderTextControlSingleLine::centerRenderer(RenderBox& renderer) const
{
    LayoutUnit logicalHeightDiff = renderer.logicalHeight() - contentLogicalHeight();
    renderer.setLogicalTop(renderer.logicalTop() - logicalHeightDiff / 2);
}

static void resetOverriddenHeight(RenderBox* box, const RenderObject* ancestor)
{
    ASSERT(box != ancestor);
    if (!box || box->style().logicalHeight().isAuto())
        return; // Null box or its height was not overridden.
    box->mutableStyle().setLogicalHeight(Length { Auto });
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
    // We don't honor paddings and borders for textfields without decorations
    // and type=search if the text height is taller than the contentHeight()
    // because of compability.

    RenderTextControlInnerBlock* innerTextRenderer = innerTextElement()->renderer();
    RenderBox* innerBlockRenderer = innerBlockElement() ? innerBlockElement()->renderBox() : nullptr;
    HTMLElement* container = containerElement();
    RenderBox* containerRenderer = container ? container->renderBox() : nullptr;

    // To ensure consistency between layouts, we need to reset any conditionally overridden height.
    resetOverriddenHeight(innerTextRenderer, this);
    resetOverriddenHeight(innerBlockRenderer, this);
    resetOverriddenHeight(containerRenderer, this);

    RenderBlockFlow::layoutBlock(false);

    // Set the text block height
    LayoutUnit desiredLogicalHeight = textBlockLogicalHeight();
    LayoutUnit logicalHeightLimit = logicalHeight();
    LayoutUnit innerTextLogicalHeight;
    if (innerTextRenderer)
        innerTextLogicalHeight = innerTextRenderer->logicalHeight();
    if (innerTextRenderer && innerTextLogicalHeight > logicalHeightLimit) {
        if (desiredLogicalHeight != innerTextLogicalHeight)
            setNeedsLayout(MarkOnlyThis);

        innerTextRenderer->mutableStyle().setLogicalHeight(Length(desiredLogicalHeight, Fixed));
        innerTextRenderer->setNeedsLayout(MarkOnlyThis);
        if (innerBlockRenderer) {
            innerBlockRenderer->mutableStyle().setLogicalHeight(Length(desiredLogicalHeight, Fixed));
            innerBlockRenderer->setNeedsLayout(MarkOnlyThis);
        }
        innerTextLogicalHeight = desiredLogicalHeight;
    }
    // The container might be taller because of decoration elements.
    LayoutUnit oldContainerLogicalTop;
    if (containerRenderer) {
        containerRenderer->layoutIfNeeded();
        oldContainerLogicalTop = containerRenderer->logicalTop();
        LayoutUnit containerLogicalHeight = containerRenderer->logicalHeight();
        if (inputElement().hasAutoFillStrongPasswordButton() && innerTextRenderer && containerLogicalHeight != innerTextLogicalHeight) {
            containerRenderer->mutableStyle().setLogicalHeight(Length { innerTextLogicalHeight, Fixed });
            setNeedsLayout(MarkOnlyThis);
        } else if (containerLogicalHeight > logicalHeightLimit) {
            containerRenderer->mutableStyle().setLogicalHeight(Length(logicalHeightLimit, Fixed));
            setNeedsLayout(MarkOnlyThis);
        } else if (containerRenderer->logicalHeight() < contentLogicalHeight()) {
            containerRenderer->mutableStyle().setLogicalHeight(Length(contentLogicalHeight(), Fixed));
            setNeedsLayout(MarkOnlyThis);
        } else
            containerRenderer->mutableStyle().setLogicalHeight(Length(containerLogicalHeight, Fixed));
    }

    // If we need another layout pass, we have changed one of children's height so we need to relayout them.
    if (needsLayout())
        RenderBlockFlow::layoutBlock(true);

    // Fix up the y-position of the container as it may have been flexed when the strong password or strong
    // confirmation password button wraps to the next line.
    if (inputElement().hasAutoFillStrongPasswordButton() && containerRenderer)
        containerRenderer->setLogicalTop(oldContainerLogicalTop);

    // Center the child block in the block progression direction (vertical centering for horizontal text fields).
    if (!container && innerTextRenderer && innerTextRenderer->height() != contentLogicalHeight())
        centerRenderer(*innerTextRenderer);
    else if (container && containerRenderer && containerRenderer->height() != contentLogicalHeight())
        centerRenderer(*containerRenderer);

    HTMLElement* placeholderElement = inputElement().placeholderElement();
    if (RenderBox* placeholderBox = placeholderElement ? placeholderElement->renderBox() : 0) {
        LayoutSize innerTextSize;
        if (innerTextRenderer)
            innerTextSize = innerTextRenderer->size();
        placeholderBox->mutableStyle().setWidth(Length(innerTextSize.width() - placeholderBox->horizontalBorderAndPaddingExtent(), Fixed));
        placeholderBox->mutableStyle().setHeight(Length(innerTextSize.height() - placeholderBox->verticalBorderAndPaddingExtent(), Fixed));
        bool neededLayout = placeholderBox->needsLayout();
        bool placeholderBoxHadLayout = placeholderBox->everHadLayout();
        placeholderBox->layoutIfNeeded();
        LayoutPoint textOffset;
        if (innerTextRenderer)
            textOffset = innerTextRenderer->location();
        if (innerBlockElement() && innerBlockElement()->renderBox())
            textOffset += toLayoutSize(innerBlockElement()->renderBox()->location());
        if (containerRenderer)
            textOffset += toLayoutSize(containerRenderer->location());
        placeholderBox->setLocation(textOffset);

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
    setHasOverflowClip(false);
}

bool RenderTextControlSingleLine::hasControlClip() const
{
    // Apply control clip for text fields with decorations.
    return !!containerElement();
}

LayoutRect RenderTextControlSingleLine::controlClipRect(const LayoutPoint& additionalOffset) const
{
    ASSERT(hasControlClip());
    LayoutRect clipRect = contentBoxRect();
    if (containerElement()->renderBox())
        clipRect = unionRect(clipRect, containerElement()->renderBox()->frameRect());
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
    if (style().fontCascade().firstFamily() == "Lucida Grande")
        return scaleEmToUnits(901);
#endif

    return RenderTextControl::getAverageCharWidth();
}

LayoutUnit RenderTextControlSingleLine::preferredContentLogicalWidth(float charWidth) const
{
    int factor;
    bool includesDecoration = inputElement().sizeShouldIncludeDecoration(factor);
    if (factor <= 0)
        factor = 20;

    LayoutUnit result = LayoutUnit::fromFloatCeil(charWidth * factor);

    float maxCharWidth = 0.f;

#if !PLATFORM(IOS_FAMILY)
    const AtomicString& family = style().fontCascade().firstFamily();
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 4027 is the (xMax - xMin) value in the "head" font table for MS Shell Dlg.
    if (family == "Lucida Grande")
        maxCharWidth = scaleEmToUnits(4027);
    else if (style().fontCascade().hasValidAverageCharWidth())
        maxCharWidth = roundf(style().fontCascade().primaryFont().maxCharWidth());
#endif

    // For text inputs, IE adds some extra width.
    if (maxCharWidth > 0.f)
        result += maxCharWidth - charWidth;

    if (includesDecoration)
        result += inputElement().decorationWidth();

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
    if (innerTextElement())
        return innerTextElement()->scrollWidth();
    return RenderBlockFlow::scrollWidth();
}

int RenderTextControlSingleLine::scrollHeight() const
{
    if (innerTextElement())
        return innerTextElement()->scrollHeight();
    return RenderBlockFlow::scrollHeight();
}

int RenderTextControlSingleLine::scrollLeft() const
{
    if (innerTextElement())
        return innerTextElement()->scrollLeft();
    return RenderBlockFlow::scrollLeft();
}

int RenderTextControlSingleLine::scrollTop() const
{
    if (innerTextElement())
        return innerTextElement()->scrollTop();
    return RenderBlockFlow::scrollTop();
}

void RenderTextControlSingleLine::setScrollLeft(int newLeft, ScrollClamping)
{
    if (innerTextElement())
        innerTextElement()->setScrollLeft(newLeft);
}

void RenderTextControlSingleLine::setScrollTop(int newTop, ScrollClamping)
{
    if (innerTextElement())
        innerTextElement()->setScrollTop(newTop);
}

bool RenderTextControlSingleLine::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier, Element** stopElement, RenderBox* startBox, const IntPoint& wheelEventAbsolutePoint)
{
    RenderTextControlInnerBlock* renderer = innerTextElement()->renderer();
    if (!renderer)
        return false;
    RenderLayer* layer = renderer->layer();
    if (layer && layer->scroll(direction, granularity, multiplier))
        return true;
    return RenderBlockFlow::scroll(direction, granularity, multiplier, stopElement, startBox, wheelEventAbsolutePoint);
}

bool RenderTextControlSingleLine::logicalScroll(ScrollLogicalDirection direction, ScrollGranularity granularity, float multiplier, Element** stopElement)
{
    RenderLayer* layer = innerTextElement()->renderer()->layer();
    if (layer && layer->scroll(logicalToPhysical(direction, style().isHorizontalWritingMode(), style().isFlippedBlocksWritingMode()), granularity, multiplier))
        return true;
    return RenderBlockFlow::logicalScroll(direction, granularity, multiplier, stopElement);
}

HTMLInputElement& RenderTextControlSingleLine::inputElement() const
{
    return downcast<HTMLInputElement>(RenderTextControl::textFormControlElement());
}

}
