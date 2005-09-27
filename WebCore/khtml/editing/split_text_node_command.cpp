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

#include "split_text_node_command.h"

#include "xml/dom_docimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include <kxmlcore/Assertions.h>
#else
#define ASSERT(assertion) assert(assertion)
#endif

using DOM::DocumentImpl;
using DOM::TextImpl;
using DOM::NodeImpl;

namespace khtml {

SplitTextNodeCommand::SplitTextNodeCommand(DocumentImpl *document, TextImpl *text, int offset)
    : EditCommand(document), m_text1(0), m_text2(text), m_offset(offset)
{
    ASSERT(m_text2);
    ASSERT(m_text2->length() > 0);

    m_text2->ref();
}

SplitTextNodeCommand::~SplitTextNodeCommand()
{
    if (m_text1)
        m_text1->deref();

    ASSERT(m_text2);
    m_text2->deref();
}

void SplitTextNodeCommand::doApply()
{
    ASSERT(m_text2);
    ASSERT(m_offset > 0);

    int exceptionCode = 0;

    // EDIT FIXME: This should use better smarts for figuring out which portion
    // of the split to copy (based on their comparitive sizes). We should also
    // just use the DOM's splitText function.
    
    if (!m_text1) {
        // create only if needed.
        // if reapplying, this object will already exist.
        m_text1 = document()->createTextNode(m_text2->substringData(0, m_offset, exceptionCode));
        ASSERT(exceptionCode == 0);
        ASSERT(m_text1);
        m_text1->ref();
    }

    document()->copyMarkers(m_text2, 0, m_offset, m_text1, 0);
    m_text2->deleteData(0, m_offset, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_text2->parentNode()->insertBefore(m_text1, m_text2, exceptionCode);
    ASSERT(exceptionCode == 0);
        
    ASSERT(m_text2->previousSibling()->isTextNode());
    ASSERT(m_text2->previousSibling() == m_text1);
}

void SplitTextNodeCommand::doUnapply()
{
    ASSERT(m_text1);
    ASSERT(m_text2);
    ASSERT(m_text1->nextSibling() == m_text2);
        
    int exceptionCode = 0;
    m_text2->insertData(0, m_text1->data(), exceptionCode);
    ASSERT(exceptionCode == 0);

    document()->copyMarkers(m_text1, 0, m_offset, m_text2, 0);

    m_text2->parentNode()->removeChild(m_text1, exceptionCode);
    ASSERT(exceptionCode == 0);

    m_offset = m_text1->length();
}

} // namespace khtml
