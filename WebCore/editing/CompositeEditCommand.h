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

#ifndef CompositeEditCommand_h
#define CompositeEditCommand_h

#include "EditCommand.h"
#include <wtf/Vector.h>

namespace WebCore {

class CSSStyleDeclaration;
class Text;

class CompositeEditCommand : public EditCommand {
public:
    CompositeEditCommand(Document*);

    bool isFirstCommand(EditCommand* c) { return !m_commands.isEmpty() && m_commands.first() == c; }

protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(Node* appendChild, Node* parentNode);
    void applyCommandToComposite(PassRefPtr<EditCommand>);
    void applyStyle(CSSStyleDeclaration*, EditAction = EditActionChangeAttributes);
    void applyStyle(CSSStyleDeclaration*, const Position& start, const Position& end, EditAction = EditActionChangeAttributes);
    void applyStyledElement(Element*);
    void removeStyledElement(Element*);
    void deleteSelection(bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool replace = false, bool expandForSpecialElements = true);
    void deleteSelection(const Selection&, bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool replace = false, bool expandForSpecialElements = true);
    virtual void deleteTextFromNode(Text* node, int offset, int count);
    void inputText(const String&, bool selectInsertedText = false);
    void insertNodeAfter(Node* insertChild, Node* refChild);
    void insertNodeAt(Node* insertChild, const Position&);
    void insertNodeBefore(Node* insertChild, Node* refChild);
    void insertParagraphSeparator(bool useDefaultParagraphElement = false);
    void insertLineBreak();
    void insertTextIntoNode(Text* node, int offset, const String& text);
    void joinTextNodes(Text*, Text*);
    void rebalanceWhitespace();
    void rebalanceWhitespaceAt(const Position&);
    void prepareWhitespaceAtPositionForSplit(Position& position);
    void removeCSSProperty(CSSStyleDeclaration*, int property);
    void removeNodeAttribute(Element*, const QualifiedName& attribute);
    void removeChildrenInRange(Node*, int from, int to);
    virtual void removeNode(Node*);
    void removeNodePreservingChildren(Node*);
    void removeNodeAndPruneAncestors(Node*);
    void prune(PassRefPtr<Node>);
    void replaceTextInNode(Text* node, int offset, int count, const String& replacementText);
    Position positionOutsideTabSpan(const Position&);
    void insertNodeAtTabSpanPosition(Node*, const Position&);
    void setNodeAttribute(Element*, const QualifiedName& attribute, const String& value);
    void splitTextNode(Text*, int offset);
    void splitElement(Element*, Node* atChild);
    void mergeIdenticalElements(Element*, Element*);
    void wrapContentsInDummySpan(Element*);
    void splitTextNodeContainingElement(Text*, int offset);

    void deleteInsignificantText(Text*, int start, int end);
    void deleteInsignificantText(const Position& start, const Position& end);
    void deleteInsignificantTextDownstream(const Position&);

    Node *appendBlockPlaceholder(Node*);
    Node *insertBlockPlaceholder(const Position&);
    Node *addBlockPlaceholderIfNeeded(Node*);
    void removePlaceholderAt(const VisiblePosition&);

    Node* moveParagraphContentsToNewBlockIfNecessary(const Position&);
    
    void pushAnchorElementDown(Node*);
    void pushPartiallySelectedAnchorElementsDown();
    
    void moveParagraph(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, bool preserveSelection = false, bool preserveStyle = true);
    void moveParagraphs(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, bool preserveSelection = false, bool preserveStyle = true);
    
    bool breakOutOfEmptyListItem();
    
    Position positionAvoidingSpecialElementBoundary(const Position&, bool alwaysAvoidAnchors = true);
    
    Node* splitTreeToNode(Node*, Node*, bool splitAncestor = false);

    Vector<RefPtr<EditCommand> > m_commands;

private:
    virtual void doUnapply();
    virtual void doReapply();
};

} // namespace WebCore

#endif // CompositeEditCommand_h
