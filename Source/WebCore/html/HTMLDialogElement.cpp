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
#include "EventLoop.h"
#include "EventNames.h"

#include "HTMLNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDialogElement);

using namespace HTMLNames;

HTMLDialogElement::HTMLDialogElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

void HTMLDialogElement::show()
{
    // If the element already has an open attribute, then return.
    if (isOpen())
        return;

    setBooleanAttribute(openAttr, true);
}

ExceptionOr<void> HTMLDialogElement::showModal()
{
    // If subject already has an open attribute, then throw an "InvalidStateError" DOMException.
    if (isOpen())
        return Exception { InvalidStateError };

    // If subject is not connected, then throw an "InvalidStateError" DOMException.
    if (!isConnected())
        return Exception { InvalidStateError };

    setBooleanAttribute(openAttr, true);

    m_isModal = true;

    if (!isInTopLayer())
        addToTopLayer();

    // FIXME: Add steps 8 & 9 from spec. (webkit.org/b/227537)

    return { };
}

void HTMLDialogElement::close(const String& result)
{
    if (!isOpen())
        return;

    setBooleanAttribute(openAttr, false);

    m_isModal = false;

    if (!result.isNull())
        m_returnValue = result;

    if (isInTopLayer())
        removeFromTopLayer();

    // FIXME: Add step 6 from spec. (webkit.org/b/227537)

    document().eventLoop().queueTask(TaskSource::UserInteraction, [protectedThis = GCReachableRef { *this }] {
        protectedThis->dispatchEvent(Event::create(eventNames().closeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void HTMLDialogElement::queueCancelTask()
{
    document().eventLoop().queueTask(TaskSource::UserInteraction, [protectedThis = GCReachableRef { *this }] {
        auto cancelEvent = Event::create(eventNames().cancelEvent, Event::CanBubble::No, Event::IsCancelable::Yes);
        protectedThis->dispatchEvent(cancelEvent);
        if (!cancelEvent->defaultPrevented())
            protectedThis->close(nullString());
    });
}

}
