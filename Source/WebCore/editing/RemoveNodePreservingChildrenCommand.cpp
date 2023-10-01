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
#include "RemoveNodePreservingChildrenCommand.h"

#include "Editing.h"
#include "Node.h"
#include <wtf/Assertions.h>

namespace WebCore {

RemoveNodePreservingChildrenCommand::RemoveNodePreservingChildrenCommand(Ref<Node>&& node, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable, EditAction editingAction)
    : CompositeEditCommand(node->document(), editingAction)
    , m_node(WTFMove(node))
    , m_shouldAssumeContentIsAlwaysEditable(shouldAssumeContentIsAlwaysEditable)
{
}

void RemoveNodePreservingChildrenCommand::doApply()
{
    Vector<Ref<Node>> children;
    auto node = protectedNode();
    RefPtr parent { node->parentNode() };
    if (!parent || (m_shouldAssumeContentIsAlwaysEditable == DoNotAssumeContentIsAlwaysEditable && !isEditableNode(*parent)))
        return;

    for (Node* child = node->firstChild(); child; child = child->nextSibling())
        children.append(*child);

    size_t size = children.size();
    for (size_t i = 0; i < size; ++i) {
        auto child = WTFMove(children[i]);
        removeNode(child, m_shouldAssumeContentIsAlwaysEditable);
        insertNodeBefore(WTFMove(child), node, m_shouldAssumeContentIsAlwaysEditable);
    }
    removeNode(node, m_shouldAssumeContentIsAlwaysEditable);
}

}
