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

#ifndef __composite_edit_command_h__
#define __composite_edit_command_h__

#include "edit_command.h"
#include "qvaluelist.h"

namespace DOM {
    class CSSStyleDeclarationImpl;
    class DOMString;
    class TextImpl;
    class QualifiedName;
}

namespace khtml {

class CompositeEditCommand : public EditCommand
{
public:
    CompositeEditCommand(DOM::DocumentImpl *);
	
    virtual void doUnapply();
    virtual void doReapply();

protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(DOM::NodeImpl *appendChild, DOM::NodeImpl *parentNode);
    void applyCommandToComposite(EditCommandPtr &);
    void applyStyle(DOM::CSSStyleDeclarationImpl *style, EditAction editingAction=EditActionChangeAttributes);
    void deleteKeyPressed();
    void deleteSelection(bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteSelection(const SelectionController &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteTextFromNode(DOM::TextImpl *node, int offset, int count);
    void inputText(const DOM::DOMString &text, bool selectInsertedText = false);
    void insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAt(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, int offset);
    void insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertParagraphSeparator();
    void insertTextIntoNode(DOM::TextImpl *node, int offset, const DOM::DOMString &text);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void rebalanceWhitespace();
    void rebalanceWhitespaceAt(const DOM::Position &position);
    void removeCSSProperty(DOM::CSSStyleDeclarationImpl *, int property);
    void removeFullySelectedNode(DOM::NodeImpl *node);
    void removeNodeAttribute(DOM::ElementImpl *, const DOM::QualifiedName& attribute);
    void removeChildrenInRange(DOM::NodeImpl *node, int from, int to);
    void removeNode(DOM::NodeImpl *removeChild);
    void removeNodePreservingChildren(DOM::NodeImpl *node);
    void replaceTextInNode(DOM::TextImpl *node, int offset, int count, const DOM::DOMString &replacementText);
    DOM::Position positionOutsideTabSpan(const DOM::Position& pos);
    void insertNodeAtTabSpanPosition(DOM::NodeImpl *node, const DOM::Position& pos);
    void setNodeAttribute(DOM::ElementImpl *, const DOM::QualifiedName& attribute, const DOM::DOMString &);
    void splitTextNode(DOM::TextImpl *text, int offset);
    void splitElement(DOM::ElementImpl *element, DOM::NodeImpl *atChild);
    void mergeIdenticalElements(DOM::ElementImpl *first, DOM::ElementImpl *second);
    void wrapContentsInDummySpan(DOM::ElementImpl *element);
    void splitTextNodeContainingElement(DOM::TextImpl *text, int offset);

    void deleteInsignificantText(DOM::TextImpl *, int start, int end);
    void deleteInsignificantText(const DOM::Position &start, const DOM::Position &end);
    void deleteInsignificantTextDownstream(const DOM::Position &);

    DOM::NodeImpl *appendBlockPlaceholder(DOM::NodeImpl *);
    DOM::NodeImpl *insertBlockPlaceholder(const DOM::Position &pos);
    DOM::NodeImpl *addBlockPlaceholderIfNeeded(DOM::NodeImpl *);
    bool removeBlockPlaceholder(DOM::NodeImpl *);
    DOM::NodeImpl *findBlockPlaceholder(DOM::NodeImpl *);

    void moveParagraphContentsToNewBlockIfNecessary(const DOM::Position &);

    QValueList<EditCommandPtr> m_cmds;
};

} // namespace khtml

#endif // __composite_edit_command_h__
