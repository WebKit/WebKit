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
#include "InsertIntoTextNodeCommand.h"

#include "Document.h"
#include "Frame.h"
#include "RenderText.h"
#include "Settings.h"
#include "Text.h"

#if PLATFORM(IOS_FAMILY)
#include "RenderText.h"
#endif

namespace WebCore {

InsertIntoTextNodeCommand::InsertIntoTextNodeCommand(Ref<Text>&& node, unsigned offset, const String& text, EditAction editingAction)
    : SimpleEditCommand(node->document(), editingAction)
    , m_node(WTFMove(node))
    , m_offset(offset)
    , m_text(text)
{
    ASSERT(m_offset <= m_node->length());
    ASSERT(!m_text.isEmpty());
}

void InsertIntoTextNodeCommand::doApply()
{
    bool passwordEchoEnabled = frame().settings().passwordEchoEnabled();
    if (passwordEchoEnabled)
        document().updateLayoutIgnorePendingStylesheets();

    if (!m_node->hasEditableStyle())
        return;

    if (passwordEchoEnabled) {
        if (RenderText* renderText = m_node->renderer())
            renderText->momentarilyRevealLastTypedCharacter(m_offset + m_text.length());
    }

    m_node->insertData(m_offset, m_text);
}

void InsertIntoTextNodeCommand::doReapply()
{
    if (!m_node->hasEditableStyle())
        return;

    m_node->insertData(m_offset, m_text);
}

void InsertIntoTextNodeCommand::doUnapply()
{
    if (!m_node->hasEditableStyle())
        return;

    m_node->deleteData(m_offset, m_text.length());
}

#ifndef NDEBUG

void InsertIntoTextNodeCommand::getNodesInCommand(HashSet<Node*>& nodes)
{
    addNodeAndDescendants(m_node.ptr(), nodes);
}

#endif

} // namespace WebCore
