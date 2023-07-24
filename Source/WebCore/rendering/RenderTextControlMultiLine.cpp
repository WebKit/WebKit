/**
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/) 
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
 */

#include "config.h"
#include "RenderTextControlMultiLine.h"

#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"
#include "LocalFrame.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderStyleSetters.h"
#include "ShadowRoot.h"
#include "StyleInheritedData.h"
#include "TextControlInnerElements.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTextControlMultiLine);

RenderTextControlMultiLine::RenderTextControlMultiLine(HTMLTextAreaElement& element, RenderStyle&& style)
    : RenderTextControl(element, WTFMove(style))
{
}

RenderTextControlMultiLine::~RenderTextControlMultiLine()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

HTMLTextAreaElement& RenderTextControlMultiLine::textAreaElement() const
{
    return downcast<HTMLTextAreaElement>(RenderTextControl::textFormControlElement());
}

bool RenderTextControlMultiLine::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction hitTestAction)
{
    if (!RenderTextControl::nodeAtPoint(request, result, locationInContainer, accumulatedOffset, hitTestAction))
        return false;

    if (result.innerNode() == &textAreaElement() || result.innerNode() == innerTextElement())
        hitInnerTextElement(result, locationInContainer.point(), accumulatedOffset);

    return true;
}

float RenderTextControlMultiLine::getAverageCharWidth()
{
#if !PLATFORM(IOS_FAMILY)
    // Since Lucida Grande is the default font, we want this to match the width
    // of Courier New, the default font for textareas in IE, Firefox and Safari Win.
    // 1229 is the avgCharWidth value in the OS/2 table for Courier New.
    if (style().fontCascade().firstFamily() == "Lucida Grande"_s)
        return scaleEmToUnits(1229);
#endif

    return RenderTextControl::getAverageCharWidth();
}

LayoutUnit RenderTextControlMultiLine::preferredContentLogicalWidth(float charWidth) const
{
    float width = ceilf(charWidth * textAreaElement().cols());

    // We are able to have a vertical scrollbar if the overflow style is scroll or auto
    if ((style().overflowY() == Overflow::Scroll) || (style().overflowY() == Overflow::Auto))
        width += scrollbarThickness();

    return LayoutUnit(width);
}

LayoutUnit RenderTextControlMultiLine::computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const
{
    return lineHeight * textAreaElement().rows() + nonContentHeight;
}

LayoutUnit RenderTextControlMultiLine::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
}

void RenderTextControlMultiLine::layoutExcludedChildren(bool relayoutChildren)
{
    RenderTextControl::layoutExcludedChildren(relayoutChildren);
    HTMLElement* placeholder = textFormControlElement().placeholderElement();
    RenderElement* placeholderRenderer = placeholder ? placeholder->renderer() : 0;
    if (!placeholderRenderer)
        return;
    if (is<RenderBox>(placeholderRenderer)) {
        auto& placeholderBox = downcast<RenderBox>(*placeholderRenderer);
        placeholderBox.mutableStyle().setLogicalWidth(Length(contentLogicalWidth() - placeholderBox.borderAndPaddingLogicalWidth(), LengthType::Fixed));
        placeholderBox.layoutIfNeeded();
        placeholderBox.setX(borderLeft() + paddingLeft());
        placeholderBox.setY(borderTop() + paddingTop());
    }
}
    
}
