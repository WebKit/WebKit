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
#include "remove_node_command.h"

#include "xml/dom_nodeimpl.h"

#include <kxmlcore/Assertions.h>

using DOM::DocumentImpl;
using DOM::NodeImpl;

namespace khtml {

RemoveNodeCommand::RemoveNodeCommand(DocumentImpl *document, NodeImpl *removeChild)
    : EditCommand(document), m_parent(0), m_removeChild(removeChild), m_refChild(0)
{
    ASSERT(m_removeChild);
    m_removeChild->ref();

    m_parent = m_removeChild->parentNode();
    ASSERT(m_parent);
    m_parent->ref();
    
    m_refChild = m_removeChild->nextSibling();
    if (m_refChild)
        m_refChild->ref();
}

RemoveNodeCommand::~RemoveNodeCommand()
{
    ASSERT(m_parent);
    m_parent->deref();

    ASSERT(m_removeChild);
    m_removeChild->deref();

    if (m_refChild)
        m_refChild->deref();
}

void RemoveNodeCommand::doApply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    m_parent->removeChild(m_removeChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void RemoveNodeCommand::doUnapply()
{
    ASSERT(m_parent);
    ASSERT(m_removeChild);

    int exceptionCode = 0;
    m_parent->insertBefore(m_removeChild, m_refChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

} // namespace khtml
