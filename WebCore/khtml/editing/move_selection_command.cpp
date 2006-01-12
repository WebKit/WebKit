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
#include "move_selection_command.h"
#include "replace_selection_command.h"

#include "DocumentFragmentImpl.h"

#include <kxmlcore/Assertions.h>

using DOM::DocumentImpl;
using DOM::DocumentFragmentImpl;
using DOM::NodeImpl;
using DOM::Position;

namespace khtml {

MoveSelectionCommand::MoveSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, Position &position, bool smartMove) 
    : CompositeEditCommand(document), m_fragment(fragment), m_position(position), m_smartMove(smartMove)
{
    ASSERT(m_fragment);
}

MoveSelectionCommand::~MoveSelectionCommand()
{
}

void MoveSelectionCommand::doApply()
{
    SelectionController selection = endingSelection();
    ASSERT(selection.isRange());

    Position pos = m_position;
    if (pos.isNull())
        return;
        
    // Update the position otherwise it may become invalid after the selection is deleted.
    NodeImpl *positionNode = m_position.node();
    int positionOffset = m_position.offset();
    Position selectionEnd = selection.end();
    int selectionEndOffset = selectionEnd.offset();    
    if (selectionEnd.node() == positionNode && selectionEndOffset < positionOffset) {
        positionOffset -= selectionEndOffset;
        Position selectionStart = selection.start();
        if (selectionStart.node() == positionNode) {
            positionOffset += selectionStart.offset();
        }
        pos = Position(positionNode, positionOffset);
    }

    deleteSelection(m_smartMove);

    // If the node for the destination has been removed as a result of the deletion,
    // set the destination to the ending point after the deletion.
    // Fixes: <rdar://problem/3910425> REGRESSION (Mail): Crash in ReplaceSelectionCommand; 
    //        selection is empty, leading to null deref
    if (!pos.node()->inDocument())
        pos = endingSelection().start();

    setEndingSelection(pos, endingSelection().startAffinity());
    EditCommandPtr cmd(new ReplaceSelectionCommand(document(), m_fragment.get(), true, m_smartMove));
    applyCommandToComposite(cmd);
}

EditAction MoveSelectionCommand::editingAction() const
{
    return EditActionDrag;
}

} // namespace khtml
