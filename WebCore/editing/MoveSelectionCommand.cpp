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
#include "MoveSelectionCommand.h"

#include "DocumentFragment.h"
#include "ReplaceSelectionCommand.h"

namespace WebCore {

MoveSelectionCommand::MoveSelectionCommand(PassRefPtr<DocumentFragment> fragment, const Position& position, bool smartMove) 
    : CompositeEditCommand(position.node()->document()), m_fragment(fragment), m_position(position), m_smartMove(smartMove)
{
    ASSERT(m_fragment);
}

void MoveSelectionCommand::doApply()
{
    VisibleSelection selection = endingSelection();
    ASSERT(selection.isRange());

    Position pos = m_position;
    if (pos.isNull())
        return;
        
    // Update the position otherwise it may become invalid after the selection is deleted.
    Node *positionNode = m_position.node();
    int positionOffset = m_position.deprecatedEditingOffset();
    Position selectionEnd = selection.end();
    int selectionEndOffset = selectionEnd.deprecatedEditingOffset();
    if (selectionEnd.node() == positionNode && selectionEndOffset < positionOffset) {
        positionOffset -= selectionEndOffset;
        Position selectionStart = selection.start();
        if (selectionStart.node() == positionNode) {
            positionOffset += selectionStart.deprecatedEditingOffset();
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

    setEndingSelection(VisibleSelection(pos, endingSelection().affinity()));
    if (!positionNode->inDocument()) {
        // Document was modified out from under us.
        return;
    }
    applyCommandToComposite(ReplaceSelectionCommand::create(positionNode->document(), m_fragment, true, m_smartMove));
}

EditAction MoveSelectionCommand::editingAction() const
{
    return EditActionDrag;
}

} // namespace WebCore
