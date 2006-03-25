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

#include "EditCommand.h"
#include "qvaluelist.h"

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
    CompositeEditCommand(WebCore::Document *);

    virtual void doUnapply();
    virtual void doReapply();

protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(WebCore::Node *appendChild, WebCore::Node *parentNode);
    void applyCommandToComposite(EditCommandPtr &);
    void applyStyle(WebCore::CSSStyleDeclaration *style, EditAction editingAction=EditActionChangeAttributes);
    void applyStyle(WebCore::CSSStyleDeclaration *style, WebCore::Position start, WebCore::Position end, EditAction editingAction=EditActionChangeAttributes);
    void applyStyledElement(Element*);
    void removeStyledElement(Element*);
    void deleteKeyPressed();
    void deleteSelection(bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteSelection(const Selection &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteTextFromNode(WebCore::Text *node, int offset, int count);
    void inputText(const WebCore::String &text, bool selectInsertedText = false);
    void insertNodeAfter(WebCore::Node *insertChild, WebCore::Node *refChild);
    void insertNodeAt(WebCore::Node *insertChild, WebCore::Node *refChild, int offset);
    void insertNodeBefore(WebCore::Node *insertChild, WebCore::Node *refChild);
    void insertParagraphSeparator();
    void insertTextIntoNode(WebCore::Text *node, int offset, const WebCore::String &text);
    void joinTextNodes(WebCore::Text *text1, WebCore::Text *text2);
    void rebalanceWhitespace();
    void rebalanceWhitespaceAt(const WebCore::Position &position);
    void removeCSSProperty(WebCore::CSSStyleDeclaration *, int property);
    void removeFullySelectedNode(WebCore::Node *node);
    void removeNodeAttribute(WebCore::Element *, const WebCore::QualifiedName& attribute);
    void removeChildrenInRange(WebCore::Node *node, int from, int to);
    void removeNode(WebCore::Node *removeChild);
    void removeNodePreservingChildren(WebCore::Node *node);
    void replaceTextInNode(WebCore::Text *node, int offset, int count, const WebCore::String &replacementText);
    WebCore::Position positionOutsideTabSpan(const WebCore::Position& pos);
    void insertNodeAtTabSpanPosition(WebCore::Node *node, const WebCore::Position& pos);
    void setNodeAttribute(WebCore::Element *, const WebCore::QualifiedName& attribute, const WebCore::String &);
    void splitTextNode(WebCore::Text *text, int offset);
    void splitElement(WebCore::Element *element, WebCore::Node *atChild);
    void mergeIdenticalElements(WebCore::Element *first, WebCore::Element *second);
    void wrapContentsInDummySpan(WebCore::Element *element);
    void splitTextNodeContainingElement(WebCore::Text *text, int offset);

    void deleteInsignificantText(WebCore::Text *, int start, int end);
    void deleteInsignificantText(const WebCore::Position &start, const WebCore::Position &end);
    void deleteInsignificantTextDownstream(const WebCore::Position &);

    WebCore::Node *appendBlockPlaceholder(WebCore::Node *);
    WebCore::Node *insertBlockPlaceholder(const WebCore::Position &pos);
    WebCore::Node *addBlockPlaceholderIfNeeded(WebCore::Node *);
    bool removeBlockPlaceholder(WebCore::Node *);
    WebCore::Node *findBlockPlaceholder(WebCore::Node *);

    void moveParagraphContentsToNewBlockIfNecessary(const WebCore::Position &);
    
    void pushAnchorElementDown(WebCore::Node*);
    void pushPartiallySelectedAnchorElementsDown();

    DeprecatedValueList<EditCommandPtr> m_cmds;
};

} // namespace WebCore

#endif // __composite_edit_command_h__
