/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "SplitElementCommand.h"
#include "Element.h"

#include <wtf/Assertions.h>

namespace WebCore {

SplitElementCommand::SplitElementCommand(Element* element, Node* atChild)
    : EditCommand(element->document()), m_element2(element), m_atChild(atChild)
{
    ASSERT(m_element2);
    ASSERT(m_atChild);
}

void SplitElementCommand::doApply()
{
    ASSERT(m_element2);
    ASSERT(m_atChild);
    ASSERT(m_atChild->parentNode() == m_element2);

    ExceptionCode ec = 0;

    if (!m_element1) {
        // create only if needed.
        // if reapplying, this object will already exist.
        m_element1 = static_pointer_cast<Element>(m_element2->cloneNode(false));
        ASSERT(m_element1);
    }

    m_element2->parent()->insertBefore(m_element1.get(), m_element2.get(), ec);
    ASSERT(ec == 0);
    
    // Bail if we were asked to split at a bogus child, to avoid hanging below.
    if (!m_atChild || m_atChild->parentNode() != m_element2)
        return;
    
    while (m_element2->firstChild() != m_atChild) {
        ASSERT(m_element2->firstChild());
        m_element1->appendChild(m_element2->firstChild(), ec);
        ASSERT(ec == 0);
    }
}

void SplitElementCommand::doUnapply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);
    ASSERT(m_atChild);

    ASSERT(m_element1->nextSibling() == m_element2);
    ASSERT(m_element2->firstChild() && m_element2->firstChild() == m_atChild);

    ExceptionCode ec = 0;

    while (m_element1->lastChild()) {
        m_element2->insertBefore(m_element1->lastChild(), m_element2->firstChild(), ec);
        ASSERT(ec == 0);
    }

    m_element2->parentNode()->removeChild(m_element1.get(), ec);
    ASSERT(ec == 0);
}

} // namespace WebCore
