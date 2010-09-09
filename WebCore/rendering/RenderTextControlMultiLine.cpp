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
#include "RenderTextControlMultiLine.h"

#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HitTestResult.h"

namespace WebCore {

RenderTextControlMultiLine::RenderTextControlMultiLine(Node* node, bool placeholderVisible)
    : RenderTextControl(node, placeholderVisible)
{
}

RenderTextControlMultiLine::~RenderTextControlMultiLine()
{
    if (node())
        static_cast<HTMLTextAreaElement*>(node())->rendererWillBeDestroyed();
}

void RenderTextControlMultiLine::subtreeHasChanged()
{
    RenderTextControl::subtreeHasChanged();
    HTMLTextAreaElement* textArea = static_cast<HTMLTextAreaElement*>(node());
    textArea->setFormControlValueMatchesRenderer(false);
    textArea->setNeedsValidityCheck();

    if (!node()->focused())
        return;

    node()->dispatchEvent(Event::create(eventNames().inputEvent, true, false));

    if (Frame* frame = this->frame())
        frame->editor()->textDidChangeInTextArea(textArea);
}

bool RenderTextControlMultiLine::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    if (!RenderTextControl::nodeAtPoint(request, result, x, y, tx, ty, hitTestAction))
        return false;

    bool resultIsTextValueOrPlaceholder
        = (!m_placeholderVisible && result.innerNode() == innerTextElement())
        || (m_placeholderVisible && result.innerNode()->isDescendantOf(innerTextElement()));
    if (result.innerNode() == node() || resultIsTextValueOrPlaceholder)
        hitInnerTextElement(result, x, y, tx, ty);

    return true;
}

void RenderTextControlMultiLine::forwardEvent(Event* event)
{
    RenderTextControl::forwardEvent(event);
}

float RenderTextControlMultiLine::getAvgCharWidth(AtomicString family)
{
    // Since Lucida Grande is the default font, we want this to match the width
    // of Courier New, the default font for textareas in IE, Firefox and Safari Win.
    // 1229 is the avgCharWidth value in the OS/2 table for Courier New.
    if (family == AtomicString("Lucida Grande"))
        return scaleEmToUnits(1229);

    return RenderTextControl::getAvgCharWidth(family);
}

int RenderTextControlMultiLine::preferredContentWidth(float charWidth) const
{
    int factor = static_cast<HTMLTextAreaElement*>(node())->cols();
    return static_cast<int>(ceilf(charWidth * factor)) + scrollbarThickness();
}

void RenderTextControlMultiLine::adjustControlHeightBasedOnLineHeight(int lineHeight)
{
    setHeight(height() + lineHeight * static_cast<HTMLTextAreaElement*>(node())->rows());
}

int RenderTextControlMultiLine::baselinePosition(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

void RenderTextControlMultiLine::updateFromElement()
{
    createSubtreeIfNeeded(0);
    RenderTextControl::updateFromElement();

    HTMLTextAreaElement* textArea = static_cast<HTMLTextAreaElement*>(node());
    if (m_placeholderVisible)
        setInnerTextValue(textArea->strippedPlaceholder());
    else
        setInnerTextValue(textArea->value());
}

void RenderTextControlMultiLine::cacheSelection(int start, int end)
{
    static_cast<HTMLTextAreaElement*>(node())->cacheSelection(start, end);
}

PassRefPtr<RenderStyle> RenderTextControlMultiLine::createInnerTextStyle(const RenderStyle* startStyle) const
{
    RefPtr<RenderStyle> textBlockStyle;
    if (m_placeholderVisible) {
        if (RenderStyle* pseudoStyle = getCachedPseudoStyle(INPUT_PLACEHOLDER))
            textBlockStyle = RenderStyle::clone(pseudoStyle);
    }
    if (!textBlockStyle) {
        textBlockStyle = RenderStyle::create();
        textBlockStyle->inheritFrom(startStyle);
    }

    adjustInnerTextStyle(startStyle, textBlockStyle.get());
    textBlockStyle->setDisplay(BLOCK);

    return textBlockStyle.release();
}

RenderStyle* RenderTextControlMultiLine::textBaseStyle() const
{
    return style();
}

}
