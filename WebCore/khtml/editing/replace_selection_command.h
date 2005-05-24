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

#ifndef __replace_selection_command_h__
#define __replace_selection_command_h__

#include "composite_edit_command.h"

namespace DOM {
    class DocumentFragmentImpl;
}

namespace khtml {

class NodeDesiredStyle
{
public:
    NodeDesiredStyle(DOM::NodeImpl *, DOM::CSSMutableStyleDeclarationImpl *);
    NodeDesiredStyle(const NodeDesiredStyle &);
    ~NodeDesiredStyle();
    
    DOM::NodeImpl *node() const { return m_node; }
    DOM::CSSMutableStyleDeclarationImpl *style() const { return m_style; }

    NodeDesiredStyle &operator=(const NodeDesiredStyle &);

private:
    DOM::NodeImpl *m_node;
    DOM::CSSMutableStyleDeclarationImpl *m_style;
};

// --- ReplacementFragment helper class

class ReplacementFragment
{
public:
    ReplacementFragment(DOM::DocumentImpl *, DOM::DocumentFragmentImpl *, bool matchStyle);
    ~ReplacementFragment();

    enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

    DOM::DocumentFragmentImpl *root() const { return m_fragment; }
    DOM::NodeImpl *firstChild() const;
    DOM::NodeImpl *lastChild() const;

    DOM::NodeImpl *mergeStartNode() const;

    const QValueList<NodeDesiredStyle> &desiredStyles() { return m_styles; }
        
    void pruneEmptyNodes();

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
    
    static bool isInterchangeNewlineNode(const DOM::NodeImpl *);
    static bool isInterchangeConvertedSpaceSpan(const DOM::NodeImpl *);

    DOM::NodeImpl *insertFragmentForTestRendering();
    void restoreTestRenderingNodesToFragment(DOM::NodeImpl *);
    void computeStylesUsingTestRendering(DOM::NodeImpl *);
    void removeUnrenderedNodesUsingTestRendering(DOM::NodeImpl *);
    int countRenderedBlocks(DOM::NodeImpl *holder);
    void removeStyleNodes();

    // A couple simple DOM helpers
    DOM::NodeImpl *enclosingBlock(DOM::NodeImpl *) const;
    void removeNode(DOM::NodeImpl *);
    void removeNodePreservingChildren(DOM::NodeImpl *);
    void insertNodeBefore(DOM::NodeImpl *node, DOM::NodeImpl *refNode);

    EFragmentType m_type;
    DOM::DocumentImpl *m_document;
    DOM::DocumentFragmentImpl *m_fragment;
    QValueList<NodeDesiredStyle> m_styles;
    bool m_matchStyle;
    bool m_hasInterchangeNewlineAtStart;
    bool m_hasInterchangeNewlineAtEnd;
    bool m_hasMoreThanOneBlock;
};

class ReplaceSelectionCommand : public CompositeEditCommand
{
public:
    ReplaceSelectionCommand(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, bool selectReplacement=true, bool smartReplace=false, bool matchStyle=false);
    virtual ~ReplaceSelectionCommand();
    
    virtual void doApply();
    virtual EditAction editingAction() const;

private:
    void completeHTMLReplacement(const DOM::Position &lastPositionToSelect);

    void insertNodeAfterAndUpdateNodesInserted(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAtAndUpdateNodesInserted(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset);
    void insertNodeBeforeAndUpdateNodesInserted(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);

    void updateNodesInserted(DOM::NodeImpl *);
    void fixupNodeStyles(const QValueList<NodeDesiredStyle> &);
    void removeLinePlaceholderIfNeeded(DOM::NodeImpl *);

    ReplacementFragment m_fragment;
    DOM::NodeImpl *m_firstNodeInserted;
    DOM::NodeImpl *m_lastNodeInserted;
    DOM::NodeImpl *m_lastTopNodeInserted;
    DOM::CSSMutableStyleDeclarationImpl *m_insertionStyle;
    bool m_selectReplacement;
    bool m_smartReplace;
    bool m_matchStyle;
};

} // namespace khtml

#endif // __replace_selection_command_h__
