/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
 
#include "config.h"
#include "HTMLTextFieldInnerElement.h"

#include "BeforeTextInsertedEvent.h"
#include "EventNames.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "RenderTextControl.h"

namespace WebCore {

using namespace EventNames;

HTMLTextFieldInnerElement::HTMLTextFieldInnerElement(Document* doc, Node* shadowParent)
    : HTMLDivElement(doc), m_shadowParent(shadowParent)
{
}

HTMLTextFieldInnerTextElement::HTMLTextFieldInnerTextElement(Document* doc, Node* shadowParent)
    : HTMLTextFieldInnerElement(doc, shadowParent)
{
}

void HTMLTextFieldInnerTextElement::defaultEventHandler(Event* evt)
{
    // FIXME: In the future, we should add a way to have default event listeners.  Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    Node* shadowAncestor = shadowAncestorNode();
    if (shadowAncestor && shadowAncestor->renderer()) {
        ASSERT(shadowAncestor->renderer()->isTextField() || shadowAncestor->renderer()->isTextArea());
        if (evt->isBeforeTextInsertedEvent())
            if (shadowAncestor->renderer()->isTextField())
                static_cast<HTMLInputElement*>(shadowAncestor)->defaultEventHandler(evt);
            else
                static_cast<HTMLTextAreaElement*>(shadowAncestor)->defaultEventHandler(evt);
        if (evt->type() == khtmlEditableContentChangedEvent)
            static_cast<RenderTextControl*>(shadowAncestor->renderer())->subtreeHasChanged();
    }
    if (!evt->defaultHandled())
        HTMLDivElement::defaultEventHandler(evt);
}

HTMLSearchFieldResultsButtonElement::HTMLSearchFieldResultsButtonElement(Document* doc)
    : HTMLTextFieldInnerElement(doc)
{
}

void HTMLSearchFieldResultsButtonElement::defaultEventHandler(Event* evt)
{
    // On mousedown, bring up a menu, if needed
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (evt->type() == mousedownEvent) {
        input->focus();
        input->select();
        if (input && input->renderer() && static_cast<RenderTextControl*>(input->renderer())->popupIsVisible())
            static_cast<RenderTextControl*>(input->renderer())->hidePopup();
        else if (input->maxResults() > 0)
            static_cast<RenderTextControl*>(input->renderer())->showPopup();
        evt->setDefaultHandled();
    }
    if (!evt->defaultHandled())
        HTMLDivElement::defaultEventHandler(evt);
}

HTMLSearchFieldCancelButtonElement::HTMLSearchFieldCancelButtonElement(Document* doc)
    : HTMLTextFieldInnerElement(doc)
{
}

void HTMLSearchFieldCancelButtonElement::defaultEventHandler(Event* evt)
{
    // If the element is visible, on mouseup, clear the value, and set selection
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (evt->type() == mousedownEvent) {
        input->focus();
        input->select();
        evt->setDefaultHandled();
    } else if (evt->type() == mouseupEvent) {
        if (renderer() && renderer()->style()->visibility() == VISIBLE) {
            input->setValue(String(""));
            static_cast<RenderTextControl*>(input->renderer())->onSearch();
            evt->setDefaultHandled();
        }
    }
    if (!evt->defaultHandled())
        HTMLDivElement::defaultEventHandler(evt);
}

}
