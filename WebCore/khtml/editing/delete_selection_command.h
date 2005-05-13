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

#ifndef __delete_selection_command_h__
#define __delete_selection_command_h__

#include "composite_edit_command.h"

namespace khtml {

class DeleteSelectionCommand : public CompositeEditCommand
{ 
public:
    DeleteSelectionCommand(DOM::DocumentImpl *document, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    DeleteSelectionCommand(DOM::DocumentImpl *document, const Selection &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
	
    virtual void doApply();
    virtual EditAction editingAction() const;
    
private:
    virtual bool preservesTypingStyle() const;

    void initializePositionData();
    void saveTypingStyleState();
    void insertPlaceholderForAncestorBlockContent();
    bool handleSpecialCaseBRDelete();
    void handleGeneralDelete();
    void fixupWhitespace();
    void moveNodesAfterNode();
    void calculateEndingPosition();
    void calculateTypingStyleAfterDelete(DOM::NodeImpl *insertedPlaceholder);
    void clearTransientState();

    void setStartNode(DOM::NodeImpl *);

    bool m_hasSelectionToDelete;
    bool m_smartDelete;
    bool m_mergeBlocksAfterDelete;
    bool m_trailingWhitespaceValid;

    // This data is transient and should be cleared at the end of the doApply function.
    Selection m_selectionToDelete;
    DOM::Position m_upstreamStart;
    DOM::Position m_downstreamStart;
    DOM::Position m_upstreamEnd;
    DOM::Position m_downstreamEnd;
    DOM::Position m_endingPosition;
    DOM::Position m_leadingWhitespace;
    DOM::Position m_trailingWhitespace;
    DOM::NodeImpl *m_startBlock;
    DOM::NodeImpl *m_endBlock;
    DOM::NodeImpl *m_startNode;
    DOM::CSSMutableStyleDeclarationImpl *m_typingStyle;
};

} // namespace khtml

#endif // __delete_selection_command_h__
