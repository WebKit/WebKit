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

#include "wrap_contents_in_dummy_span_command.h"

#include "apply_style_command.h"
#include "xml/dom_elementimpl.h"

#if APPLE_CHANGES
#include <kxmlcore/Assertions.h>
#else
#define ASSERT(assertion) assert(assertion)
#endif

using DOM::DocumentImpl;
using DOM::ElementImpl;

namespace khtml {

WrapContentsInDummySpanCommand::WrapContentsInDummySpanCommand(DOM::DocumentImpl *document, DOM::ElementImpl *element)
    : EditCommand(document), m_element(element), m_dummySpan(0)
{
    ASSERT(m_element);

    m_element->ref();
}

WrapContentsInDummySpanCommand::~WrapContentsInDummySpanCommand()
{
    if (m_dummySpan)
        m_dummySpan->deref();

    ASSERT(m_element);
    m_element->deref();
}

void WrapContentsInDummySpanCommand::doApply()
{
    ASSERT(m_element);

    int exceptionCode = 0;

    if (!m_dummySpan) {
        m_dummySpan = createStyleSpanElement(document());
        m_dummySpan->ref();
    }

    while (m_element->firstChild()) {
        m_dummySpan->appendChild(m_element->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element->appendChild(m_dummySpan, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void WrapContentsInDummySpanCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(m_dummySpan);

    ASSERT(m_element->firstChild() == m_dummySpan);
    ASSERT(!m_element->firstChild()->nextSibling());

    int exceptionCode = 0;

    while (m_dummySpan->firstChild()) {
        m_element->appendChild(m_dummySpan->firstChild(), exceptionCode);
        ASSERT(exceptionCode == 0);
    }

    m_element->removeChild(m_dummySpan, exceptionCode);
    ASSERT(exceptionCode == 0);
}

} // namespace khtml
