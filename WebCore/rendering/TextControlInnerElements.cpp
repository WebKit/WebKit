/*
 * Copyright (C) 2006, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "Page.h"
#include "RenderLayer.h"
#include "RenderTextControlSingleLine.h"
#include "ScrollbarTheme.h"
#include "SpeechInput.h"

namespace WebCore {

using namespace HTMLNames;

class RenderTextControlInnerBlock : public RenderBlock {
public:
    RenderTextControlInnerBlock(Node* node, bool isMultiLine) : RenderBlock(node), m_multiLine(isMultiLine) { }

private:
    virtual bool nodeAtPoint(const HitTestRequest&, HitTestResult&, int x, int y, int tx, int ty, HitTestAction);
    virtual VisiblePosition positionForPoint(const IntPoint&);

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

// ----------------------------

TextControlInnerElement::TextControlInnerElement(Document* document, HTMLElement* shadowParent)
    : HTMLDivElement(divTag, document)
    , m_shadowParent(shadowParent)
{
}

PassRefPtr<TextControlInnerElement> TextControlInnerElement::create(HTMLElement* shadowParent)
{
    return adoptRef(new TextControlInnerElement(shadowParent->document(), shadowParent));
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
    if (!m_shadowParent) {
        // FIXME: This code seems very wrong.  Why are we magically adding |this| to the DOM here?
        //        We shouldn't be calling parser API methods outside of the parser!
        parent->deprecatedParserAddChild(this);
    }
 
    // Add the renderer to the render tree
    if (renderer)
        parent->renderer()->addChild(renderer);
}

// ----------------------------

inline TextControlInnerTextElement::TextControlInnerTextElement(Document* document, HTMLElement* shadowParent)
    : TextControlInnerElement(document, shadowParent)
{
}

PassRefPtr<TextControlInnerTextElement> TextControlInnerTextElement::create(Document* document, HTMLElement* shadowParent)
{
    return adoptRef(new TextControlInnerTextElement(document, shadowParent));
}

void TextControlInnerTextElement::defaultEventHandler(Event* event)
{
    // FIXME: In the future, we should add a way to have default event listeners.
    // Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    // Or possibly we could just use a normal event listener.
    if (event->isBeforeTextInsertedEvent() || event->type() == eventNames().webkitEditableContentChangedEvent) {
        if (Node* shadowAncestor = shadowAncestorNode())
            shadowAncestor->defaultEventHandler(event);
    }
    if (event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
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

// ----------------------------

inline SearchFieldResultsButtonElement::SearchFieldResultsButtonElement(Document* document)
    : TextControlInnerElement(document)
{
}

PassRefPtr<SearchFieldResultsButtonElement> SearchFieldResultsButtonElement::create(Document* document)
{
    return adoptRef(new SearchFieldResultsButtonElement(document));
}

void SearchFieldResultsButtonElement::defaultEventHandler(Event* event)
{
    // On mousedown, bring up a menu, if needed
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        input->focus();
        input->select();
        RenderTextControlSingleLine* renderer = toRenderTextControlSingleLine(input->renderer());
        if (renderer->popupIsVisible())
            renderer->hidePopup();
        else if (input->maxResults() > 0)
            renderer->showPopup();
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

// ----------------------------

inline SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(Document* document)
    : TextControlInnerElement(document)
    , m_capturing(false)
{
}

PassRefPtr<SearchFieldCancelButtonElement> SearchFieldCancelButtonElement::create(Document* document)
{
    return adoptRef(new SearchFieldCancelButtonElement(document));
}

void SearchFieldCancelButtonElement::detach()
{
    if (m_capturing) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);      
    }
    TextControlInnerElement::detach();
}


void SearchFieldCancelButtonElement::defaultEventHandler(Event* event)
{
    // If the element is visible, on mouseup, clear the value, and set selection
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        if (renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(this);
                m_capturing = true;
            }
        }
        input->focus();
        input->select();
        event->setDefaultHandled();
    }
    if (event->type() == eventNames().mouseupEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        if (m_capturing && renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(0);
                m_capturing = false;
            }
            if (hovered()) {
                RefPtr<HTMLInputElement> protector(input);
                String oldValue = input->value();
                input->setValue("");
                if (!oldValue.isEmpty()) {
                    toRenderTextControl(input->renderer())->setChangedSinceLastChangeEvent(true);
                    input->dispatchEvent(Event::create(eventNames().inputEvent, true, false));
                }
                input->onSearch();
                event->setDefaultHandled();
            }
        }
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

// ----------------------------

inline SpinButtonElement::SpinButtonElement(HTMLElement* shadowParent)
    : TextControlInnerElement(shadowParent->document(), shadowParent)
    , m_capturing(false)
    , m_upDownState(Indeterminate)
    , m_pressStartingState(Indeterminate)
    , m_repeatingTimer(this, &SpinButtonElement::repeatingTimerFired)
{
}

PassRefPtr<SpinButtonElement> SpinButtonElement::create(HTMLElement* shadowParent)
{
    return adoptRef(new SpinButtonElement(shadowParent));
}

void SpinButtonElement::defaultEventHandler(Event* event)
{
    if (!event->isMouseEvent()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    RenderBox* box = renderBox();
    if (!box) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;        
    }
    
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (input->disabled() || input->isReadOnlyFormControl()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    IntPoint local = roundedIntPoint(box->absoluteToLocal(mouseEvent->absoluteLocation(), false, true));
    if (mouseEvent->type() == eventNames().mousedownEvent && mouseEvent->button() == LeftButton) {
        if (box->borderBoxRect().contains(local)) {
            RefPtr<Node> protector(input);
            input->focus();
            input->select();
            input->stepUpFromRenderer(m_upDownState == Up ? 1 : -1);
            event->setDefaultHandled();
            startRepeatingTimer();
        }
    } else if (mouseEvent->type() == eventNames().mouseupEvent && mouseEvent->button() == LeftButton)
        stopRepeatingTimer();
    else if (event->type() == eventNames().mousemoveEvent) {
        if (box->borderBoxRect().contains(local)) {
            if (!m_capturing) {
                if (Frame* frame = document()->frame()) {
                    frame->eventHandler()->setCapturingMouseEventsNode(this);
                    m_capturing = true;
                }
            }
            UpDownState oldUpDownState = m_upDownState;
            m_upDownState = local.y() < box->height() / 2 ? Up : Down;
            if (m_upDownState != oldUpDownState)
                renderer()->repaint();
        } else {
            if (m_capturing) {
                stopRepeatingTimer();
                if (Frame* frame = document()->frame()) {
                    frame->eventHandler()->setCapturingMouseEventsNode(0);
                    m_capturing = false;
                }
            }
        }
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

void SpinButtonElement::startRepeatingTimer()
{
    m_pressStartingState = m_upDownState;
    ScrollbarTheme* theme = ScrollbarTheme::nativeTheme();
    m_repeatingTimer.start(theme->initialAutoscrollTimerDelay(), theme->autoscrollTimerDelay());
}

void SpinButtonElement::stopRepeatingTimer()
{
    m_repeatingTimer.stop();
}

void SpinButtonElement::repeatingTimerFired(Timer<SpinButtonElement>*)
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (input->disabled() || input->isReadOnlyFormControl())
        return;
    // On Mac OS, NSStepper updates the value for the button under the mouse
    // cursor regardless of the button pressed at the beginning. So the
    // following check is not needed for Mac OS.
#if !OS(MAC_OS_X)
    if (m_upDownState != m_pressStartingState)
        return;
#endif
    input->stepUpFromRenderer(m_upDownState == Up ? 1 : -1);
}

void SpinButtonElement::setHovered(bool flag)
{
    if (!hovered() && flag)
        m_upDownState = Indeterminate;
    TextControlInnerElement::setHovered(flag);
}


// ----------------------------

#if ENABLE(INPUT_SPEECH)

inline InputFieldSpeechButtonElement::InputFieldSpeechButtonElement(HTMLElement* shadowParent)
    : TextControlInnerElement(shadowParent->document(), shadowParent)
    , m_capturing(false)
    , m_state(Idle)
    , m_listenerId(document()->page()->speechInput()->registerListener(this))
{
}

InputFieldSpeechButtonElement::~InputFieldSpeechButtonElement()
{
    SpeechInput* speech = speechInput();
    if (speech)  { // Could be null when page is unloading.
        if (m_state != Idle)
            speech->cancelRecognition(m_listenerId);
        speech->unregisterListener(m_listenerId);
    }
}

PassRefPtr<InputFieldSpeechButtonElement> InputFieldSpeechButtonElement::create(HTMLElement* shadowParent)
{
    return adoptRef(new InputFieldSpeechButtonElement(shadowParent));
}

void InputFieldSpeechButtonElement::defaultEventHandler(Event* event)
{
    // For privacy reasons, only allow clicks directly coming from the user.
    if (!event->fromUserGesture()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // On mouse down, select the text and set focus.
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        if (renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(this);
                m_capturing = true;
            }
        }
        // The call to focus() below dispatches a focus event, and an event handler in the page might
        // remove the input element from DOM. To make sure it remains valid until we finish our work
        // here, we take a temporary reference.
        RefPtr<HTMLInputElement> holdRef(input);
        input->focus();
        input->select();
        event->setDefaultHandled();
    }
    // On mouse up, release capture cleanly.
    if (event->type() == eventNames().mouseupEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        if (m_capturing && renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(0);
                m_capturing = false;
            }
        }
    }

    if (event->type() == eventNames().clickEvent) {
        switch (m_state) {
        case Idle:
            if (speechInput()->startRecognition(m_listenerId, input->renderer()->absoluteBoundingBoxRect()))
                setState(Recording);
            break;
        case Recording:
            speechInput()->stopRecording(m_listenerId);
            break;
        case Recognizing:
            // Nothing to do here, we will continue to wait for results.
            break;
        }
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

void InputFieldSpeechButtonElement::setState(SpeechInputState state)
{
    if (m_state != state) {
        m_state = state;
        shadowAncestorNode()->renderer()->repaint();
    }
}

SpeechInput* InputFieldSpeechButtonElement::speechInput()
{
    return document()->page() ? document()->page()->speechInput() : 0;
}

void InputFieldSpeechButtonElement::didCompleteRecording(int)
{
    setState(Recognizing);
}

void InputFieldSpeechButtonElement::didCompleteRecognition(int)
{
    setState(Idle);
}

void InputFieldSpeechButtonElement::setRecognitionResult(int, const String& result)
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowAncestorNode());
    // The call to setValue() below dispatches an event, and an event handler in the page might
    // remove the input element from DOM. To make sure it remains valid until we finish our work
    // here, we take a temporary reference.
    RefPtr<HTMLInputElement> holdRef(input);
    input->setValue(result);
    input->dispatchFormControlChangeEvent();
    renderer()->repaint();
}

void InputFieldSpeechButtonElement::detach()
{
    if (m_capturing) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);      
    }

    if (m_state != Idle)
      speechInput()->cancelRecognition(m_listenerId);

    TextControlInnerElement::detach();
}

#endif // ENABLE(INPUT_SPEECH)

}
