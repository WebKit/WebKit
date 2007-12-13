/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "AppendNodeCommand.h"
#include "htmlediting.h"

namespace WebCore {

AppendNodeCommand::AppendNodeCommand(Node* parentNode, PassRefPtr<Node> childToAppend)
    : EditCommand(parentNode->document()), m_parentNode(parentNode), m_childToAppend(childToAppend)
{
    ASSERT(m_childToAppend);
    ASSERT(m_parentNode);
}

void AppendNodeCommand::doApply()
{
    ASSERT(m_childToAppend);
    ASSERT(m_parentNode);
    // If the child to append is already in a tree, appending it will remove it from it's old location
    // in an non-undoable way.  We might eventually find it useful to do an undoable remove in this case.
    ASSERT(!m_childToAppend->parent());
    ASSERT(enclosingNodeOfType(Position(m_parentNode.get(), 0), &isContentEditable) || !m_parentNode->attached());

    ExceptionCode ec = 0;
    m_parentNode->appendChild(m_childToAppend.get(), ec);
    ASSERT(ec == 0);
}

void AppendNodeCommand::doUnapply()
{
    ASSERT(m_childToAppend);
    ASSERT(m_parentNode);

    ExceptionCode ec = 0;
    m_parentNode->removeChild(m_childToAppend.get(), ec);
    ASSERT(ec == 0);
}

} // namespace WebCore
