/**
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
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

#include "ContainerNodeInlines.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestResult.h"
#include "NodeInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderElementInlines.h"
#include "RenderText.h"
#include "RenderTextControlSingleLine.h"
#include "RenderTheme.h"
#include "ScrollbarTheme.h"
#include "StyleInheritedData.h"
#include "StyleProperties.h"
#include "TextControlInnerElements.h"
#include "VisiblePosition.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderTextControl);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderTextControlInnerContainer);

RenderTextControl::RenderTextControl(Type type, HTMLTextFormControlElement& element, RenderStyle&& style)
    : RenderBlockFlow(type, element, WTFMove(style), BlockFlowFlag::IsTextControl)
{
    ASSERT(isRenderTextControl());
}

RenderTextControl::~RenderTextControl() = default;

HTMLTextFormControlElement& RenderTextControl::textFormControlElement() const
{
    return downcast<HTMLTextFormControlElement>(nodeForNonAnonymous());
}

Ref<HTMLTextFormControlElement> RenderTextControl::protectedTextFormControlElement() const
{
    return textFormControlElement();
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
    if (innerTextRenderer && oldStyle) {
        // FIXME: The height property of the inner text block style may be mutated by RenderTextControlSingleLine::layout.
        // See if the original has changed before setting it and triggering a layout.
        auto newInnerTextStyle = textFormControlElement().createInnerTextStyle(style());
        auto oldInnerTextStyle = textFormControlElement().createInnerTextStyle(*oldStyle);
        if (newInnerTextStyle != oldInnerTextStyle)
            innerTextRenderer->setStyle(WTFMove(newInnerTextStyle));
        else if (diff == StyleDifference::RepaintIfText || diff == StyleDifference::Repaint) {
            // Repaint is expected to be propagated down to the shadow tree when non-inherited style property changes
            // (e.g. text-decoration-color) since that's where the value actually takes effect.
            innerTextRenderer->repaint();
        }
    }
    textFormControlElement().updatePlaceholderVisibility();
}

int RenderTextControl::scrollbarThickness() const
{
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
    return ScrollbarTheme::theme().scrollbarThickness(Style::toPlatform(this->style().scrollbarWidth()), OverlayScrollbarSizeRelevancy::IgnoreOverlayScrollbarSize);
}

RenderBox::LogicalExtentComputedValues RenderTextControl::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    auto innerText = innerTextElement();
    if (!innerText)
        return RenderBox::computeLogicalHeight(LayoutUnit(), LayoutUnit());

    if (style().fieldSizing() == FieldSizing::Content) {
        auto logicalHeightExtent = RenderBox::computeLogicalHeight(logicalHeight, logicalTop);
        if (style().logicalHeight().isSpecified())
            return logicalHeightExtent;

        auto placeholderLogicalHeight = [&] -> LayoutUnit {
            CheckedPtr placeholder = textFormControlElement().placeholderElement();
            if (!placeholder)
                return { };
            CheckedPtr placeholderBox = placeholder->renderBox();
            if (!placeholderBox)
                return { };
            return placeholderBox->computeLogicalHeight(placeholderBox->logicalHeight(), placeholderBox->logicalTop()).m_extent;
        };
        logicalHeightExtent.m_extent = std::max(logicalHeightExtent.m_extent, placeholderLogicalHeight());
        return logicalHeightExtent;
    }

    if (RenderBox* innerTextBox = innerText->renderBox()) {
        LayoutUnit nonContentHeight = innerTextBox->borderAndPaddingLogicalHeight() + innerTextBox->marginLogicalHeight();
        logicalHeight = computeControlLogicalHeight(innerTextBox->lineHeight(), nonContentHeight);

        // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
        auto shouldIncludeScrollbarHeight = [&] {
            auto& style = this->style();
            auto isHorizontalWritingMode = this->isHorizontalWritingMode();
            return (isHorizontalWritingMode && style.overflowX() == Overflow::Scroll) || (!isHorizontalWritingMode && style.overflowY() == Overflow::Scroll);
        };
        if (shouldIncludeScrollbarHeight())
            logicalHeight += scrollbarThickness();
        
        // FIXME: The logical height of the inner text box should have been added
        // before calling computeLogicalHeight to avoid this hack.
        cacheIntrinsicContentLogicalHeightForFlexItem(logicalHeight);
        
        logicalHeight += borderAndPaddingLogicalHeight();
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

    const char16_t ch = '0';
    const String str = span(ch);
    const FontCascade& font = style().fontCascade();
    TextRun textRun = constructTextRun(str, style(), ExpansionBehavior::allowRightOnly());
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
    if (style().fieldSizing() == FieldSizing::Content)
        return RenderBlockFlow::computeIntrinsicLogicalWidths(minLogicalWidth, maxLogicalWidth);

    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = width.value();
            maxLogicalWidth = width.value();
        }
        return;
    }
    // Use average character width. Matches IE.
    maxLogicalWidth = preferredContentLogicalWidth(const_cast<RenderTextControl*>(this)->getAverageCharWidth());
    maxLogicalWidth = RenderTheme::singleton().adjustedMaximumLogicalWidthForControl(style(), textFormControlElement(), maxLogicalWidth);

    auto& logicalWidth = style().logicalWidth();
    if (logicalWidth.isCalculated())
        minLogicalWidth = std::max(0_lu, Style::evaluate<LayoutUnit>(logicalWidth, 0_lu, style().usedZoomForLength()));
    else if (!logicalWidth.isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderTextControl::computePreferredLogicalWidths()
{
    ASSERT(needsPreferredLogicalWidthsUpdate());
    if (style().fieldSizing() == FieldSizing::Content) {
        RenderBlockFlow::computePreferredLogicalWidths();
        return;
    }

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (auto fixedLogicalWidth = style().logicalWidth().tryFixed(); fixedLogicalWidth && fixedLogicalWidth->isPositiveOrZero())
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalWidth);
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), borderAndPaddingLogicalWidth());

    clearNeedsPreferredWidthsUpdate();
}

void RenderTextControl::layoutExcludedChildren(RelayoutChildren relayoutChildren)
{
    RenderBlockFlow::layoutExcludedChildren(relayoutChildren);

    auto* placeholder = textFormControlElement().placeholderElement();
    if (!placeholder)
        return;

    CheckedPtr placeholderRenderer = placeholder->renderer();
    if (!placeholderRenderer)
        return;

    placeholderRenderer->setIsExcludedFromNormalLayout(true);

    if (style().fieldSizing() == FieldSizing::Content) {
        // In order to take placeholder height into account while computing the size of the input box, we need to
        // layout the placeholder too (which is how excluded content normally works).
        placeholderRenderer->setChildNeedsLayout(MarkOnlyThis);
        placeholderRenderer->layoutIfNeeded();
    }

    if (relayoutChildren == RelayoutChildren::Yes) {
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
    return innerText && innerText->renderer() && innerText->renderer()->hasNonVisibleOverflow();
}

int RenderTextControl::innerLineHeight() const
{
    if (auto innerTextElement = this->innerTextElement(); innerTextElement && innerTextElement->renderer())
        return innerTextElement->renderer()->style().computedLineHeight();
    return style().computedLineHeight();
}
#endif

RenderTextControlInnerContainer::RenderTextControlInnerContainer(Element& element, RenderStyle&& style)
    : RenderFlexibleBox(Type::TextControlInnerContainer, element, WTFMove(style))
{

}

RenderTextControlInnerContainer::~RenderTextControlInnerContainer() = default;

} // namespace WebCore
