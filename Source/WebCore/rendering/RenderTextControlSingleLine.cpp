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
#include "Chrome.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "RenderLayer.h"
#include "RenderScrollbar.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleFontData.h"
#include "StyleResolver.h"
#include "TextControlInnerElements.h"
#include <wtf/StackStats.h>

#if PLATFORM(IOS)
#include "RenderThemeIOS.h"
#endif

namespace WebCore {

using namespace HTMLNames;

RenderTextControlSingleLine::RenderTextControlSingleLine(HTMLInputElement& element, PassRef<RenderStyle> style)
    : RenderTextControl(element, std::move(style))
    , m_shouldDrawCapsLockIndicator(false)
    , m_desiredInnerTextLogicalHeight(-1)
{
}

RenderTextControlSingleLine::~RenderTextControlSingleLine()
{
}

inline HTMLElement* RenderTextControlSingleLine::innerSpinButtonElement() const
{
    return inputElement().innerSpinButtonElement();
}

void RenderTextControlSingleLine::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    RenderTextControl::paint(paintInfo, paintOffset);

    if (paintInfo.phase == PaintPhaseBlockBackground && m_shouldDrawCapsLockIndicator) {
        LayoutRect contentsRect = contentBoxRect();

        // Center in the block progression direction.
        if (isHorizontalWritingMode())
            contentsRect.setY((height() - contentsRect.height()) / 2);
        else
            contentsRect.setX((width() - contentsRect.width()) / 2);

        // Convert the rect into the coords used for painting the content
        contentsRect.moveBy(paintOffset + location());
        theme().paintCapsLockIndicator(this, paintInfo, pixelSnappedIntRect(contentsRect));
    }
}

LayoutUnit RenderTextControlSingleLine::computeLogicalHeightLimit() const
{
    return containerElement() ? contentLogicalHeight() : logicalHeight();
}

void RenderTextControlSingleLine::centerRenderer(RenderBox& renderer) const
{
    LayoutUnit logicalHeightDiff = renderer.logicalHeight() - contentLogicalHeight();
    float center = logicalHeightDiff / 2;
    renderer.setLogicalTop(renderer.logicalTop() - LayoutUnit(round(center)));
}

static void setNeedsLayoutOnAncestors(RenderObject* start, RenderObject* ancestor)
{
    ASSERT(start != ancestor);
    for (RenderObject* renderer = start; renderer != ancestor; renderer = renderer->parent()) {
        ASSERT(renderer);
        renderer->setNeedsLayout(MarkOnlyThis);
    }
}

void RenderTextControlSingleLine::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    // FIXME: We should remove the height-related hacks in layout() and
    // styleDidChange(). We need them because
    // - Center the inner elements vertically if the input height is taller than
    //   the intrinsic height of the inner elements.
    // - Shrink the inner elment heights if the input height is samller than the
    //   intrinsic heights of the inner elements.

    // We don't honor paddings and borders for textfields without decorations
    // and type=search if the text height is taller than the contentHeight()
    // because of compability.

    RenderTextControlInnerBlock* innerTextRenderer = innerTextElement()->renderer();
    RenderBox* innerBlockRenderer = innerBlockElement() ? innerBlockElement()->renderBox() : 0;

    // To ensure consistency between layouts, we need to reset any conditionally overriden height.
    if (innerTextRenderer && !innerTextRenderer->style().logicalHeight().isAuto()) {
        innerTextRenderer->style().setLogicalHeight(Length(Auto));
        setNeedsLayoutOnAncestors(innerTextRenderer, this);
    }
    if (innerBlockRenderer && !innerBlockRenderer->style().logicalHeight().isAuto()) {
        innerBlockRenderer->style().setLogicalHeight(Length(Auto));
        setNeedsLayoutOnAncestors(innerBlockRenderer, this);
    }

    RenderBlockFlow::layoutBlock(false);

    HTMLElement* container = containerElement();
    RenderBox* containerRenderer = container ? container->renderBox() : 0;

    // Set the text block height
    LayoutUnit desiredLogicalHeight = textBlockLogicalHeight();
    LayoutUnit logicalHeightLimit = computeLogicalHeightLimit();
    if (innerTextRenderer && innerTextRenderer->logicalHeight() > logicalHeightLimit) {
        if (desiredLogicalHeight != innerTextRenderer->logicalHeight())
            setNeedsLayout(MarkOnlyThis);

        m_desiredInnerTextLogicalHeight = desiredLogicalHeight;

        innerTextRenderer->style().setLogicalHeight(Length(desiredLogicalHeight, Fixed));
        innerTextRenderer->setNeedsLayout(MarkOnlyThis);
        if (innerBlockRenderer) {
            innerBlockRenderer->style().setLogicalHeight(Length(desiredLogicalHeight, Fixed));
            innerBlockRenderer->setNeedsLayout(MarkOnlyThis);
        }
    }
    // The container might be taller because of decoration elements.
    if (containerRenderer) {
        containerRenderer->layoutIfNeeded();
        LayoutUnit containerLogicalHeight = containerRenderer->logicalHeight();
        if (containerLogicalHeight > logicalHeightLimit) {
            containerRenderer->style().setLogicalHeight(Length(logicalHeightLimit, Fixed));
            setNeedsLayout(MarkOnlyThis);
        } else if (containerRenderer->logicalHeight() < contentLogicalHeight()) {
            containerRenderer->style().setLogicalHeight(Length(contentLogicalHeight(), Fixed));
            setNeedsLayout(MarkOnlyThis);
        } else
            containerRenderer->style().setLogicalHeight(Length(containerLogicalHeight, Fixed));
    }

    // If we need another layout pass, we have changed one of children's height so we need to relayout them.
    if (needsLayout())
        RenderBlockFlow::layoutBlock(true);

    // Center the child block in the block progression direction (vertical centering for horizontal text fields).
    if (!container && innerTextRenderer && innerTextRenderer->height() != contentLogicalHeight())
        centerRenderer(*innerTextRenderer);
    else
        centerContainerIfNeeded(containerRenderer);

    // Ignores the paddings for the inner spin button.
    if (RenderBox* innerSpinBox = innerSpinButtonElement() ? innerSpinButtonElement()->renderBox() : 0) {
        RenderBox* parentBox = innerSpinBox->parentBox();
        if (containerRenderer && !containerRenderer->style().isLeftToRightDirection())
            innerSpinBox->setLogicalLocation(LayoutPoint(-paddingLogicalLeft(), -paddingBefore()));
        else
            innerSpinBox->setLogicalLocation(LayoutPoint(parentBox->logicalWidth() - innerSpinBox->logicalWidth() + paddingLogicalRight(), -paddingBefore()));
        innerSpinBox->setLogicalHeight(logicalHeight() - borderBefore() - borderAfter());
    }

    HTMLElement* placeholderElement = inputElement().placeholderElement();
    if (RenderBox* placeholderBox = placeholderElement ? placeholderElement->renderBox() : 0) {
        LayoutSize innerTextSize;
        if (innerTextRenderer)
            innerTextSize = innerTextRenderer->size();
        placeholderBox->style().setWidth(Length(innerTextSize.width() - placeholderBox->horizontalBorderAndPaddingExtent(), Fixed));
        placeholderBox->style().setHeight(Length(innerTextSize.height() - placeholderBox->verticalBorderAndPaddingExtent(), Fixed));
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

#if PLATFORM(IOS)
    // FIXME: We should not be adjusting styles during layout. <rdar://problem/7675493>
    if (inputElement().isSearchField())
        RenderThemeIOS::adjustRoundBorderRadius(style(), this);
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
    if (result.innerNode()->isDescendantOf(innerTextElement()) || result.innerNode() == &inputElement() || (container && container == result.innerNode())) {
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
    m_desiredInnerTextLogicalHeight = -1;
    RenderTextControl::styleDidChange(diff, oldStyle);

    // We may have set the width and the height in the old style in layout().
    // Reset them now to avoid getting a spurious layout hint.
    HTMLElement* innerBlock = innerBlockElement();
    if (RenderObject* innerBlockRenderer = innerBlock ? innerBlock->renderer() : 0) {
        innerBlockRenderer->style().setHeight(Length());
        innerBlockRenderer->style().setWidth(Length());
    }
    HTMLElement* container = containerElement();
    if (RenderObject* containerRenderer = container ? container->renderer() : 0) {
        containerRenderer->style().setHeight(Length());
        containerRenderer->style().setWidth(Length());
    }
    RenderTextControlInnerBlock* innerTextRenderer = innerTextElement()->renderer();
    if (innerTextRenderer && diff == StyleDifferenceLayout)
        innerTextRenderer->setNeedsLayout(MarkContainingBlockChain);
    if (HTMLElement* placeholder = inputElement().placeholderElement())
        placeholder->setInlineStyleProperty(CSSPropertyTextOverflow, textShouldBeTruncated() ? CSSValueEllipsis : CSSValueClip);
    setHasOverflowClip(false);
}

void RenderTextControlSingleLine::capsLockStateMayHaveChanged()
{
    // Only draw the caps lock indicator if these things are true:
    // 1) The field is a password field
    // 2) The frame is active
    // 3) The element is focused
    // 4) The caps lock is on
    bool shouldDrawCapsLockIndicator =
        inputElement().isPasswordField()
        && frame().selection().isFocusedAndActive()
        && document().focusedElement() == &inputElement()
        && PlatformKeyboardEvent::currentCapsLockState();

    if (shouldDrawCapsLockIndicator != m_shouldDrawCapsLockIndicator) {
        m_shouldDrawCapsLockIndicator = shouldDrawCapsLockIndicator;
        repaint();
    }
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

float RenderTextControlSingleLine::getAvgCharWidth(AtomicString family)
{
#if !PLATFORM(IOS)
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 901 is the avgCharWidth value in the OS/2 table for MS Shell Dlg.
    if (family == "Lucida Grande")
        return scaleEmToUnits(901);
#endif

    return RenderTextControl::getAvgCharWidth(family);
}

LayoutUnit RenderTextControlSingleLine::preferredContentLogicalWidth(float charWidth) const
{
    int factor;
    bool includesDecoration = inputElement().sizeShouldIncludeDecoration(factor);
    if (factor <= 0)
        factor = 20;

    LayoutUnit result = static_cast<LayoutUnit>(ceiledLayoutUnit(charWidth * factor));

    float maxCharWidth = 0.f;

#if !PLATFORM(IOS)
    const AtomicString& family = style().font().firstFamily();
    // Since Lucida Grande is the default font, we want this to match the width
    // of MS Shell Dlg, the default font for textareas in Firefox, Safari Win and
    // IE for some encodings (in IE, the default font is encoding specific).
    // 4027 is the (xMax - xMin) value in the "head" font table for MS Shell Dlg.
    if (family == "Lucida Grande")
        maxCharWidth = scaleEmToUnits(4027);
    else if (hasValidAvgCharWidth(family))
        maxCharWidth = roundf(style().font().primaryFont()->maxCharWidth());
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

PassRef<RenderStyle> RenderTextControlSingleLine::createInnerTextStyle(const RenderStyle* startStyle) const
{
    auto textBlockStyle = RenderStyle::create();
    textBlockStyle.get().inheritFrom(startStyle);
    adjustInnerTextStyle(startStyle, &textBlockStyle.get());

    textBlockStyle.get().setWhiteSpace(PRE);
    textBlockStyle.get().setOverflowWrap(NormalOverflowWrap);
    textBlockStyle.get().setOverflowX(OHIDDEN);
    textBlockStyle.get().setOverflowY(OHIDDEN);
    textBlockStyle.get().setTextOverflow(textShouldBeTruncated() ? TextOverflowEllipsis : TextOverflowClip);

    if (m_desiredInnerTextLogicalHeight >= 0)
        textBlockStyle.get().setLogicalHeight(Length(m_desiredInnerTextLogicalHeight, Fixed));
    // Do not allow line-height to be smaller than our default.
    if (textBlockStyle.get().fontMetrics().lineSpacing() > lineHeight(true, HorizontalLine, PositionOfInteriorLineBoxes))
        textBlockStyle.get().setLineHeight(RenderStyle::initialLineHeight());

    textBlockStyle.get().setDisplay(BLOCK);

    return textBlockStyle;
}

PassRef<RenderStyle> RenderTextControlSingleLine::createInnerBlockStyle(const RenderStyle* startStyle) const
{
    auto innerBlockStyle = RenderStyle::create();
    innerBlockStyle.get().inheritFrom(startStyle);

    innerBlockStyle.get().setFlexGrow(1);
    // min-width: 0; is needed for correct shrinking.
    // FIXME: Remove this line when https://bugs.webkit.org/show_bug.cgi?id=111790 is fixed.
    innerBlockStyle.get().setMinWidth(Length(0, Fixed));
    innerBlockStyle.get().setDisplay(BLOCK);
    innerBlockStyle.get().setDirection(LTR);

    // We don't want the shadow dom to be editable, so we set this block to read-only in case the input itself is editable.
    innerBlockStyle.get().setUserModify(READ_ONLY);

    return innerBlockStyle;
}

bool RenderTextControlSingleLine::textShouldBeTruncated() const
{
    return document().focusedElement() != &inputElement() && style().textOverflow() == TextOverflowEllipsis;
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

void RenderTextControlSingleLine::setScrollLeft(int newLeft)
{
    if (innerTextElement())
        innerTextElement()->setScrollLeft(newLeft);
}

void RenderTextControlSingleLine::setScrollTop(int newTop)
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
    return toHTMLInputElement(RenderTextControl::textFormControlElement());
}

}
