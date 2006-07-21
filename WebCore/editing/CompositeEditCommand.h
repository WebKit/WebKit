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

#ifndef composite_edit_command_h__
#define composite_edit_command_h__

#include "EditCommand.h"
#include "DeprecatedValueList.h"

namespace WebCore {
    class CSSStyleDeclaration;
    class String;
    class Text;
    class QualifiedName;
}

namespace WebCore {

class CompositeEditCommand : public EditCommand
{
public:
    CompositeEditCommand(Document *);

    virtual void doUnapply();
    virtual void doReapply();

protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(Node *appendChild, Node *parentNode);
    void applyCommandToComposite(EditCommandPtr &);
    void applyStyle(CSSStyleDeclaration *style, EditAction editingAction=EditActionChangeAttributes);
    void applyStyle(CSSStyleDeclaration *style, Position start, Position end, EditAction editingAction=EditActionChangeAttributes);
    void applyStyledElement(Element*);
    void removeStyledElement(Element*);
    void deleteKeyPressed();
    void deleteSelection(bool smartDelete=false, bool mergeBlocksAfterDelete=true, bool replace=false);
    void deleteSelection(const Selection &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true, bool replace=false);
    virtual void deleteTextFromNode(Text *node, int offset, int count);
    void inputText(const String &text, bool selectInsertedText = false);
    void insertNodeAfter(Node *insertChild, Node *refChild);
    void insertNodeAt(Node *insertChild, Node *refChild, int offset);
    void insertNodeBefore(Node *insertChild, Node *refChild);
    void insertParagraphSeparator();
    void insertTextIntoNode(Text *node, int offset, const String &text);
    void joinTextNodes(Text *text1, Text *text2);
    void rebalanceWhitespace();
    void rebalanceWhitespaceAt(const Position &position);
    void removeCSSProperty(CSSStyleDeclaration *, int property);
    void removeNodeAttribute(Element *, const QualifiedName& attribute);
    void removeChildrenInRange(Node *node, int from, int to);
    virtual void removeNode(Node*);
    void removeNodePreservingChildren(Node*);
    void removeNodeAndPruneAncestors(Node* node);
    void prune(PassRefPtr<Node> node);
    void replaceTextInNode(Text *node, int offset, int count, const String &replacementText);
    Position positionOutsideTabSpan(const Position& pos);
    void insertNodeAtTabSpanPosition(Node *node, const Position& pos);
    void setNodeAttribute(Element *, const QualifiedName& attribute, const String &);
    void splitTextNode(Text *text, int offset);
    void splitElement(Element *element, Node *atChild);
    void mergeIdenticalElements(Element *first, Element *second);
    void wrapContentsInDummySpan(Element *element);
    void splitTextNodeContainingElement(Text *text, int offset);

    void deleteInsignificantText(Text *, int start, int end);
    void deleteInsignificantText(const Position &start, const Position &end);
    void deleteInsignificantTextDownstream(const Position &);

    Node *appendBlockPlaceholder(Node*);
    Node *insertBlockPlaceholder(const Position &pos);
    Node *addBlockPlaceholderIfNeeded(Node*);
    void removeBlockPlaceholder(const VisiblePosition&);

    void moveParagraphContentsToNewBlockIfNecessary(const Position &);
    
    void pushAnchorElementDown(Node*);
    void pushPartiallySelectedAnchorElementsDown();
    
    void moveParagraph(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, bool preserveSelection = false, bool preserveStyle = true);
    void moveParagraphs(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, bool preserveSelection = false, bool preserveStyle = true);
    
    bool breakOutOfEmptyListItem();

    DeprecatedValueList<EditCommandPtr> m_cmds;
};

} // namespace WebCore

#endif // __composite_edit_command_h__
