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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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

#include "Chrome.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "MouseEvent.h"
#include "Page.h"
#include "RenderBox.h"
#include "RenderTheme.h"
#include "ScrollbarTheme.h"
#include "WheelEvent.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SpinButtonElement);

using namespace HTMLNames;

inline SpinButtonElement::SpinButtonElement(Document& document, SpinButtonOwner& spinButtonOwner)
    : HTMLDivElement(divTag, document)
    , m_spinButtonOwner(&spinButtonOwner)
    , m_capturing(false)
    , m_upDownState(Indeterminate)
    , m_pressStartingState(Indeterminate)
    , m_repeatingTimer(*this, &SpinButtonElement::repeatingTimerFired)
{
    setHasCustomStyleResolveCallbacks();
}

Ref<SpinButtonElement> SpinButtonElement::create(Document& document, SpinButtonOwner& spinButtonOwner)
{
    auto element = adoptRef(*new SpinButtonElement(document, spinButtonOwner));
    static MainThreadNeverDestroyed<const AtomString> webkitInnerSpinButtonName("-webkit-inner-spin-button", AtomString::ConstructFromLiteral);
    element->setPseudo(webkitInnerSpinButtonName);
    return element;
}

void SpinButtonElement::willDetachRenderers()
{
    releaseCapture();
}

void SpinButtonElement::defaultEventHandler(Event& event)
{
    if (!is<MouseEvent>(event)) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    RenderBox* box = renderBox();
    if (!box) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    if (!shouldRespondToMouseEvents()) {
        if (!event.defaultHandled())
            HTMLDivElement::defaultEventHandler(event);
        return;
    }

    MouseEvent& mouseEvent = downcast<MouseEvent>(event);
    IntPoint local = roundedIntPoint(box->absoluteToLocal(mouseEvent.absoluteLocation(), UseTransforms));
    if (mouseEvent.type() == eventNames().mousedownEvent && mouseEvent.button() == LeftButton) {
        if (box->borderBoxRect().contains(local)) {
            // The following functions of HTMLInputElement may run JavaScript
            // code which detaches this shadow node. We need to take a reference
            // and check renderer() after such function calls.
            Ref<SpinButtonElement> protectedThis(*this);
            if (m_spinButtonOwner)
                m_spinButtonOwner->focusAndSelectSpinButtonOwner();
            if (renderer()) {
                if (m_upDownState != Indeterminate) {
                    // A JavaScript event handler called in doStepAction() below
                    // might change the element state and we might need to
                    // cancel the repeating timer by the state change. If we
                    // started the timer after doStepAction(), we would have no
                    // chance to cancel the timer.
                    startRepeatingTimer();
                    doStepAction(m_upDownState == Up ? 1 : -1);
                }
            }
            mouseEvent.setDefaultHandled();
        }
    } else if (mouseEvent.type() == eventNames().mouseupEvent && mouseEvent.button() == LeftButton)
        stopRepeatingTimer();
    else if (mouseEvent.type() == eventNames().mousemoveEvent) {
        if (box->borderBoxRect().contains(local)) {
            if (!m_capturing) {
                if (RefPtr<Frame> frame = document().frame()) {
                    frame->eventHandler().setCapturingMouseEventsElement(this);
                    m_capturing = true;
                    if (Page* page = document().page())
                        page->chrome().registerPopupOpeningObserver(*this);
                }
            }
            UpDownState oldUpDownState = m_upDownState;
            switch (renderer()->theme().innerSpinButtonLayout(*renderer())) {
            case RenderTheme::InnerSpinButtonLayout::Vertical:
                m_upDownState = local.y() < box->height() / 2 ? Up : Down;
                break;
            case RenderTheme::InnerSpinButtonLayout::HorizontalUpLeft:
                m_upDownState = local.x() < box->width() / 2 ? Up : Down;
                break;
            case RenderTheme::InnerSpinButtonLayout::HorizontalUpRight:
                m_upDownState = local.x() > box->width() / 2 ? Up : Down;
                break;
            }
            if (m_upDownState != oldUpDownState)
                renderer()->repaint();
        } else {
            releaseCapture();
            m_upDownState = Indeterminate;
        }
    }

    if (!mouseEvent.defaultHandled())
        HTMLDivElement::defaultEventHandler(mouseEvent);
}

void SpinButtonElement::willOpenPopup()
{
    releaseCapture();
    m_upDownState = Indeterminate;
}

void SpinButtonElement::forwardEvent(Event& event)
{
    if (!is<WheelEvent>(event))
        return;

    if (!m_spinButtonOwner)
        return;

    if (!m_spinButtonOwner->shouldSpinButtonRespondToWheelEvents())
        return;

    doStepAction(downcast<WheelEvent>(event).wheelDeltaY());
    event.setDefaultHandled();
}

bool SpinButtonElement::willRespondToMouseMoveEvents()
{
    if (renderBox() && shouldRespondToMouseEvents())
        return true;

    return HTMLDivElement::willRespondToMouseMoveEvents();
}

bool SpinButtonElement::willRespondToMouseClickEvents()
{
    if (renderBox() && shouldRespondToMouseEvents())
        return true;

    return HTMLDivElement::willRespondToMouseClickEvents();
}

void SpinButtonElement::doStepAction(int amount)
{
    if (!m_spinButtonOwner)
        return;

    if (amount > 0)
        m_spinButtonOwner->spinButtonStepUp();
    else if (amount < 0)
        m_spinButtonOwner->spinButtonStepDown();
}

void SpinButtonElement::releaseCapture()
{
    stopRepeatingTimer();
    if (m_capturing) {
        if (RefPtr<Frame> frame = document().frame()) {
            frame->eventHandler().setCapturingMouseEventsElement(nullptr);
            m_capturing = false;
            if (Page* page = document().page())
                page->chrome().unregisterPopupOpeningObserver(*this);
        }
    }
}

bool SpinButtonElement::matchesReadWritePseudoClass() const
{
    return shadowHost()->matchesReadWritePseudoClass();
}

void SpinButtonElement::startRepeatingTimer()
{
    m_pressStartingState = m_upDownState;
    ScrollbarTheme& theme = ScrollbarTheme::theme();
    m_repeatingTimer.start(theme.initialAutoscrollTimerDelay(), theme.autoscrollTimerDelay());
}

void SpinButtonElement::stopRepeatingTimer()
{
    m_repeatingTimer.stop();
}

void SpinButtonElement::step(int amount)
{
    if (!shouldRespondToMouseEvents())
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
    
void SpinButtonElement::repeatingTimerFired()
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

bool SpinButtonElement::shouldRespondToMouseEvents()
{
    return !m_spinButtonOwner || m_spinButtonOwner->shouldSpinButtonRespondToMouseEvents();
}

}
