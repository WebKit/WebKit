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

#include "append_node_command.h"

#include "xml/dom_nodeimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#else
#define ASSERT(assertion) assert(assertion)
#endif

using DOM::DocumentImpl;
using DOM::NodeImpl;

namespace khtml {

AppendNodeCommand::AppendNodeCommand(DocumentImpl *document, NodeImpl *appendChild, NodeImpl *parentNode)
    : EditCommand(document), m_appendChild(appendChild), m_parentNode(parentNode)
{
    ASSERT(m_appendChild);
    m_appendChild->ref();

    ASSERT(m_parentNode);
    m_parentNode->ref();
}

AppendNodeCommand::~AppendNodeCommand()
{
    ASSERT(m_appendChild);
    m_appendChild->deref();

    ASSERT(m_parentNode);
    m_parentNode->deref();
}

void AppendNodeCommand::doApply()
{
    ASSERT(m_appendChild);
    ASSERT(m_parentNode);

    int exceptionCode = 0;
    m_parentNode->appendChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

void AppendNodeCommand::doUnapply()
{
    ASSERT(m_appendChild);
    ASSERT(m_parentNode);
    ASSERT(state() == Applied);

    int exceptionCode = 0;
    m_parentNode->removeChild(m_appendChild, exceptionCode);
    ASSERT(exceptionCode == 0);
}

} // namespace khtml
