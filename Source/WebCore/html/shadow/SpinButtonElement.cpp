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
#include "RenderSearchField.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScrollbarTheme.h"
#include "SpeechInput.h"
#include "SpeechInputEvent.h"
#include "TextEvent.h"
#include "TextEventInputType.h"
#include "WheelEvent.h"

namespace WebCore {

using namespace HTMLNames;

TextControlInnerElement::TextControlInnerElement(Document* document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomCallbacks();
}

PassRefPtr<TextControlInnerElement> TextControlInnerElement::create(Document* document)
{
    return adoptRef(new TextControlInnerElement(document));
}

PassRefPtr<RenderStyle> TextControlInnerElement::customStyleForRenderer()
{
    RenderTextControlSingleLine* parentRenderer = toRenderTextControlSingleLine(shadowHost()->renderer());
    return parentRenderer->createInnerBlockStyle(parentRenderer->style());
}

// ----------------------------

inline TextControlInnerTextElement::TextControlInnerTextElement(Document* document)
    : HTMLDivElement(divTag, document)
{
    setHasCustomCallbacks();
}

PassRefPtr<TextControlInnerTextElement> TextControlInnerTextElement::create(Document* document)
{
    return adoptRef(new TextControlInnerTextElement(document));
}

void TextControlInnerTextElement::defaultEventHandler(Event* event)
{
    // FIXME: In the future, we should add a way to have default event listeners.
    // Then we would add one to the text field's inner div, and we wouldn't need this subclass.
    // Or possibly we could just use a normal event listener.
    if (event->isBeforeTextInsertedEvent() || event->type() == eventNames().webkitEditableContentChangedEvent) {
        Element* shadowAncestor = shadowHost();
        // A TextControlInnerTextElement can have no host if its been detached,
        // but kept alive by an EditCommand. In this case, an undo/redo can
        // cause events to be sent to the TextControlInnerTextElement. To
        // prevent an infinite loop, we must check for this case before sending
        // the event up the chain.
        if (shadowAncestor)
            shadowAncestor->defaultEventHandler(event);
    }
    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

RenderObject* TextControlInnerTextElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    bool multiLine = false;
    Element* shadowAncestor = shadowHost();
    if (shadowAncestor && shadowAncestor->renderer()) {
        ASSERT(shadowAncestor->renderer()->isTextField() || shadowAncestor->renderer()->isTextArea());
        multiLine = shadowAncestor->renderer()->isTextArea();
    }
    return new (arena) RenderTextControlInnerBlock(this, multiLine);
}

PassRefPtr<RenderStyle> TextControlInnerTextElement::customStyleForRenderer()
{
    RenderTextControl* parentRenderer = toRenderTextControl(shadowHost()->renderer());
    return parentRenderer->createInnerTextStyle(parentRenderer->style());
}

// ----------------------------

inline SearchFieldResultsButtonElement::SearchFieldResultsButtonElement(Document* document)
    : HTMLDivElement(divTag, document)
{
}

PassRefPtr<SearchFieldResultsButtonElement> SearchFieldResultsButtonElement::create(Document* document)
{
    return adoptRef(new SearchFieldResultsButtonElement(document));
}

const AtomicString& SearchFieldResultsButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, resultsId, ("-webkit-search-results-button"));
    DEFINE_STATIC_LOCAL(AtomicString, resultsDecorationId, ("-webkit-search-results-decoration"));
    DEFINE_STATIC_LOCAL(AtomicString, decorationId, ("-webkit-search-decoration"));
    Element* host = shadowHost();
    if (!host)
        return resultsId;
    if (HTMLInputElement* input = host->toInputElement()) {
        if (input->maxResults() < 0)
            return decorationId;
        if (input->maxResults() > 0)
            return resultsId;
        return resultsDecorationId;
    }
    return resultsId;
}

void SearchFieldResultsButtonElement::defaultEventHandler(Event* event)
{
    // On mousedown, bring up a menu, if needed
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowHost());
    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        input->focus();
        input->select();
        RenderSearchField* renderer = toRenderSearchField(input->renderer());
        if (renderer->popupIsVisible())
            renderer->hidePopup();
        else if (input->maxResults() > 0)
            renderer->showPopup();
        event->setDefaultHandled();
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool SearchFieldResultsButtonElement::willRespondToMouseClickEvents()
{
    return true;
}

// ----------------------------

inline SearchFieldCancelButtonElement::SearchFieldCancelButtonElement(Document* document)
    : HTMLDivElement(divTag, document)
    , m_capturing(false)
{
}

PassRefPtr<SearchFieldCancelButtonElement> SearchFieldCancelButtonElement::create(Document* document)
{
    return adoptRef(new SearchFieldCancelButtonElement(document));
}

const AtomicString& SearchFieldCancelButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, pseudoId, ("-webkit-search-cancel-button"));
    return pseudoId;
}

void SearchFieldCancelButtonElement::detach()
{
    if (m_capturing) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);
    }
    HTMLDivElement::detach();
}


void SearchFieldCancelButtonElement::defaultEventHandler(Event* event)
{
    // If the element is visible, on mouseup, clear the value, and set selection
    RefPtr<HTMLInputElement> input(static_cast<HTMLInputElement*>(shadowHost()));
    if (input->disabled() || input->readOnly()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

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
        if (m_capturing) {
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(0);
                m_capturing = false;
            }
            if (hovered()) {
                String oldValue = input->value();
                input->setValueForUser("");
                input->onSearch();
                event->setDefaultHandled();
            }
        }
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

bool SearchFieldCancelButtonElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowHost());
    if (!input->disabled() && !input->readOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

// ----------------------------

inline SpinButtonElement::SpinButtonElement(Document* document, StepActionHandler& stepActionHandler)
    : HTMLDivElement(divTag, document)
    , m_stepActionHandler(&stepActionHandler)
    , m_capturing(false)
    , m_upDownState(Indeterminate)
    , m_pressStartingState(Indeterminate)
    , m_repeatingTimer(this, &SpinButtonElement::repeatingTimerFired)
{
}

PassRefPtr<SpinButtonElement> SpinButtonElement::create(Document* document, StepActionHandler& stepActionHandler)
{
    return adoptRef(new SpinButtonElement(document, stepActionHandler));
}

const AtomicString& SpinButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, innerPseudoId, ("-webkit-inner-spin-button"));
    return innerPseudoId;
}

void SpinButtonElement::detach()
{
    releaseCapture();
    HTMLDivElement::detach();
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

    RefPtr<HTMLInputElement> input(static_cast<HTMLInputElement*>(shadowHost()));
    if (input->disabled() || input->readOnly()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent* mouseEvent = static_cast<MouseEvent*>(event);
    IntPoint local = roundedIntPoint(box->absoluteToLocal(mouseEvent->absoluteLocation(), false, true));
    if (mouseEvent->type() == eventNames().mousedownEvent && mouseEvent->button() == LeftButton) {
        if (box->pixelSnappedBorderBoxRect().contains(local)) {
            // The following functions of HTMLInputElement may run JavaScript
            // code which detaches this shadow node. We need to take a reference
            // and check renderer() after such function calls.
            RefPtr<Node> protector(this);
            input->focus();
            input->select();
            if (renderer()) {
                if (m_upDownState != Indeterminate) {
                    doStepAction(m_upDownState == Up ? 1 : -1);
                    if (renderer())
                        startRepeatingTimer();
                }
            }
            event->setDefaultHandled();
        }
    } else if (mouseEvent->type() == eventNames().mouseupEvent && mouseEvent->button() == LeftButton)
        stopRepeatingTimer();
    else if (event->type() == eventNames().mousemoveEvent) {
        if (box->pixelSnappedBorderBoxRect().contains(local)) {
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
            releaseCapture();
            m_upDownState = Indeterminate;
        }
    }

    if (!event->defaultHandled())
        HTMLDivElement::defaultEventHandler(event);
}

void SpinButtonElement::forwardEvent(Event* event)
{
    if (!renderBox())
        return;

    if (!event->hasInterface(eventNames().interfaceForWheelEvent))
        return;

    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowHost());
    if (input->disabled() || input->readOnly() || !input->focused())
        return;

    doStepAction(static_cast<WheelEvent*>(event)->wheelDeltaY());
    event->setDefaultHandled();
}

bool SpinButtonElement::willRespondToMouseMoveEvents()
{
    const HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowHost());
    if (renderBox() && !input->disabled() && !input->readOnly())
        return true;

    return HTMLDivElement::willRespondToMouseMoveEvents();
}

bool SpinButtonElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowHost());
    if (renderBox() && !input->disabled() && !input->readOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

void SpinButtonElement::doStepAction(int amount)
{
    if (!m_stepActionHandler)
        return;

    if (amount > 0)
        m_stepActionHandler->spinButtonStepUp();
    else if (amount < 0)
        m_stepActionHandler->spinButtonStepDown();
}

void SpinButtonElement::releaseCapture()
{
    stopRepeatingTimer();
    if (m_capturing) {
        if (Frame* frame = document()->frame()) {
            frame->eventHandler()->setCapturingMouseEventsNode(0);
            m_capturing = false;
        }
    }
}

bool SpinButtonElement::shouldMatchReadOnlySelector() const
{
    return shadowHost()->shouldMatchReadOnlySelector();
}

bool SpinButtonElement::shouldMatchReadWriteSelector() const
{
    return shadowHost()->shouldMatchReadWriteSelector();
}

void SpinButtonElement::startRepeatingTimer()
{
    m_pressStartingState = m_upDownState;
    ScrollbarTheme* theme = ScrollbarTheme::theme();
    m_repeatingTimer.start(theme->initialAutoscrollTimerDelay(), theme->autoscrollTimerDelay());
}

void SpinButtonElement::stopRepeatingTimer()
{
    m_repeatingTimer.stop();
}

void SpinButtonElement::step(int amount)
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowHost());
    if (input->disabled() || input->readOnly())
        return;
    // On Mac OS, NSStepper updates the value for the button under the mouse
    // cursor regardless of the button pressed at the beginning. So the
    // following check is not needed for Mac OS.
#if !OS(MAC_OS_X)
    if (m_upDownState != m_pressStartingState)
        return;
#endif
    doStepAction(amount);
}
    
void SpinButtonElement::repeatingTimerFired(Timer<SpinButtonElement>*)
{
    if (m_upDownState != Indeterminate)
        step(m_upDownState == Up ? 1 : -1);
}

void SpinButtonElement::setHovered(bool flag)
{
    if (!flag)
        m_upDownState = Indeterminate;
    HTMLDivElement::setHovered(flag);
}


// ----------------------------

#if ENABLE(INPUT_SPEECH)

inline InputFieldSpeechButtonElement::InputFieldSpeechButtonElement(Document* document)
    : HTMLDivElement(divTag, document)
    , m_capturing(false)
    , m_state(Idle)
    , m_listenerId(0)
{
}

InputFieldSpeechButtonElement::~InputFieldSpeechButtonElement()
{
    SpeechInput* speech = speechInput();
    if (speech && m_listenerId)  { // Could be null when page is unloading.
        if (m_state != Idle)
            speech->cancelRecognition(m_listenerId);
        speech->unregisterListener(m_listenerId);
    }
}

PassRefPtr<InputFieldSpeechButtonElement> InputFieldSpeechButtonElement::create(Document* document)
{
    return adoptRef(new InputFieldSpeechButtonElement(document));
}

void InputFieldSpeechButtonElement::defaultEventHandler(Event* event)
{
    // For privacy reasons, only allow clicks directly coming from the user.
    if (!ScriptController::processingUserGesture()) {
        HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // The call to focus() below dispatches a focus event, and an event handler in the page might
    // remove the input element from DOM. To make sure it remains valid until we finish our work
    // here, we take a temporary reference.
    RefPtr<HTMLInputElement> input(static_cast<HTMLInputElement*>(shadowHost()));

    if (input->disabled() || input->readOnly()) {
        if (!event->defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    // On mouse down, select the text and set focus.
    if (event->type() == eventNames().mousedownEvent && event->isMouseEvent() && static_cast<MouseEvent*>(event)->button() == LeftButton) {
        if (renderer() && renderer()->visibleToHitTesting()) {
            if (Frame* frame = document()->frame()) {
                frame->eventHandler()->setCapturingMouseEventsNode(this);
                m_capturing = true;
            }
        }
        RefPtr<InputFieldSpeechButtonElement> holdRefButton(this);
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

    if (event->type() == eventNames().clickEvent && m_listenerId) {
        switch (m_state) {
        case Idle:
            startSpeechInput();
            break;
        case Recording:
            stopSpeechInput();
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

bool InputFieldSpeechButtonElement::willRespondToMouseClickEvents()
{
    const HTMLInputElement* input = static_cast<HTMLInputElement*>(shadowHost());
    if (!input->disabled() && !input->readOnly())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

void InputFieldSpeechButtonElement::setState(SpeechInputState state)
{
    if (m_state != state) {
        m_state = state;
        shadowHost()->renderer()->repaint();
    }
}

SpeechInput* InputFieldSpeechButtonElement::speechInput()
{
    return SpeechInput::from(document()->page());
}

void InputFieldSpeechButtonElement::didCompleteRecording(int)
{
    setState(Recognizing);
}

void InputFieldSpeechButtonElement::didCompleteRecognition(int)
{
    setState(Idle);
}

void InputFieldSpeechButtonElement::setRecognitionResult(int, const SpeechInputResultArray& results)
{
    m_results = results;

    // The call to setValue() below dispatches an event, and an event handler in the page might
    // remove the input element from DOM. To make sure it remains valid until we finish our work
    // here, we take a temporary reference.
    RefPtr<HTMLInputElement> input(static_cast<HTMLInputElement*>(shadowHost()));
    if (input->disabled() || input->readOnly())
        return;

    RefPtr<InputFieldSpeechButtonElement> holdRefButton(this);
    if (document() && document()->domWindow()) {
        // Call selectionChanged, causing the element to cache the selection,
        // so that the text event inserts the text in this element even if
        // focus has moved away from it.
        input->selectionChanged(false);
        input->dispatchEvent(TextEvent::create(document()->domWindow(), results.isEmpty() ? "" : results[0]->utterance(), TextEventInputOther));
    }

    // This event is sent after the text event so the website can perform actions using the input field content immediately.
    // It provides alternative recognition hypotheses and notifies that the results come from speech input.
    input->dispatchEvent(SpeechInputEvent::create(eventNames().webkitspeechchangeEvent, results));

    // Check before accessing the renderer as the above event could have potentially turned off
    // speech in the input element, hence removing this button and renderer from the hierarchy.
    if (renderer())
        renderer()->repaint();
}

void InputFieldSpeechButtonElement::attach()
{
    ASSERT(!m_listenerId);
    if (SpeechInput* input = SpeechInput::from(document()->page()))
        m_listenerId = input->registerListener(this);
    HTMLDivElement::attach();
}

void InputFieldSpeechButtonElement::detach()
{
    if (m_capturing) {
        if (Frame* frame = document()->frame())
            frame->eventHandler()->setCapturingMouseEventsNode(0);
    }

    if (m_listenerId) {
        if (m_state != Idle)
            speechInput()->cancelRecognition(m_listenerId);
        speechInput()->unregisterListener(m_listenerId);
        m_listenerId = 0;
    }

    HTMLDivElement::detach();
}

void InputFieldSpeechButtonElement::startSpeechInput()
{
    if (m_state != Idle)
        return;

    RefPtr<HTMLInputElement> input = static_cast<HTMLInputElement*>(shadowHost());
    AtomicString language = input->computeInheritedLanguage();
    String grammar = input->getAttribute(webkitgrammarAttr);
    IntRect rect = document()->view()->contentsToRootView(getPixelSnappedRect());
    if (speechInput()->startRecognition(m_listenerId, rect, language, grammar, document()->securityOrigin()))
        setState(Recording);
}

void InputFieldSpeechButtonElement::stopSpeechInput()
{
    if (m_state == Recording)
        speechInput()->stopRecording(m_listenerId);
}

const AtomicString& InputFieldSpeechButtonElement::shadowPseudoId() const
{
    DEFINE_STATIC_LOCAL(AtomicString, pseudoId, ("-webkit-input-speech-button"));
    return pseudoId;
}

#endif // ENABLE(INPUT_SPEECH)

}
