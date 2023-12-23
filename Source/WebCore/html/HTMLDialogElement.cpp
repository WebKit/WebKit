/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "HTMLDialogElement.h"

#include "CSSSelector.h"
#include "DocumentInlines.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FocusOptions.h"
#include "HTMLNames.h"
#include "PopoverData.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderElement.h"
#include "ScopedEventQueue.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDialogElement);

using namespace HTMLNames;

HTMLDialogElement::HTMLDialogElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

ExceptionOr<void> HTMLDialogElement::show()
{
    // If the element already has an open attribute, then return.
    if (isOpen()) {
        if (!isModal())
            return { };
        return Exception { ExceptionCode::InvalidStateError, "Cannot call show() on an open modal dialog."_s };
    }

    setBooleanAttribute(openAttr, true);

    m_previouslyFocusedElement = document().focusedElement();

    document().hideAllPopoversUntil(nullptr, FocusPreviousElement::No, FireEvents::No);

    runFocusingSteps();
    return { };
}

ExceptionOr<void> HTMLDialogElement::showModal()
{
    // If subject already has an open attribute, then throw an "InvalidStateError" DOMException.
    if (isOpen()) {
        if (isModal())
            return { };
        return Exception { ExceptionCode::InvalidStateError, "Cannot call showModal() on an open non-modal dialog."_s };
    }

    // If subject is not connected, then throw an "InvalidStateError" DOMException.
    if (!isConnected())
        return Exception { ExceptionCode::InvalidStateError, "Element is not connected."_s };

    if (isPopoverShowing())
        return Exception { ExceptionCode::InvalidStateError, "Element is already an open popover."_s };

    // setBooleanAttribute will dispatch a DOMSubtreeModified event.
    // Postpone callback execution that can potentially make the dialog disconnected.
    EventQueueScope scope;
    setBooleanAttribute(openAttr, true);

    setIsModal(true);

    if (!isInTopLayer())
        addToTopLayer();

    m_previouslyFocusedElement = document().focusedElement();

    document().hideAllPopoversUntil(nullptr, FocusPreviousElement::No, FireEvents::No);

    runFocusingSteps();

    return { };
}

void HTMLDialogElement::close(const String& result)
{
    if (!isOpen())
        return;

    setBooleanAttribute(openAttr, false);

    if (isModal())
        removeFromTopLayer();

    setIsModal(false);

    if (!result.isNull())
        m_returnValue = result;

    if (RefPtr element = std::exchange(m_previouslyFocusedElement, nullptr).get()) {
        FocusOptions options;
        options.preventScroll = true;
        element->focus(options);
    }

    queueTaskToDispatchEvent(TaskSource::UserInteraction, Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

void HTMLDialogElement::queueCancelTask()
{
    queueTaskKeepingThisNodeAlive(TaskSource::UserInteraction, [this] {
        auto cancelEvent = Event::create(eventNames().cancelEvent, Event::CanBubble::No, Event::IsCancelable::Yes);
        dispatchEvent(cancelEvent);
        if (!cancelEvent->defaultPrevented())
            close(nullString());
    });
}

// https://html.spec.whatwg.org/multipage/interactive-elements.html#dialog-focusing-steps
void HTMLDialogElement::runFocusingSteps()
{
    RefPtr<Element> control;
    if (m_isModal && hasAttributeWithoutSynchronization(HTMLNames::autofocusAttr))
        control = this;
    if (!control)
        control = findFocusDelegate();

    if (!control)
        control = this;

    if (control->isFocusable())
        control->runFocusingStepsForAutofocus();
    else if (m_isModal)
        document().setFocusedElement(nullptr); // Focus fixup rule

    if (!control->document().isSameOriginAsTopDocument())
        return;

    Ref topDocument = control->document().topDocument();
    topDocument->clearAutofocusCandidates();
    topDocument->setAutofocusProcessed();
}

void HTMLDialogElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    setIsModal(false);
}

void HTMLDialogElement::setIsModal(bool newValue)
{
    if (m_isModal == newValue)
        return;
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Modal, newValue);
    m_isModal = newValue;
}

}
