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

#ifndef ReplaceSelectionCommand_h
#define ReplaceSelectionCommand_h

#include "CompositeEditCommand.h"

namespace WebCore {

class DocumentFragment;

enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

// --- ReplacementFragment helper class

class ReplacementFragment : Noncopyable {
public:
    ReplacementFragment(Document*, DocumentFragment*, bool matchStyle, const Selection&);

    Node* firstChild() const;
    Node* lastChild() const;

    bool isEmpty() const;
    
    bool hasInterchangeNewlineAtStart() const { return m_hasInterchangeNewlineAtStart; }
    bool hasInterchangeNewlineAtEnd() const { return m_hasInterchangeNewlineAtEnd; }
    
    void removeNode(PassRefPtr<Node>);
    void removeNodePreservingChildren(Node*);

private:
    PassRefPtr<Node> insertFragmentForTestRendering(Node* context);
    void removeUnrenderedNodes(Node*);
    void restoreTestRenderingNodesToFragment(Node*);
    void removeInterchangeNodes(Node*);
    
    void insertNodeBefore(Node* node, Node* refNode);

    RefPtr<Document> m_document;
    RefPtr<DocumentFragment> m_fragment;
    bool m_matchStyle;
    bool m_hasInterchangeNewlineAtStart;
    bool m_hasInterchangeNewlineAtEnd;
};

class ReplaceSelectionCommand : public CompositeEditCommand {
public:
    ReplaceSelectionCommand(Document*, PassRefPtr<DocumentFragment>,
        bool selectReplacement = true, bool smartReplace = false, bool matchStyle = false, bool preventNesting = true, bool movingParagraph = false,
        EditAction = EditActionPaste);
    
    virtual void doApply();
    virtual EditAction editingAction() const;

private:
    void completeHTMLReplacement(const Position& lastPositionToSelect);

    void insertNodeAfterAndUpdateNodesInserted(Node* insertChild, Node* refChild);
    void insertNodeAtAndUpdateNodesInserted(Node*, const Position&);
    void insertNodeBeforeAndUpdateNodesInserted(Node* insertChild, Node* refChild);

    void updateNodesInserted(Node*);
    bool shouldRemoveEndBR(Node*, const VisiblePosition&);
    
    bool shouldMergeStart(bool, bool);
    bool shouldMergeEnd(bool);
    bool shouldMerge(const VisiblePosition&, const VisiblePosition&);
    
    void negateStyleRulesThatAffectAppearance();
    void removeRedundantStyles(Node*);
    
    void handlePasteAsQuotationNode();
    
    virtual void removeNodePreservingChildren(Node*);
    virtual void removeNodeAndPruneAncestors(Node*);
    
    VisiblePosition positionAtStartOfInsertedContent();
    VisiblePosition positionAtEndOfInsertedContent();

    RefPtr<Node> m_firstNodeInserted;
    RefPtr<Node> m_lastLeafInserted;
    RefPtr<CSSMutableStyleDeclaration> m_insertionStyle;
    bool m_selectReplacement;
    bool m_smartReplace;
    bool m_matchStyle;
    RefPtr<DocumentFragment> m_documentFragment;
    bool m_preventNesting;
    bool m_movingParagraph;
    EditAction m_editAction;
};

} // namespace WebCore

#endif // ReplaceSelectionCommand_h
