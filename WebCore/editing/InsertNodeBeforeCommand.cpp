/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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
#include "InsertNodeBeforeCommand.h"

#include "htmlediting.h"

namespace WebCore {

InsertNodeBeforeCommand::InsertNodeBeforeCommand(PassRefPtr<Node> insertChild, PassRefPtr<Node> refChild)
    : SimpleEditCommand(refChild->document())
    , m_insertChild(insertChild)
    , m_refChild(refChild)
{
    ASSERT(m_insertChild);
    ASSERT(!m_insertChild->parentNode());
    ASSERT(m_refChild);
    ASSERT(m_refChild->parentNode());

    ASSERT(enclosingNodeOfType(Position(m_refChild->parentNode(), 0), isContentEditable) || !m_refChild->parentNode()->attached());
}

void InsertNodeBeforeCommand::doApply()
{
    Node* parent = m_refChild->parentNode();
    if (!parent)
        return;

    ExceptionCode ec;
    parent->insertBefore(m_insertChild.get(), m_refChild.get(), ec);
}

void InsertNodeBeforeCommand::doUnapply()
{
    ExceptionCode ec;
    m_insertChild->remove(ec);
}

}
