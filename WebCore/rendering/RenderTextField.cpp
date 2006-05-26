/**
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderTextField.h"

#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFieldInnerElement.h"
#include "SelectionController.h"
#include "TextIterator.h"
#include "dom2_eventsimpl.h"
#include <math.h>
#include "RenderTheme.h"
#include "visible_units.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;
using namespace std;

RenderTextField::RenderTextField(Node* node, bool multiLine)
    : RenderFlexibleBox(node), m_dirty(false), m_multiLine(multiLine)
{
}

RenderTextField::~RenderTextField()
{
    if (m_multiLine && node())
        static_cast<HTMLTextAreaElement*>(node())->rendererWillBeDestroyed();
    // The renderer for the div has already been destroyed by destroyLeftoverChildren
    if (m_div)
        m_div->detach();
}

void RenderTextField::setStyle(RenderStyle* style)
{
    RenderFlexibleBox::setStyle(style);
    if (m_div) {
        RenderBlock* divRenderer = static_cast<RenderBlock*>(m_div->renderer());
        RenderStyle* divStyle = createDivStyle(style);
        divRenderer->setStyle(divStyle);
        for (Node *n = m_div->firstChild(); n; n = n->traverseNextNode(m_div.get()))
            if (n->renderer())
                n->renderer()->setStyle(divStyle);
    }
    setHasOverflowClip(false);
    setReplaced(isInline());
}

RenderStyle* RenderTextField::createDivStyle(RenderStyle* startStyle)
{
    RenderStyle* divStyle = new (renderArena()) RenderStyle();
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    
    divStyle->inheritFrom(startStyle);
    divStyle->setDisplay(BLOCK);
    divStyle->setBoxFlex(1.0f);
    divStyle->setUserModify(element->isReadOnlyControl() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);

    if (m_multiLine) {
        // Forward overflow property
        if (startStyle->overflow() == OHIDDEN || startStyle->overflow() == OSCROLL) 
            divStyle->setOverflow(startStyle->overflow());
        else
            divStyle->setOverflow(OAUTO);
        // Set word wrap property based on wrap attribute
        if (static_cast<HTMLTextAreaElement*>(element)->wrap() == HTMLTextAreaElement::ta_NoWrap) {
            divStyle->setWhiteSpace(PRE);
            divStyle->setWordWrap(WBNORMAL);
        } else {
            divStyle->setWhiteSpace(PRE_WRAP);
            divStyle->setWordWrap(BREAK_WORD);
        }

    } else {
        divStyle->setWhiteSpace(PRE);
        divStyle->setOverflow(OHIDDEN);
    }

    // We're adding this extra pixel of padding to match WinIE.
    divStyle->setPaddingLeft(Length(1, Fixed));
    divStyle->setPaddingRight(Length(1, Fixed));

    if (!element->isEnabled()) {
        Color textColor = startStyle->color();
        Color disabledTextColor;
        if (differenceSquared(textColor, Color::white) > differenceSquared(startStyle->backgroundColor(), Color::white))
            disabledTextColor = textColor.light();
        else
            disabledTextColor = textColor.dark();
        divStyle->setColor(disabledTextColor);
    }

    return divStyle;
}

void RenderTextField::updateFromElement()
{
    if (!m_div) {
        // Create the div and give it a parent, renderer, and style
        m_div = new HTMLTextFieldInnerElement(document(), node());
        RenderBlock* divRenderer = new (renderArena()) RenderBlock(m_div.get());
        m_div->setRenderer(divRenderer);
        m_div->setAttached();
        m_div->setInDocument(true);
        
        RenderStyle* divStyle = createDivStyle(style());
        divRenderer->setStyle(divStyle); 
        
        // Add div to Render tree
        RenderFlexibleBox::addChild(divRenderer);
    }

        HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
        m_div->renderer()->style()->setUserModify(element->isReadOnlyControl() ? READ_ONLY : READ_WRITE_PLAINTEXT_ONLY);
        String value;
        if (m_multiLine)
            value = static_cast<HTMLTextAreaElement*>(element)->value().copy();    
        else
            value = static_cast<HTMLInputElement*>(element)->value().copy();        
        if (!element->valueMatchesRenderer() || m_multiLine) {
            String oldText = text();
            if (value.isNull())
                value = "";
            value.replace('\\', backslashAsCurrencySymbol());
            if (value != oldText || !m_div->hasChildNodes()) {
                ExceptionCode ec = 0;
                m_div->setInnerText(value, ec);
                setEdited(false);
            }
          element->setValueMatchesRenderer();
    }
}

int RenderTextField::selectionStart()
{
    return indexForVisiblePosition(document()->frame()->selection().start());        
}

int RenderTextField::selectionEnd()
{
    return indexForVisiblePosition(document()->frame()->selection().end());        
}

void RenderTextField::setSelectionStart(int start)
{
    setSelectionRange(start, max(start, selectionEnd()));
}
 
void RenderTextField::setSelectionEnd(int end)
{
    setSelectionRange(min(end, selectionStart()), end);
}
    
void RenderTextField::select()
{
    setSelectionRange(0, text().length());
}

void RenderTextField::setSelectionRange(int start, int end)
{
    end = max(end, 0);
    start = min(max(start, 0), end);
    
    document()->updateLayout();
    
    VisiblePosition startPosition = visiblePositionForIndex(start);
    VisiblePosition endPosition;
    if (start == end)
        endPosition = startPosition;
    else
        endPosition = visiblePositionForIndex(end);
    
    ASSERT(startPosition.isNotNull() && endPosition.isNotNull());
    ASSERT(startPosition.deepEquivalent().node()->shadowAncestorNode() == node() && endPosition.deepEquivalent().node()->shadowAncestorNode() == node());

    SelectionController sel = SelectionController(startPosition, endPosition);
    document()->frame()->setSelection(sel);
}

VisiblePosition RenderTextField::visiblePositionForIndex(int index)
{    
    if (index <= 0)
        return VisiblePosition(m_div.get(), 0, DOWNSTREAM);
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->selectNodeContents(m_div.get(), ec);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    return VisiblePosition(it.range()->endContainer(ec), it.range()->endOffset(ec), UPSTREAM);
}

int RenderTextField::indexForVisiblePosition(const VisiblePosition& pos)
{
    Position indexPosition = pos.deepEquivalent();
    if (!indexPosition.node() || indexPosition.node()->rootEditableElement() != m_div)
        return 0;
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->setStart(m_div.get(), 0, ec);
    range->setEnd(indexPosition.node(), indexPosition.offset(), ec);
    return TextIterator::rangeLength(range.get());
}

void RenderTextField::subtreeHasChanged()
{
    setEdited(true);
    HTMLGenericFormElement* element = static_cast<HTMLGenericFormElement*>(node());
    if (m_multiLine) {
        element->setValueMatchesRenderer(false);
        document()->frame()->textDidChangeInTextArea(element);
    } else {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element);
        if (input) {
            input->setValueFromRenderer(text());
            if (!isEdited())
                document()->frame()->textFieldDidBeginEditing(input);
            document()->frame()->textDidChangeInTextField(input);
        }
    }
}

String RenderTextField::text()
{
    if (m_div)
        return m_div->textContent(true).replace('\\', backslashAsCurrencySymbol());
    return String();
}

String RenderTextField::textWithHardLineBreaks()
{
    String s("");
    
    if (!m_div)
        return s;

    document()->updateLayout();

    ASSERT(m_div->firstChild());
    InlineBox* box = m_div->firstChild()->renderer()->inlineBox(0, DOWNSTREAM);
    if (!box)
        return s;
    
    ExceptionCode ec = 0;
    RefPtr<Range> range = new Range(document());
    range->selectNodeContents(m_div.get(), ec);
    for (RootInlineBox* line = box->root(); line; line = line->nextRootBox()) {
        // If we're at a soft wrap, then insert the hard line break here
        if (!line->endsWithBreak() && line->nextRootBox()) {
            // Update range so it ends before this wrap
            ASSERT(line->lineBreakObj());
            range->setEnd(line->lineBreakObj()->node(), line->lineBreakPos(), ec);

            s.append(range->toString(true, ec));
            s.append("\n");

            // Update range so it starts after this wrap
            range->setEnd(m_div.get(), 1, ec);
            range->setStart(line->lineBreakObj()->node(), line->lineBreakPos(), ec);
        }
    }
    s.append(range->toString(true, ec));
    ASSERT(ec == 0);

    return s.replace('\\', backslashAsCurrencySymbol());
}

void RenderTextField::calcHeight()
{
    
    int rows = 1;
    if (m_multiLine)
        rows = static_cast<HTMLTextAreaElement*>(node())->rows();
    
    int line = m_div->renderer()->lineHeight(true);
    int toAdd = paddingTop() + paddingBottom() + borderTop() + borderBottom() +
                m_div->renderer()->paddingTop() + m_div->renderer()->paddingBottom();
    
    int scrollbarSize = 0;
    // FIXME: We should get the size of the scrollbar from the RenderTheme instead of hard coding it here.
    if (m_div->renderer()->style()->overflow() != OHIDDEN)
        scrollbarSize = 15;
                
    m_height = line * rows + toAdd + scrollbarSize;
    
    RenderFlexibleBox::calcHeight();
}

short RenderTextField::baselinePosition(bool b, bool isRootLineBox) const
{
    if (m_multiLine)
        return height() + marginTop() + marginBottom();
    return RenderFlexibleBox::baselinePosition(b, isRootLineBox);
}

void RenderTextField::calcMinMaxWidth()
{
    m_minWidth = 0;
    m_maxWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value());
    else {
        // Figure out how big a text control needs to be for a given number of characters
        // (using "0" as the nominal character).
        const UChar ch = '0';
        float charWidth = style()->font().floatWidth(TextRun(&ch, 1), TextStyle(0, 0, 0, false, false, false));
        int factor;
        int scrollbarSize = 0;
        if (m_multiLine) {
            factor = static_cast<HTMLTextAreaElement*>(node())->cols();
            // FIXME: We should get the size of the scrollbar from the RenderTheme instead of hard coding it here.
            if (m_div->renderer()->style()->overflow() != OHIDDEN)
                scrollbarSize = 15;
        }
        else {
            factor = static_cast<HTMLInputElement*>(node())->size();
            if (factor <= 0)
                factor = 20;
        }
        m_maxWidth = (int)ceilf(charWidth * factor) + scrollbarSize;
    }
    
    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxWidth = max(m_maxWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minWidth = max(m_minWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minWidth = 0;
    else
        m_minWidth = m_maxWidth;
    
    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxWidth = min(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minWidth = min(m_minWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight() + 
                m_div->renderer()->paddingLeft() + m_div->renderer()->paddingRight();
    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();    
}

void RenderTextField::forwardEvent(Event* evt)
{
    if (evt->type() == blurEvent) {
        RenderObject* innerRenderer = m_div->renderer();
        if (innerRenderer) {
            RenderLayer* innerLayer = innerRenderer->layer();
            if (innerLayer)
                innerLayer->scrollToOffset(style()->direction() == RTL ? innerLayer->scrollWidth() : 0, 0);
        }
    } else
        m_div->defaultEventHandler(evt);
}

void RenderTextField::selectionChanged()
{
    // FIXME: Implement this, and find the right place to detect a selection change to call this method.
    // We also want to retain more information about selection for text controls, so this would be the place
    // to store that info.
}

}
