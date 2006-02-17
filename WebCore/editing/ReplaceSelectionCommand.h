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
#include <kxmlcore/PassRefPtr.h>

namespace WebCore {

class DocumentFragmentImpl;

class NodeDesiredStyle {
public:
    NodeDesiredStyle(PassRefPtr<NodeImpl>, PassRefPtr<CSSMutableStyleDeclarationImpl>);
    
    NodeImpl* node() const { return m_node.get(); }
    CSSMutableStyleDeclarationImpl* style() const { return m_style.get(); }

private:
    RefPtr<NodeImpl> m_node;
    RefPtr<CSSMutableStyleDeclarationImpl> m_style;
};

// --- ReplacementFragment helper class

class ReplacementFragment
{
public:
    ReplacementFragment(DocumentImpl *, DocumentFragmentImpl *, bool matchStyle);
    ~ReplacementFragment();

    enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

    DocumentFragmentImpl *root() const { return m_fragment.get(); }
    NodeImpl *firstChild() const;
    NodeImpl *lastChild() const;

    NodeImpl *mergeStartNode() const;

    const QValueList<NodeDesiredStyle> &desiredStyles() { return m_styles; }

    EFragmentType type() const { return m_type; }
    bool isEmpty() const { return m_type == EmptyFragment; }
    bool isSingleTextNode() const { return m_type == SingleTextNodeFragment; }
    bool isTreeFragment() const { return m_type == TreeFragment; }

    bool hasMoreThanOneBlock() const { return m_hasMoreThanOneBlock; }
    bool hasInterchangeNewlineAtStart() const { return m_hasInterchangeNewlineAtStart; }
    bool hasInterchangeNewlineAtEnd() const { return m_hasInterchangeNewlineAtEnd; }

private:
    // no copy construction or assignment
    ReplacementFragment(const ReplacementFragment &);
    ReplacementFragment &operator=(const ReplacementFragment &);
    
    static bool isInterchangeNewlineNode(const NodeImpl *);
    static bool isInterchangeConvertedSpaceSpan(const NodeImpl *);

    PassRefPtr<NodeImpl> insertFragmentForTestRendering();
    void restoreTestRenderingNodesToFragment(NodeImpl *);
    void computeStylesUsingTestRendering(NodeImpl *);
    void removeUnrenderedNodesUsingTestRendering(NodeImpl *);
    int countRenderedBlocks(NodeImpl *holder);
    void removeStyleNodes();

    // A couple simple DOM helpers
    NodeImpl *enclosingBlock(NodeImpl *) const;
    void removeNode(PassRefPtr<NodeImpl>);
    void removeNodePreservingChildren(NodeImpl *);
    void insertNodeBefore(NodeImpl *node, NodeImpl *refNode);

    EFragmentType m_type;
    RefPtr<DocumentImpl> m_document;
    RefPtr<DocumentFragmentImpl> m_fragment;
    QValueList<NodeDesiredStyle> m_styles;
    bool m_matchStyle;
    bool m_hasInterchangeNewlineAtStart;
    bool m_hasInterchangeNewlineAtEnd;
    bool m_hasMoreThanOneBlock;
};

class ReplaceSelectionCommand : public CompositeEditCommand
{
public:
    ReplaceSelectionCommand(DocumentImpl *document, DocumentFragmentImpl *fragment, bool selectReplacement=true, bool smartReplace=false, bool matchStyle=false);
    virtual ~ReplaceSelectionCommand();
    
    virtual void doApply();
    virtual EditAction editingAction() const;

private:
    void completeHTMLReplacement(const Position &lastPositionToSelect);

    void insertNodeAfterAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild);
    void insertNodeAtAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild, int offset);
    void insertNodeBeforeAndUpdateNodesInserted(NodeImpl *insertChild, NodeImpl *refChild);

    void updateNodesInserted(NodeImpl *);
    void fixupNodeStyles(const QValueList<NodeDesiredStyle> &);
    void removeLinePlaceholderIfNeeded(NodeImpl *);

    ReplacementFragment m_fragment;
    RefPtr<NodeImpl> m_firstNodeInserted;
    RefPtr<NodeImpl> m_lastNodeInserted;
    RefPtr<NodeImpl> m_lastTopNodeInserted;
    RefPtr<CSSMutableStyleDeclarationImpl> m_insertionStyle;
    bool m_selectReplacement;
    bool m_smartReplace;
    bool m_matchStyle;
};

} // namespace khtml

#endif // __replace_selection_command_h__
