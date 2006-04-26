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

#ifndef replace_selection_command_h__
#define replace_selection_command_h__

#include "CompositeEditCommand.h"
#include <kxmlcore/HashMap.h>
#include <kxmlcore/Vector.h>

namespace WebCore {

class DocumentFragment;

enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

class RenderingInfo : public Shared<RenderingInfo> {
public:
    RenderingInfo(PassRefPtr<CSSMutableStyleDeclaration>, bool);
    
    CSSMutableStyleDeclaration* style() const { return m_style.get(); }
    bool isBlockFlow() const { return m_isBlockFlow; }
private:
    RefPtr<CSSMutableStyleDeclaration> m_style;
    bool m_isBlockFlow;
};

typedef Vector<RefPtr<WebCore::Node> > NodeVector;
typedef HashMap<Node*, RefPtr<RenderingInfo> > RenderingInfoMap;

// --- ReplacementFragment helper class

class ReplacementFragment
{
public:
    ReplacementFragment(Document*, DocumentFragment*, bool, Element*);
    ~ReplacementFragment();

    Node *firstChild() const;
    Node *lastChild() const;

    Node *mergeStartNode() const;

    const RenderingInfoMap& renderingInfo() const { return m_renderingInfo; }
    const NodeVector& nodes() const { return m_nodes; }

    EFragmentType type() const { return m_type; }
    bool isEmpty() const { return m_type == EmptyFragment; }
    bool isSingleTextNode() const { return m_type == SingleTextNodeFragment; }
    bool isTreeFragment() const { return m_type == TreeFragment; }

    bool hasMoreThanOneBlock() const { return m_hasMoreThanOneBlock; }
    bool hasInterchangeNewlineAtStart() const { return m_hasInterchangeNewlineAtStart; }
    bool hasInterchangeNewlineAtEnd() const { return m_hasInterchangeNewlineAtEnd; }
    
    bool isBlockFlow(Node*) const;
    
    void removeNode(PassRefPtr<Node>);

private:
    // no copy construction or assignment
    ReplacementFragment(const ReplacementFragment &);
    ReplacementFragment &operator=(const ReplacementFragment &);
    
    static bool isInterchangeNewlineNode(const Node *);
    static bool isInterchangeConvertedSpaceSpan(const Node *);
    
    PassRefPtr<Node> insertFragmentForTestRendering();
    void saveRenderingInfo(Node*);
    void computeStylesUsingTestRendering(Node*);
    void removeUnrenderedNodes(Node*);
    void restoreTestRenderingNodesToFragment(Node*);
    int renderedBlocks(Node*);
    void removeStyleNodes();

    Node *enclosingBlock(Node *) const;
    void removeNodePreservingChildren(Node *);
    void insertNodeBefore(Node *node, Node *refNode);

    EFragmentType m_type;
    RefPtr<Document> m_document;
    RefPtr<DocumentFragment> m_fragment;
    RenderingInfoMap m_renderingInfo;
    NodeVector m_nodes;
    bool m_matchStyle;
    bool m_hasInterchangeNewlineAtStart;
    bool m_hasInterchangeNewlineAtEnd;
    bool m_hasMoreThanOneBlock;
};

class ReplaceSelectionCommand : public CompositeEditCommand
{
public:
    ReplaceSelectionCommand(Document *document, DocumentFragment *fragment, bool selectReplacement=true, bool smartReplace=false, bool matchStyle=false, bool forceMergeStart=false);
    virtual ~ReplaceSelectionCommand();
    
    virtual void doApply();
    virtual EditAction editingAction() const;

private:
    void completeHTMLReplacement(const Position &lastPositionToSelect);

    void insertNodeAfterAndUpdateNodesInserted(Node *insertChild, Node *refChild);
    void insertNodeAtAndUpdateNodesInserted(Node *insertChild, Node *refChild, int offset);
    void insertNodeBeforeAndUpdateNodesInserted(Node *insertChild, Node *refChild);

    void updateNodesInserted(Node *);
    void fixupNodeStyles(const NodeVector&, const RenderingInfoMap&);
    void removeLinePlaceholderIfNeeded(Node *);
    
    bool shouldMergeStart(const ReplacementFragment&, const Selection&);
    bool shouldMergeEnd(const ReplacementFragment&, const Selection&);

    RefPtr<Node> m_firstNodeInserted;
    RefPtr<Node> m_lastNodeInserted;
    RefPtr<Node> m_lastTopNodeInserted;
    RefPtr<CSSMutableStyleDeclaration> m_insertionStyle;
    bool m_selectReplacement;
    bool m_smartReplace;
    bool m_matchStyle;
    RefPtr<DocumentFragment> m_documentFragment;
    bool m_forceMergeStart;
};

} // namespace WebCore

#endif // __replace_selection_command_h__
