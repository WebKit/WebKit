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

#include "HTMLNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDialogElement);

using namespace HTMLNames;

HTMLDialogElement::HTMLDialogElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
}

bool HTMLDialogElement::isOpen() const
{
    return m_isOpen;
}

const String& HTMLDialogElement::returnValue()
{
    return m_returnValue;
}

void HTMLDialogElement::setReturnValue(String&& returnValue)
{
    m_returnValue = WTFMove(returnValue);
}

void HTMLDialogElement::show()
{
    // If the element already has an open attribute, then return.
    if (isOpen())
        return;
    
    toggleOpen();
}

ExceptionOr<void> HTMLDialogElement::showModal()
{
    // If subject already has an open attribute, then throw an "InvalidStateError" DOMException.
    if (isOpen())
        return Exception { InvalidStateError };

    // If subject is not connected, then throw an "InvalidStateError" DOMException.
    if (!isConnected())
        return Exception { InvalidStateError };

    toggleOpen();
    return { };
}

void HTMLDialogElement::close(const String& returnValue)
{
    if (!isOpen())
        return;
    
    toggleOpen();
    if (!returnValue.isNull())
        m_returnValue = returnValue;
}

void HTMLDialogElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == HTMLNames::openAttr) {
        m_isOpen = !value.isNull();
        return;
    }
    
    HTMLElement::parseAttribute(name, value);
}

void HTMLDialogElement::toggleOpen()
{
    m_isOpen = !m_isOpen;
    setAttributeWithoutSynchronization(HTMLNames::openAttr, m_isOpen ? emptyAtom() : nullAtom());
}

}
