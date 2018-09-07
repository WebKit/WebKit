/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
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
#include "MoveSelectionCommand.h"

#include "DeleteSelectionCommand.h"
#include "DocumentFragment.h"
#include "ReplaceSelectionCommand.h"

namespace WebCore {

MoveSelectionCommand::MoveSelectionCommand(Ref<DocumentFragment>&& fragment, const Position& position, bool smartInsert, bool smartDelete)
    : CompositeEditCommand(position.anchorNode()->document())
    , m_fragment(WTFMove(fragment))
    , m_position(position)
    , m_smartInsert(smartInsert)
    , m_smartDelete(smartDelete)
{
}

void MoveSelectionCommand::doApply()
{
    ASSERT(endingSelection().isNonOrphanedRange());

    Position pos = m_position;
    if (pos.isNull())
        return;

    // Update the position otherwise it may become invalid after the selection is deleted.
    Position selectionEnd = endingSelection().end();
    if (pos.anchorType() == Position::PositionIsOffsetInAnchor && selectionEnd.anchorType() == Position::PositionIsOffsetInAnchor
        && selectionEnd.containerNode() == pos.containerNode() && selectionEnd.offsetInContainerNode() < pos.offsetInContainerNode()) {
        pos.moveToOffset(pos.offsetInContainerNode() - selectionEnd.offsetInContainerNode());

        Position selectionStart = endingSelection().start();
        if (selectionStart.anchorType() == Position::PositionIsOffsetInAnchor && selectionStart.containerNode() == pos.containerNode())
            pos.moveToOffset(pos.offsetInContainerNode() + selectionStart.offsetInContainerNode());
    }

    {
        auto deleteSelection = DeleteSelectionCommand::create(document(), m_smartDelete, true, false, true, true, EditAction::DeleteByDrag);
        deleteSelection->setParent(this);
        deleteSelection->apply();
        m_commands.append(WTFMove(deleteSelection));
    }

    // If the node for the destination has been removed as a result of the deletion,
    // set the destination to the ending point after the deletion.
    // Fixes: <rdar://problem/3910425> REGRESSION (Mail): Crash in ReplaceSelectionCommand; 
    //        selection is empty, leading to null deref
    if (!pos.anchorNode()->isConnected())
        pos = endingSelection().start();

    cleanupAfterDeletion(pos);

    setEndingSelection(VisibleSelection(pos, endingSelection().affinity(), endingSelection().isDirectional()));
    setStartingSelection(endingSelection());
    if (!pos.anchorNode()->isConnected()) {
        // Document was modified out from under us.
        return;
    }
    OptionSet<ReplaceSelectionCommand::CommandOption> options { ReplaceSelectionCommand::SelectReplacement, ReplaceSelectionCommand::PreventNesting };
    if (m_smartInsert)
        options.add(ReplaceSelectionCommand::SmartReplace);

    {
        auto replaceSelection = ReplaceSelectionCommand::create(document(), WTFMove(m_fragment), options, EditAction::InsertFromDrop);
        replaceSelection->setParent(this);
        replaceSelection->apply();
        m_commands.append(WTFMove(replaceSelection));
    }
}

EditAction MoveSelectionCommand::editingAction() const
{
    return EditAction::DeleteByDrag;
}

} // namespace WebCore
