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

#include "DocumentImpl.h"
#include "Frame.h"
#include "RenderText.h"
#include "htmlnames.h"
#include "HTMLInputElementImpl.h"
#include "HTMLTextFieldInnerElementImpl.h"
#include "dom_elementimpl.h"

namespace WebCore {

using namespace HTMLNames;

RenderTextField::RenderTextField(NodeImpl* node)
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
    
    return divStyle;
}

void RenderTextField::updateFromElement()
{
    if (element()->hasTagName(inputTag)) {
        HTMLInputElementImpl* input = static_cast<HTMLInputElementImpl*>(element());
        String value = input->value().copy();
        if (value.isNull())
            value = String("");
        unsigned ml = input->maxLength();
        bool valueHasChanged = false;
        if (value.length() > ml) {
            value.truncate(ml);
            valueHasChanged = true;
        }
            
        if (!m_div) {
            // Create the div and give it a parent, renderer, and style
            m_div = new HTMLTextFieldInnerElementImpl(document(), node());
            RenderBlock* divRenderer = new (renderArena()) RenderBlock(m_div.get());
            m_div->setRenderer(divRenderer);
            m_div->setAttached();
            m_div->setInDocument(true);
            // FIXME:  Address focus and event problems with this shadow DOM.
            // For example, events should appear to occur on the input element, but must take affect on this div.
            
            RenderStyle* divStyle = createDivStyle(style());
            divRenderer->setStyle(divStyle); 
            
            // Add div to Render tree
            RenderBlock::addChild(divRenderer);
        }
        
        TextImpl* text = static_cast<TextImpl*>(m_div->firstChild());
        if (!text) {
            // Add text node to DOM tree
            text = new TextImpl(document(), value.impl());
            int exception = 0;
            m_div->appendChild(text, exception);
        }
        
        if (!input->valueMatchesRenderer()) {
            String oldText = this->text();
            value.replace(QChar('\\'), backslashAsCurrencySymbol());
            if (value != oldText) {
                int exception = 0;
                text->setData(value, exception);
                setEdited(false);
                //FIXME: Update cursor as necessary
            }
            input->setValueMatchesRenderer();
        }
        
        if (valueHasChanged)
            input->setValueFromRenderer(value);
    }
}


int RenderTextField::selectionStart()
{
    // FIXME: Implement this.
    return 0;
}

int RenderTextField::selectionEnd()
{
    // FIXME: Implement this.
    return 0;
}

void RenderTextField::setSelectionStart(int start)
{
    // FIXME: Implement this.
}
 
void RenderTextField::setSelectionEnd(int end)
{
    // FIXME: Implement this.
}
    
void RenderTextField::select()
{
    DocumentImpl* doc = document();
    if (doc && m_div)
        doc->frame()->selectContentsOfNode(m_div.get());
}

void RenderTextField::setSelectionRange(int start, int end)
{
    // FIXME: Implement this.
}

void RenderTextField::subtreeHasChanged()
{
    HTMLInputElementImpl* input = static_cast<HTMLInputElementImpl*>(element());
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
    RenderBlock::calcMinMaxWidth();

    // Figure out how big a text field needs to be for a given number of characters
    // (using "w" as the nominal character).
    int size = static_cast<HTMLInputElementImpl*>(element())->size();
    if (size <= 0)
        size = 20;
    
    m_maxWidth = style()->font().width("w") * size + paddingLeft() + paddingRight() + borderLeft() + borderRight();
    setMinMaxKnown();
}

}
