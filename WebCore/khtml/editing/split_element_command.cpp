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

#include "split_element_command.h"

#include "xml/dom_elementimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#else
#define ASSERT(assertion) assert(assertion)
#endif

using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::NodeImpl;

namespace khtml {

SplitElementCommand::SplitElementCommand(DOM::DocumentImpl *document, DOM::ElementImpl *element, DOM::NodeImpl *atChild)
    : EditCommand(document), m_element1(0), m_element2(element), m_atChild(atChild)
{
    ASSERT(m_element2);
    ASSERT(m_atChild);

    m_element2->ref();
    m_atChild->ref();
}

SplitElementCommand::~SplitElementCommand()
{
    if (m_element1)
        m_element1->deref();

    ASSERT(m_element2);
    m_element2->deref();
    ASSERT(m_atChild);
    m_atChild->deref();
}

void SplitElementCommand::doApply()
{
    ASSERT(m_element2);
    ASSERT(m_atChild);

    int exceptionCode = 0;

    if (!m_element1) {
        // create only if needed.
        // if reapplying, this object will already exist.
        m_element1 = static_cast<ElementImpl *>(m_element2->cloneNode(false));
        ASSERT(m_element1);
        m_element1->ref();
    }

    m_element2->parent()->insertBefore(m_element1, m_element2, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    while (m_element2->firstChild() != m_atChild) {
        ASSERT(m_element2->firstChild());
        m_element1->appendChild(m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }
}

void SplitElementCommand::doUnapply()
{
    ASSERT(m_element1);
    ASSERT(m_element2);
    ASSERT(m_atChild);

    ASSERT(m_element1->nextSibling() == m_element2);
    ASSERT(m_element2->firstChild() && m_element2->firstChild() == m_atChild);

    int exceptionCode = 0;

    while (m_element1->lastChild()) {
        m_element2->insertBefore(m_element1->lastChild(), m_element2->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element2->parentNode()->removeChild(m_element1, exceptionCode);
    ASSERT(exceptionCode == 0);
}

} // namespace khtml
