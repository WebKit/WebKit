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
#include "SpinButtonElement.h"

#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "MouseEvent.h"
#include "RenderBox.h"
#include "ScrollbarTheme.h"
#include "WheelEvent.h"

namespace WebCore {

using namespace HTMLNames;

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

}
