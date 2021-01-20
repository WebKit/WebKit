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
#include "RemoveNodeCommand.h"

#include "Editing.h"
#include "RenderElement.h"
#include <wtf/Assertions.h>

namespace WebCore {

RemoveNodeCommand::RemoveNodeCommand(Ref<Node>&& node, ShouldAssumeContentIsAlwaysEditable shouldAssumeContentIsAlwaysEditable, EditAction editingAction)
    : SimpleEditCommand(node->document(), editingAction)
    , m_node(WTFMove(node))
    , m_shouldAssumeContentIsAlwaysEditable(shouldAssumeContentIsAlwaysEditable)
{
    ASSERT(m_node->parentNode());
}

void RemoveNodeCommand::doApply()
{
    ContainerNode* parent = m_node->parentNode();
    if (!parent || (m_shouldAssumeContentIsAlwaysEditable == DoNotAssumeContentIsAlwaysEditable
        && !isEditableNode(*parent) && parent->renderer()))
        return;
    ASSERT(isEditableNode(*parent) || !parent->renderer());

    m_parent = parent;
    m_refChild = m_node->nextSibling();

    m_node->remove();
}

void RemoveNodeCommand::doUnapply()
{
    RefPtr<ContainerNode> parent = WTFMove(m_parent);
    RefPtr<Node> refChild = WTFMove(m_refChild);
    if (!parent || !parent->hasEditableStyle())
        return;

    parent->insertBefore(m_node, refChild.get());
}

#ifndef NDEBUG
void RemoveNodeCommand::getNodesInCommand(HashSet<Node*>& nodes)
{
    addNodeAndDescendants(m_parent.get(), nodes);
    addNodeAndDescendants(m_refChild.get(), nodes);
    addNodeAndDescendants(m_node.ptr(), nodes);
}
#endif

}
