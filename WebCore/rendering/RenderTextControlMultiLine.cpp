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

#include "EventNames.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "HTMLTextAreaElement.h"

namespace WebCore {

using namespace HTMLNames;

RenderTextControlMultiLine::RenderTextControlMultiLine(Node* node)
    : RenderTextControl(node)
{
    ASSERT(node->hasTagName(textareaTag));
}

RenderTextControlMultiLine::~RenderTextControlMultiLine()
{
    if (node())
        static_cast<HTMLTextAreaElement*>(node())->rendererWillBeDestroyed();
}

void RenderTextControlMultiLine::subtreeHasChanged()
{
    RenderTextControl::subtreeHasChanged();
    formControlElement()->setValueMatchesRenderer(false);

    if (!node()->focused())
        return;

    if (Frame* frame = document()->frame())
        frame->textDidChangeInTextArea(static_cast<Element*>(node()));
}

void RenderTextControlMultiLine::layout()
{
    int oldHeight = m_height;
    calcHeight();

    int oldWidth = m_width;
    calcWidth();

    bool relayoutChildren = oldHeight != m_height || oldWidth != m_width;
    RenderObject* innerTextRenderer = innerTextElement()->renderer();

    // Set the text block height
    int desiredHeight = textBlockHeight();
    if (desiredHeight != innerTextRenderer->height())
        relayoutChildren = true;
    innerTextRenderer->style()->setHeight(Length(desiredHeight, Fixed));

    // Set the text block width
    int desiredWidth = textBlockWidth();
    if (desiredWidth != innerTextRenderer->width())
        relayoutChildren = true;
    innerTextRenderer->style()->setWidth(Length(desiredWidth, Fixed));

    RenderBlock::layoutBlock(relayoutChildren);
}

bool RenderTextControlMultiLine::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    if (!RenderTextControl::nodeAtPoint(request, result, x, y, tx, ty, hitTestAction))
        return false;

    if (result.innerNode() == element()) {
        hitInnerTextBlock(result, x, y, tx, ty);
        return true;
    }

    return false;
}

void RenderTextControlMultiLine::forwardEvent(Event* event)
{
    RenderTextControl::forwardEvent(event);
}

int RenderTextControlMultiLine::preferredContentWidth(float charWidth) const
{
    int factor = static_cast<HTMLTextAreaElement*>(node())->cols();
    return static_cast<int>(ceilf(charWidth * factor)) + scrollbarThickness();
}

void RenderTextControlMultiLine::adjustControlHeightBasedOnLineHeight(int lineHeight)
{
    m_height += lineHeight * static_cast<HTMLTextAreaElement*>(node())->rows();
}

int RenderTextControlMultiLine::baselinePosition(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

void RenderTextControlMultiLine::updateFromElement()
{
    createSubtreeIfNeeded(0);
    RenderTextControl::updateFromElement();

    setInnerTextValue(static_cast<HTMLTextAreaElement*>(node())->value());
}

void RenderTextControlMultiLine::cacheSelection(int start, int end)
{
    static_cast<HTMLTextAreaElement*>(node())->cacheSelection(start, end);
}

PassRefPtr<RenderStyle> RenderTextControlMultiLine::createInnerTextStyle(const RenderStyle* startStyle) const
{
    RefPtr<RenderStyle> textBlockStyle = RenderStyle::create();
    textBlockStyle->inheritFrom(startStyle);

    adjustInnerTextStyle(startStyle, textBlockStyle.get());

    // Forward overflow properties.
    textBlockStyle->setOverflowX(startStyle->overflowX() == OVISIBLE ? OAUTO : startStyle->overflowX());
    textBlockStyle->setOverflowY(startStyle->overflowY() == OVISIBLE ? OAUTO : startStyle->overflowY());

    // Set word wrap property based on wrap attribute.
    if (static_cast<HTMLTextAreaElement*>(node())->shouldWrapText()) {
        textBlockStyle->setWhiteSpace(PRE_WRAP);
        textBlockStyle->setWordWrap(BreakWordWrap);
    } else {
        textBlockStyle->setWhiteSpace(PRE);
        textBlockStyle->setWordWrap(NormalWordWrap);
    }

    textBlockStyle->setDisplay(BLOCK);

    // We're adding three extra pixels of padding to line textareas up with text fields.
    textBlockStyle->setPaddingLeft(Length(3, Fixed));
    textBlockStyle->setPaddingRight(Length(3, Fixed));

    return textBlockStyle.release();
}

}
