/*
 * Copyright (C) 2005, 2008, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DeleteFromTextNodeCommand.h"

#include "Editing.h"
#include "Document.h"
#include "Text.h"

namespace WebCore {

DeleteFromTextNodeCommand::DeleteFromTextNodeCommand(Ref<Text>&& node, unsigned offset, unsigned count, EditAction editingAction)
    : SimpleEditCommand(node->document(), editingAction)
    , m_node(WTFMove(node))
    , m_offset(offset)
    , m_count(count)
{
    ASSERT(m_offset <= m_node->length());
    ASSERT(m_offset + m_count <= m_node->length());
}

void DeleteFromTextNodeCommand::doApply()
{
    if (!isEditableNode(m_node))
        return;

    auto result = m_node->substringData(m_offset, m_count);
    if (result.hasException())
        return;
    m_text = result.releaseReturnValue();
    m_node->deleteData(m_offset, m_count);
}

void DeleteFromTextNodeCommand::doUnapply()
{
    if (!m_node->hasEditableStyle())
        return;

    m_node->insertData(m_offset, m_text);
}

#ifndef NDEBUG
void DeleteFromTextNodeCommand::getNodesInCommand(HashSet<Node*>& nodes)
{
    addNodeAndDescendants(m_node.ptr(), nodes);
}
#endif

} // namespace WebCore
