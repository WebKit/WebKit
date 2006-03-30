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

#include <algorithm>
#include "Document.h"
#include "Frame.h"
#include "RenderText.h"
#include "HTMLNames.h"
#include "HTMLInputElement.h"
#include "HTMLTextFieldInnerElement.h"
#include "SelectionController.h"
#include "VisiblePosition.h"
#include "TextIterator.h"

namespace WebCore {

using namespace HTMLNames;
using namespace std;

RenderTextField::RenderTextField(Node* node)
:RenderBlock(node), m_dirty(false)
{
}

RenderTextField::~RenderTextField()
{
    // The renderer for the div has already been destroyed by destroyLeftoverChildren
    if (m_div)
        m_div->detach();
}

void RenderTextField::setStyle(RenderStyle* style)
{
    RenderBlock::setStyle(style);
    if (m_div) {
        RenderBlock* divRenderer = static_cast<RenderBlock*>(m_div->renderer());
        RenderStyle* divStyle = createDivStyle(style);
        divRenderer->setStyle(divStyle);
        for (Node *n = m_div->firstChild(); n; n = n->traverseNextNode(m_div.get()))
            if (n->renderer())
                n->renderer()->setStyle(divStyle);
    }
    setReplaced(isInline());
}

RenderStyle* RenderTextField::createDivStyle(RenderStyle* startStyle)
{
    RenderStyle* divStyle = new (renderArena()) RenderStyle();
    divStyle->inheritFrom(startStyle);
    divStyle->setDisplay(BLOCK);
    divStyle->setOverflow(OHIDDEN);
    divStyle->setWhiteSpace(NOWRAP);
    divStyle->setUserModify(READ_WRITE);
    // We're adding this extra pixel of padding to match WinIE.
    divStyle->setPaddingLeft(Length(1, Fixed));
    divStyle->setPaddingRight(Length(1, Fixed));
    return divStyle;
}

void RenderTextField::updateFromElement()
{
    if (element()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(element());
        String value = input->value().copy();
        if (value.isNull())
            value = "";
        unsigned ml = input->maxLength();
        bool valueHasChanged = false;
        if (value.length() > ml) {
            value.truncate(ml);
            valueHasChanged = true;
        }
            
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
            RenderBlock::addChild(divRenderer);
        }
        
        if (!input->valueMatchesRenderer()) {
            String oldText = this->text();
            value.replace(QChar('\\'), backslashAsCurrencySymbol());
            if (value != oldText) {
                ExceptionCode ec = 0;
                m_div->setInnerText(value, ec);
                setEdited(false);
            }
            input->setValueMatchesRenderer();
        }
        
        if (valueHasChanged)
            input->setValueFromRenderer(value);
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
    VisiblePosition endPosition = visiblePositionForIndex(end);
    
    ASSERT(startPosition.isNotNull() && endPosition.isNotNull());
    ASSERT(startPosition.deepEquivalent().node()->rootEditableElement() == m_div && endPosition.deepEquivalent().node()->rootEditableElement() == m_div);
    
    SelectionController sel = SelectionController(startPosition, endPosition);
    document()->frame()->setSelection(sel, false);
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
    HTMLInputElement* input = static_cast<HTMLInputElement*>(element());
    if (input) {
        input->setValueFromRenderer(text());
        setEdited(true);
    }
}

String RenderTextField::text()
{
    if (m_div)
        return m_div->textContent();
    return String();
}

void RenderTextField::calcMinMaxWidth()
{
    // Figure out how big a text field needs to be for a given number of characters
    // (using "0" as the nominal character).
    int size = static_cast<HTMLInputElement*>(element())->size();
    if (size <= 0)
        size = 20;
    
    QChar ch[1];
    ch[0] = '0';
    m_minWidth = m_maxWidth = (int)ceilf(style()->font().floatWidth(ch, 1, 0, 1, 0, 0) * size) + paddingLeft() + paddingRight() + 
                borderLeft() + borderRight() + m_div->renderer()->paddingLeft() + m_div->renderer()->paddingRight();
    setMinMaxKnown();
}

void RenderTextField::forwardEvent(Event* evt)
{
    m_div->defaultEventHandler(evt);
}

}
