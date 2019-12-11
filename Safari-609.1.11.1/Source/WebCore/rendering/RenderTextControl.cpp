/**
 * Copyright (C) 2006, 2007, 2014 Apple Inc. All rights reserved.
 *           (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)  
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
#include "RenderTextControl.h"

#include "HTMLTextFormControlElement.h"
#include "HitTestResult.h"
#include "RenderText.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScrollbarTheme.h"
#include "StyleInheritedData.h"
#include "StyleProperties.h"
#include "TextControlInnerElements.h"
#include "VisiblePosition.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTextControl);
WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTextControlInnerContainer);

RenderTextControl::RenderTextControl(HTMLTextFormControlElement& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
}

RenderTextControl::~RenderTextControl() = default;

HTMLTextFormControlElement& RenderTextControl::textFormControlElement() const
{
    return downcast<HTMLTextFormControlElement>(nodeForNonAnonymous());
}

RefPtr<TextControlInnerTextElement> RenderTextControl::innerTextElement() const
{
    return textFormControlElement().innerTextElement();
}

void RenderTextControl::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    auto innerText = innerTextElement();
    if (!innerText)
        return;
    RenderTextControlInnerBlock* innerTextRenderer = innerText->renderer();
    if (innerTextRenderer) {
        // We may have set the width and the height in the old style in layout().
        // Reset them now to avoid getting a spurious layout hint.
        innerTextRenderer->mutableStyle().setHeight(Length());
        innerTextRenderer->mutableStyle().setWidth(Length());
        innerTextRenderer->setStyle(textFormControlElement().createInnerTextStyle(style()));
    }
    textFormControlElement().updatePlaceholderVisibility();
}

int RenderTextControl::textBlockLogicalHeight() const
{
    return logicalHeight() - borderAndPaddingLogicalHeight();
}

int RenderTextControl::textBlockLogicalWidth() const
{
    auto innerText = innerTextElement();
    ASSERT(innerText);

    LayoutUnit unitWidth = logicalWidth() - borderAndPaddingLogicalWidth();
    if (innerText->renderer())
        unitWidth -= innerText->renderBox()->paddingStart() + innerText->renderBox()->paddingEnd();

    return unitWidth;
}

int RenderTextControl::scrollbarThickness() const
{
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
    return ScrollbarTheme::theme().scrollbarThickness();
}

RenderBox::LogicalExtentComputedValues RenderTextControl::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    auto innerText = innerTextElement();
    ASSERT(innerText);
    if (RenderBox* innerTextBox = innerText->renderBox()) {
        LayoutUnit nonContentHeight = innerTextBox->verticalBorderAndPaddingExtent() + innerTextBox->verticalMarginExtent();
        logicalHeight = computeControlLogicalHeight(innerTextBox->lineHeight(true, HorizontalLine, PositionOfInteriorLineBoxes), nonContentHeight);

        // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
        if ((isHorizontalWritingMode() && (style().overflowX() == Overflow::Scroll ||  (style().overflowX() == Overflow::Auto && innerText->renderer()->style().overflowWrap() == OverflowWrap::Normal)))
            || (!isHorizontalWritingMode() && (style().overflowY() == Overflow::Scroll ||  (style().overflowY() == Overflow::Auto && innerText->renderer()->style().overflowWrap() == OverflowWrap::Normal))))
            logicalHeight += scrollbarThickness();
        
        // FIXME: The logical height of the inner text box should have been added
        // before calling computeLogicalHeight to avoid this hack.
        cacheIntrinsicContentLogicalHeightForFlexItem(logicalHeight);
        
        logicalHeight += verticalBorderAndPaddingExtent();
    }

    return RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
}

void RenderTextControl::hitInnerTextElement(HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset)
{
    auto innerText = innerTextElement();
    if (!innerText->renderer())
        return;

    LayoutPoint adjustedLocation = accumulatedOffset + location();
    LayoutPoint localPoint = pointInContainer - toLayoutSize(adjustedLocation + innerText->renderBox()->location()) + toLayoutSize(scrollPosition());
    result.setInnerNode(innerText.get());
    result.setInnerNonSharedNode(innerText.get());
    result.setLocalPoint(localPoint);
}

float RenderTextControl::getAverageCharWidth()
{
    float width;
    if (style().fontCascade().fastAverageCharWidthIfAvailable(width))
        return width;

    const UChar ch = '0';
    const String str = String(&ch, 1);
    const FontCascade& font = style().fontCascade();
    TextRun textRun = constructTextRun(str, style(), AllowTrailingExpansion);
    return font.width(textRun);
}

float RenderTextControl::scaleEmToUnits(int x) const
{
    // This matches the unitsPerEm value for MS Shell Dlg and Courier New from the "head" font table.
    float unitsPerEm = 2048.0f;
    return roundf(style().fontCascade().size() * x / unitsPerEm);
}

void RenderTextControl::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // Use average character width. Matches IE.
    maxLogicalWidth = preferredContentLogicalWidth(const_cast<RenderTextControl*>(this)->getAverageCharWidth());
    if (RenderBox* innerTextRenderBox = innerTextElement()->renderBox())
        maxLogicalWidth += innerTextRenderBox->paddingStart() + innerTextRenderBox->paddingEnd();
    if (!style().logicalWidth().isPercentOrCalculated())
        minLogicalWidth = maxLogicalWidth;
}

void RenderTextControl::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style().logicalWidth().isFixed() && style().logicalWidth().value() >= 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().logicalWidth().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (style().logicalMinWidth().isFixed() && style().logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().logicalMinWidth().value()));
    }

    if (style().logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().logicalMaxWidth().value()));
    }

    LayoutUnit toAdd = borderAndPaddingLogicalWidth();

    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false);
}

void RenderTextControl::addFocusRingRects(Vector<LayoutRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*)
{
    if (!size().isEmpty())
        rects.append(LayoutRect(additionalOffset, size()));
}

void RenderTextControl::layoutExcludedChildren(bool relayoutChildren)
{
    RenderBlockFlow::layoutExcludedChildren(relayoutChildren);

    HTMLElement* placeholder = textFormControlElement().placeholderElement();
    RenderElement* placeholderRenderer = placeholder ? placeholder->renderer() : 0;
    if (!placeholderRenderer)
        return;
    placeholderRenderer->setIsExcludedFromNormalLayout(true);

    if (relayoutChildren) {
        // The markParents arguments should be false because this function is
        // called from layout() of the parent and the placeholder layout doesn't
        // affect the parent layout.
        placeholderRenderer->setChildNeedsLayout(MarkOnlyThis);
    }
}

#if PLATFORM(IOS_FAMILY)
bool RenderTextControl::canScroll() const
{
    auto innerText = innerTextElement();
    return innerText && innerText->renderer() && innerText->renderer()->hasOverflowClip();
}

int RenderTextControl::innerLineHeight() const
{
    auto innerText = innerTextElement();
    if (innerText && innerText->renderer())
        return innerText->renderer()->style().computedLineHeight();
    return style().computedLineHeight();
}
#endif

} // namespace WebCore
