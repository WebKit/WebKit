/*
 * Copyright (C) 2006, 2008 Apple Inc.  All rights reserved.
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
#include "TextControlInnerElements.h"

#include "BeforeTextInsertedEvent.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLTextAreaElement.h"
#include "MouseEvent.h"
#include "RenderLayer.h"
#include "RenderTextControlSingleLine.h"

namespace WebCore {

class RenderTextControlInnerBlock : public RenderBlock {
public:
    RenderTextControlInnerBlock(Node* node, bool isMultiLine) : RenderBlock(node), m_multiLine(isMultiLine) { }

    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual VisiblePosition positionForPoint(const IntPoint&);
    private:
        bool m_multiLine;
};

bool RenderTextControlInnerBlock::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty, HitTestAction hitTestAction)
{
    RenderObject* renderer = node()->shadowAncestorNode()->renderer();

    bool placeholderIsVisible = false;
    if (renderer->isTextField())
        placeholderIsVisible = toRenderTextControlSingleLine(renderer)->placeholderIsVisible();

    return RenderBlock::nodeAtPoint(request, result, x, y, tx, ty, placeholderIsVisible ? HitTestBlockBackground : hitTestAction);
}

VisiblePosition RenderTextControlInnerBlock::positionForPoint(const IntPoint& point)
{
    IntPoint contentsPoint(point);

    // Multiline text controls have the scroll on shadowAncestorNode, so we need to take that
    // into account here.
    if (m_multiLine) {
        RenderTextControl* renderer = toRenderTextControl(node()->shadowAncestorNode()->renderer());
        if (renderer->hasOverflowClip())
            contentsPoint += renderer->layer()->scrolledContentOffset();
    }

    return RenderBlock::positionForPoint(contentsPoint);
}

TextControlInnerElement::TextControlInnerElement(Document* doc, Node* shadowParent)
    : HTMLDivElement(HTMLNames::divTag, doc)
    , m_shadowParent(shadowParent)
{
}

void TextControlInnerElement::attachInnerElement(Node* parent, PassRefPtr<RenderStyle> style, RenderArena* arena)
{
    // When adding these elements, create the renderer & style first before adding to the DOM.
    // Otherwise, the render tree will create some anonymous blocks that will mess up our layout.

    // Create the renderer with the specified style
    RenderObject* renderer = createRenderer(arena, style.get());
    if (renderer) {
        setRenderer(renderer);
        renderer->setStyle(style);
    }
    
    // Set these explicitly since this normally happens during an attach()
    setAttached();
    setInDocument();
    
    // For elements without a shadow parent, add the node to the DOM normally.
    if (!m_shadowParent)
        parent->addChild(this);
    
    // Add the renderer to the render tree
    if (renderer)
        parent->renderer()->addChild(renderer);
}

TextControlInnerTextElement::TextControlInnerTextElement(Document* doc, Node* shadowParent)
    : TextControlInnerElement(doc, shadowParent)
{
}

void TextControlInnerTextElement::defaultEventHandler(Event* evt)
{
    // FIXME: In the future, we should add a way to have default event listeners.  Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    Node* shadowAncestor = shadowAncestorNode();
    if (shadowAncestor) {
        if (evt->isBeforeTextInsertedEvent() || evt->type() == eventNames().webkitEditableContentChangedEvent)
            shadowAncestor->defaultEventHandler(evt);
    }
    if (!evt->defaultHandled())
        HTMLDivElement::defaultEventHandler(evt);
}

RenderObject* TextControlInnerTextElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    bool multiLine = false;
    Node* shadowAncestor = shadowAncestorNode();
    if (shadowAncestor && shadowAncestor->renderer()) {
        ASSERT(shadowAncestor->renderer()->isTextField() || shadowAncestor->renderer()->isTextArea());
        multiLine = shadowAncestor->renderer()->isTextArea();
    }
    return new (arena) RenderTextControlInnerBlock(this, multiLine);
}

SearchFieldResultsButtonElement::SearchFieldResultsButtonElement(Document* doc)
    : TextControlInnerElement(doc)
{
}

void SearchFieldResultsButtonElement::defaultEventHandler(Event* evt)
{
    // On mousedown, bring up a menu, if needed
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (evt->type() == eventNames().mousedownEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        input->focus();
        input->select();
        RenderTextControlSingleLine* renderer = toRenderTextControlSingleLine(input->renderer());
        if (renderer->popupIsVisible())
            renderer->hidePopup();
        else if (input->maxResults() > 0)
            renderer->showPopup();
        evt->setDefaultHandled();
    }
    if (!evt->defaultHandled())
        HTMLDivElement::defaultEventHandler(evt);
}

SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(Document* doc)
    : TextControlInnerElement(doc)
    , m_capturing(false)
{
}

void SearchFieldCancelButtonElement::detach()
{
    if (m_capturing) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);      
    }
    TextControlInnerElement::detach();
}


void SearchFieldCancelButtonElement::defaultEventHandler(Event* evt)
{
    // If the element is visible, on mouseup, clear the value, and set selection
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (evt->type() == eventNames().mousedownEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        input->focus();
        input->select();
        evt->setDefaultHandled();
        if (renderer() && renderer()->visibleToHitTesting())
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(this);
                m_capturing = true;
            }
    } else if (evt->type() == eventNames().mouseupEvent && evt->isMouseEvent() && static_cast<MouseEvent*>(evt)->button() == LeftButton) {
        if (m_capturing && renderer() && renderer()->visibleToHitTesting()) {
            if (hovered()) {
                input->setValue("");
                input->onSearch();
                evt->setDefaultHandled();
            }
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(0);
                m_capturing = false;
            }
        }
    }
    if (!evt->defaultHandled())
        HTMLDivElement::defaultEventHandler(evt);
}

SpinButtonElement::SpinButtonElement(Document* doc, Node* shadowParent)
    : TextControlInnerElement(doc, shadowParent)
    , m_capturing(false)
    , m_onUpButton(false)
{
}

void SpinButtonElement::defaultEventHandler(Event* evt)
{
    if (!evt->isMouseEvent()) {
        if (!evt->defaultHandled())
            HTMLDivElement::defaultEventHandler(evt);
        return;
    }
    const MouseEvent* mevt = static_cast<MouseEvent*>(evt);
    if (mevt->button() != LeftButton) {
        if (!evt->defaultHandled())
            HTMLDivElement::defaultEventHandler(evt);
        return;
    }

    RenderBox* box = renderBox();
    if (!box) {
        if (!evt->defaultHandled())
            HTMLDivElement::defaultEventHandler(evt);
        return;        
    }
    
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    IntPoint local = roundedIntPoint(box->absoluteToLocal(mevt->absoluteLocation(), false, true));
    if (evt->type() == eventNames().clickEvent) {
        if (box->borderBoxRect().contains(local)) {
            input->focus();
            input->select();
            if (local.y() < box->y() + box->height() / 2)
                input->stepUpFromRenderer(1);
            else
                input->stepUpFromRenderer(-1);
            evt->setDefaultHandled();
        }
    } else if (evt->type() == eventNames().mousemoveEvent) {
        if (box->borderBoxRect().contains(local)) {
            if (!m_capturing) {
                if (Frame* frame = document()->frame()) {
                    frame->eventHandler()->setCapturingMouseEventsNode(input);
                    m_capturing = true;
                }
            }
            bool oldOnUpButton = m_onUpButton;
            m_onUpButton = local.y() < box->y() + box->height() / 2;
            if (m_onUpButton != oldOnUpButton)
                renderer()->repaint();
        } else {
            if (m_capturing) {
                if (Frame* frame = document()->frame()) {
                    frame->eventHandler()->setCapturingMouseEventsNode(0);
                    m_capturing = false;
                }
            }
        }
    }
    if (!evt->defaultHandled())
        HTMLDivElement::defaultEventHandler(evt);
}

}
