/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "CSSPrimitiveValueMappings.h"
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
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

RenderTextControl::RenderTextControl(HTMLTextFormControlElement& element, PassRef<RenderStyle> style)
    : RenderBlockFlow(element, WTF::move(style))
{
}

RenderTextControl::~RenderTextControl()
{
}

HTMLTextFormControlElement& RenderTextControl::textFormControlElement() const
{
    return toHTMLTextFormControlElement(nodeForNonAnonymous());
}

TextControlInnerTextElement* RenderTextControl::innerTextElement() const
{
    return textFormControlElement().innerTextElement();
}

void RenderTextControl::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);
    TextControlInnerTextElement* innerText = innerTextElement();
    if (!innerText)
        return;
    RenderTextControlInnerBlock* innerTextRenderer = innerText->renderer();
    if (innerTextRenderer) {
        // We may have set the width and the height in the old style in layout().
        // Reset them now to avoid getting a spurious layout hint.
        innerTextRenderer->style().setHeight(Length());
        innerTextRenderer->style().setWidth(Length());
        innerTextRenderer->setStyle(createInnerTextStyle(&style()));
    }
    textFormControlElement().updatePlaceholderVisibility(false);
}

void RenderTextControl::adjustInnerTextStyle(const RenderStyle* startStyle, RenderStyle* textBlockStyle) const
{
    // The inner block, if present, always has its direction set to LTR,
    // so we need to inherit the direction and unicode-bidi style from the element.
    textBlockStyle->setDirection(style().direction());
    textBlockStyle->setUnicodeBidi(style().unicodeBidi());

    HTMLTextFormControlElement& control = textFormControlElement();
    if (HTMLElement* innerText = control.innerTextElement()) {
        if (const StyleProperties* properties = innerText->presentationAttributeStyle()) {
            RefPtr<CSSValue> value = properties->getPropertyCSSValue(CSSPropertyWebkitUserModify);
            if (value && value->isPrimitiveValue())
                textBlockStyle->setUserModify(toCSSPrimitiveValue(*value));
        }
    }

    if (control.isDisabledFormControl())
        textBlockStyle->setColor(theme().disabledTextColor(textBlockStyle->visitedDependentColor(CSSPropertyColor), startStyle->visitedDependentColor(CSSPropertyBackgroundColor)));
#if PLATFORM(IOS)
    if (textBlockStyle->textSecurity() != TSNONE && !textBlockStyle->isLeftToRightDirection()) {
        // Preserve the alignment but force the direction to LTR so that the last-typed, unmasked character
        // (which cannot have RTL directionality) will appear to the right of the masked characters. See <rdar://problem/7024375>.
        
        switch (textBlockStyle->textAlign()) {
        case TASTART:
        case JUSTIFY:
            textBlockStyle->setTextAlign(RIGHT);
            break;
        case TAEND:
            textBlockStyle->setTextAlign(LEFT);
            break;
        case LEFT:
        case RIGHT:
        case CENTER:
        case WEBKIT_LEFT:
        case WEBKIT_RIGHT:
        case WEBKIT_CENTER:
            break;
        }

        textBlockStyle->setDirection(LTR);
    }
#endif
}

int RenderTextControl::textBlockLogicalHeight() const
{
    return logicalHeight() - borderAndPaddingLogicalHeight();
}

int RenderTextControl::textBlockLogicalWidth() const
{
    TextControlInnerTextElement* innerText = innerTextElement();
    ASSERT(innerText);

    LayoutUnit unitWidth = logicalWidth() - borderAndPaddingLogicalWidth();
    if (innerText->renderer())
        unitWidth -= innerText->renderBox()->paddingStart() + innerText->renderBox()->paddingEnd();

    return unitWidth;
}

int RenderTextControl::scrollbarThickness() const
{
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
    return ScrollbarTheme::theme()->scrollbarThickness();
}

void RenderTextControl::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues& computedValues) const
{
    TextControlInnerTextElement* innerText = innerTextElement();
    ASSERT(innerText);
    if (RenderBox* innerTextBox = innerText->renderBox()) {
        LayoutUnit nonContentHeight = innerTextBox->verticalBorderAndPaddingExtent() + innerTextBox->verticalMarginExtent();
        logicalHeight = computeControlLogicalHeight(innerTextBox->lineHeight(true, HorizontalLine, PositionOfInteriorLineBoxes), nonContentHeight) + verticalBorderAndPaddingExtent();

        // We are able to have a horizontal scrollbar if the overflow style is scroll, or if its auto and there's no word wrap.
        if ((isHorizontalWritingMode() && (style().overflowX() == OSCROLL ||  (style().overflowX() == OAUTO && innerText->renderer()->style().overflowWrap() == NormalOverflowWrap)))
            || (!isHorizontalWritingMode() && (style().overflowY() == OSCROLL ||  (style().overflowY() == OAUTO && innerText->renderer()->style().overflowWrap() == NormalOverflowWrap))))
            logicalHeight += scrollbarThickness();
    }

    RenderBox::computeLogicalHeight(logicalHeight, logicalTop, computedValues);
}

void RenderTextControl::hitInnerTextElement(HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset)
{
    TextControlInnerTextElement* innerText = innerTextElement();
    if (!innerText->renderer())
        return;

    LayoutPoint adjustedLocation = accumulatedOffset + location();
    LayoutPoint localPoint = pointInContainer - toLayoutSize(adjustedLocation + innerText->renderBox()->location()) + scrolledContentOffset();
    result.setInnerNode(innerText);
    result.setInnerNonSharedNode(innerText);
    result.setLocalPoint(localPoint);
}

float RenderTextControl::getAverageCharWidth()
{
    float width;
    if (style().font().fastAverageCharWidthIfAvailable(width))
        return width;

    const UChar ch = '0';
    const String str = String(&ch, 1);
    const Font& font = style().font();
    TextRun textRun = constructTextRun(this, font, str, style(), TextRun::AllowTrailingExpansion);
    textRun.disableRoundingHacks();
    return font.width(textRun);
}

float RenderTextControl::scaleEmToUnits(int x) const
{
    // This matches the unitsPerEm value for MS Shell Dlg and Courier New from the "head" font table.
    float unitsPerEm = 2048.0f;
    return roundf(style().font().size() * x / unitsPerEm);
}

void RenderTextControl::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // Use average character width. Matches IE.
    maxLogicalWidth = preferredContentLogicalWidth(const_cast<RenderTextControl*>(this)->getAverageCharWidth());
    if (RenderBox* innerTextRenderBox = innerTextElement()->renderBox())
        maxLogicalWidth += innerTextRenderBox->paddingStart() + innerTextRenderBox->paddingEnd();
    if (!style().logicalWidth().isPercent())
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

void RenderTextControl::addFocusRingRects(Vector<IntRect>& rects, const LayoutPoint& additionalOffset, const RenderLayerModelObject*)
{
    if (!size().isEmpty())
        rects.append(pixelSnappedIntRect(additionalOffset, size()));
}

RenderObject* RenderTextControl::layoutSpecialExcludedChild(bool relayoutChildren)
{
    HTMLElement* placeholder = textFormControlElement().placeholderElement();
    RenderElement* placeholderRenderer = placeholder ? placeholder->renderer() : 0;
    if (!placeholderRenderer)
        return 0;
    if (relayoutChildren) {
        // The markParents arguments should be false because this function is
        // called from layout() of the parent and the placeholder layout doesn't
        // affect the parent layout.
        placeholderRenderer->setChildNeedsLayout(MarkOnlyThis);
    }
    return placeholderRenderer;
}

#if PLATFORM(IOS)
bool RenderTextControl::canScroll() const
{
    Element* innerText = innerTextElement();
    return innerText && innerText->renderer() && innerText->renderer()->hasOverflowClip();
}

int RenderTextControl::innerLineHeight() const
{
    Element* innerText = innerTextElement();
    if (innerText && innerText->renderer())
        return innerText->renderer()->style().computedLineHeight();
    return style().computedLineHeight();
}
#endif

} // namespace WebCore
