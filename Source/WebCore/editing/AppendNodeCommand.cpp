/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "AppendNodeCommand.h"

#include "AXObjectCache.h"
#include "Document.h"
#include "Editing.h"
#include "RenderElement.h"
#include "Text.h"

namespace WebCore {

AppendNodeCommand::AppendNodeCommand(Ref<ContainerNode>&& parent, Ref<Node>&& node, EditAction editingAction)
    : SimpleEditCommand(parent->document(), editingAction)
    , m_parent(WTFMove(parent))
    , m_node(WTFMove(node))
{
    ASSERT(!m_node->parentNode());
    ASSERT(m_parent->hasEditableStyle() || !m_parent->renderer());
}

void AppendNodeCommand::doApply()
{
    if (!m_parent->hasEditableStyle() && m_parent->renderer())
        return;

    m_parent->appendChild(m_node);
}

void AppendNodeCommand::doUnapply()
{
    if (!m_parent->hasEditableStyle())
        return;

    m_node->remove();
}

#ifndef NDEBUG
void AppendNodeCommand::getNodesInCommand(HashSet<Node*>& nodes)
{
    addNodeAndDescendants(m_parent.ptr(), nodes);
    addNodeAndDescendants(m_node.ptr(), nodes);
}
#endif

} // namespace WebCore
