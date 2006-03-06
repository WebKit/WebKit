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
#include "WrapContentsInDummySpanCommand.h"

#include "ApplyStyleCommand.h"
#include "HTMLElementImpl.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

WrapContentsInDummySpanCommand::WrapContentsInDummySpanCommand(DOM::DocumentImpl *document, DOM::ElementImpl *element)
    : EditCommand(document), m_element(element)
{
    ASSERT(m_element);
}

void WrapContentsInDummySpanCommand::doApply()
{
    ASSERT(m_element);

    ExceptionCode ec = 0;

    if (!m_dummySpan)
        m_dummySpan = static_pointer_cast<HTMLElementImpl>(createStyleSpanElement(document()));
 
    while (m_element->firstChild()) {
        m_dummySpan->appendChild(m_element->firstChild(), ec);
        ASSERT(ec == 0);
    }

    m_element->appendChild(m_dummySpan.get(), ec);
    ASSERT(ec == 0);
}

void WrapContentsInDummySpanCommand::doUnapply()
{
    ASSERT(m_element);
    ASSERT(m_dummySpan);

    ASSERT(m_element->firstChild() == m_dummySpan);
    ASSERT(!m_element->firstChild()->nextSibling());

    ExceptionCode ec = 0;

    while (m_dummySpan->firstChild()) {
        m_element->appendChild(m_dummySpan->firstChild(), ec);
        ASSERT(ec == 0);
    }

    m_element->removeChild(m_dummySpan.get(), ec);
    ASSERT(ec == 0);
}

} // namespace khtml
