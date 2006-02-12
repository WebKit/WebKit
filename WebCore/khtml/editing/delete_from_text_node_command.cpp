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
#include "delete_from_text_node_command.h"

#include "TextImpl.h"

#include <kxmlcore/Assertions.h>

using DOM::DocumentImpl;
using DOM::TextImpl;

namespace khtml {

DeleteFromTextNodeCommand::DeleteFromTextNodeCommand(DocumentImpl *document, TextImpl *node, int offset, int count)
    : EditCommand(document), m_node(node), m_offset(offset), m_count(count)
{
    ASSERT(m_node);
    ASSERT(m_offset >= 0);
    ASSERT(m_offset < (int)m_node->length());
    ASSERT(m_count >= 0);
}

void DeleteFromTextNodeCommand::doApply()
{
    ASSERT(m_node);

    int exceptionCode = 0;
    m_text = m_node->substringData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
    
    m_node->deleteData(m_offset, m_count, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void DeleteFromTextNodeCommand::doUnapply()
{
    ASSERT(m_node);
    ASSERT(!m_text.isEmpty());

    int exceptionCode = 0;
    m_node->insertData(m_offset, m_text, exceptionCode);
    ASSERT(exceptionCode == 0);
}

} // namespace khtml
