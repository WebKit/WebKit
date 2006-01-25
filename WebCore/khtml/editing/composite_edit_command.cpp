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

#include "config.h"
#include "composite_edit_command.h"

#include "DocumentImpl.h"
#include "InlineTextBox.h"
#include "append_node_command.h"
#include "apply_style_command.h"
#include "delete_from_text_node_command.h"
#include "delete_selection_command.h"
#include "dom2_rangeimpl.h"
#include "dom_textimpl.h"
#include "htmlediting.h"
#include "htmlnames.h"
#include "insert_into_text_node_command.h"
#include "insert_node_before_command.h"
#include "insert_paragraph_separator_command.h"
#include "insert_text_command.h"
#include "join_text_nodes_command.h"
#include "merge_identical_elements_command.h"
#include "rebalance_whitespace_command.h"
#include "remove_css_property_command.h"
#include "remove_node_attribute_command.h"
#include "remove_node_command.h"
#include "remove_node_preserving_children_command.h"
#include "set_node_attribute_command.h"
#include "split_element_command.h"
#include "split_text_node_command.h"
#include "split_text_node_containing_element_command.h"
#include "visible_position.h"
#include "visible_units.h"
#include "wrap_contents_in_dummy_span_command.h"
#include <kxmlcore/Assertions.h>

namespace WebCore {

using namespace HTMLNames;

static const DOMString &blockPlaceholderClassString();

CompositeEditCommand::CompositeEditCommand(DocumentImpl *document) 
    : EditCommand(document)
{
}

void CompositeEditCommand::doUnapply()
{
    if (m_cmds.count() == 0) {
        return;
    }
    
    for (int i = m_cmds.count() - 1; i >= 0; --i)
        m_cmds[i]->unapply();

    setState(NotApplied);
}

void CompositeEditCommand::doReapply()
{
    if (m_cmds.count() == 0) {
        return;
    }

    for (QValueList<EditCommandPtr>::ConstIterator it = m_cmds.begin(); it != m_cmds.end(); ++it)
        (*it)->reapply();

    setState(Applied);
}

//
// sugary-sweet convenience functions to help create and apply edit commands in composite commands
//
void CompositeEditCommand::applyCommandToComposite(EditCommandPtr &cmd)
{
    cmd.setStartingSelection(endingSelection());
    cmd.setEndingSelection(endingSelection());
    cmd.setParent(this);
    cmd.apply();
    m_cmds.append(cmd);
}

void CompositeEditCommand::applyStyle(CSSStyleDeclarationImpl *style, EditAction editingAction)
{
    EditCommandPtr cmd(new ApplyStyleCommand(document(), style, editingAction));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::applyStyle(CSSStyleDeclarationImpl *style, Position start, Position end, EditAction editingAction)
{
    EditCommandPtr cmd(new ApplyStyleCommand(document(), style, start, end, editingAction));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertParagraphSeparator()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorCommand(document()));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertNodeBefore(NodeImpl *insertChild, NodeImpl *refChild)
{
    ASSERT(!refChild->hasTagName(bodyTag));
    EditCommandPtr cmd(new InsertNodeBeforeCommand(document(), insertChild, refChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertNodeAfter(NodeImpl *insertChild, NodeImpl *refChild)
{
    ASSERT(!refChild->hasTagName(bodyTag));
    if (refChild->parentNode()->lastChild() == refChild) {
        appendNode(insertChild, refChild->parentNode());
    }
    else {
        ASSERT(refChild->nextSibling());
        insertNodeBefore(insertChild, refChild->nextSibling());
    }
}

void CompositeEditCommand::insertNodeAt(NodeImpl *insertChild, NodeImpl *refChild, int offset)
{
    if (refChild->hasChildNodes() || (refChild->renderer() && refChild->renderer()->isBlockFlow())) {
        NodeImpl *child = refChild->firstChild();
        for (int i = 0; child && i < offset; i++)
            child = child->nextSibling();
        if (child)
            insertNodeBefore(insertChild, child);
        else
            appendNode(insertChild, refChild);
    } 
    else if (refChild->caretMinOffset() >= offset) {
        insertNodeBefore(insertChild, refChild);
    } 
    else if (refChild->isTextNode() && refChild->caretMaxOffset() > offset) {
        splitTextNode(static_cast<TextImpl *>(refChild), offset);
        insertNodeBefore(insertChild, refChild);
    } 
    else {
        insertNodeAfter(insertChild, refChild);
    }
}

void CompositeEditCommand::appendNode(NodeImpl *appendChild, NodeImpl *parent)
{
    EditCommandPtr cmd(new AppendNodeCommand(document(), appendChild, parent));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeFullySelectedNode(NodeImpl *node)
{
    if (isTableStructureNode(node) || node == node->rootEditableElement()) {
        // Do not remove an element of table structure; remove its contents.
        // Likewise for the root editable element.
        NodeImpl *child = node->firstChild();
        while (child) {
            NodeImpl *remove = child;
            child = child->nextSibling();
            removeFullySelectedNode(remove);
        }
        
        // make sure empty cell has some height
        updateLayout();
        RenderObject *r = node->renderer();
        if (r && r->isTableCell() && r->contentHeight() <= 0)
            insertBlockPlaceholder(Position(node,0));
        return;
    }
    
    removeNode(node);
}

void CompositeEditCommand::removeChildrenInRange(NodeImpl *node, int from, int to)
{
    NodeImpl *nodeToRemove = node->childNode(from);
    for (int i = from; i < to; i++) {
        ASSERT(nodeToRemove);
        NodeImpl *next = nodeToRemove->nextSibling();
        removeNode(nodeToRemove);
        nodeToRemove = next;
    }
}

void CompositeEditCommand::removeNode(NodeImpl *removeChild)
{
    EditCommandPtr cmd(new RemoveNodeCommand(document(), removeChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeNodePreservingChildren(NodeImpl *removeChild)
{
    EditCommandPtr cmd(new RemoveNodePreservingChildrenCommand(document(), removeChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::splitTextNode(TextImpl *text, int offset)
{
    EditCommandPtr cmd(new SplitTextNodeCommand(document(), text, offset));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::splitElement(ElementImpl *element, NodeImpl *atChild)
{
    EditCommandPtr cmd(new SplitElementCommand(document(), element, atChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::mergeIdenticalElements(DOM::ElementImpl *first, DOM::ElementImpl *second)
{
    EditCommandPtr cmd(new MergeIdenticalElementsCommand(document(), first, second));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::wrapContentsInDummySpan(DOM::ElementImpl *element)
{
    EditCommandPtr cmd(new WrapContentsInDummySpanCommand(document(), element));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::splitTextNodeContainingElement(DOM::TextImpl *text, int offset)
{
    EditCommandPtr cmd(new SplitTextNodeContainingElementCommand(document(), text, offset));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::joinTextNodes(TextImpl *text1, TextImpl *text2)
{
    EditCommandPtr cmd(new JoinTextNodesCommand(document(), text1, text2));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::inputText(const DOMString &text, bool selectInsertedText)
{
    InsertTextCommand *impl = new InsertTextCommand(document());
    EditCommandPtr cmd(impl);
    applyCommandToComposite(cmd);
    impl->input(text, selectInsertedText);
}

void CompositeEditCommand::insertTextIntoNode(TextImpl *node, int offset, const DOMString &text)
{
    EditCommandPtr cmd(new InsertIntoTextNodeCommand(document(), node, offset, text));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::deleteTextFromNode(TextImpl *node, int offset, int count)
{
    EditCommandPtr cmd(new DeleteFromTextNodeCommand(document(), node, offset, count));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::replaceTextInNode(TextImpl *node, int offset, int count, const DOMString &replacementText)
{
    EditCommandPtr deleteCommand(new DeleteFromTextNodeCommand(document(), node, offset, count));
    applyCommandToComposite(deleteCommand);
    EditCommandPtr insertCommand(new InsertIntoTextNodeCommand(document(), node, offset, replacementText));
    applyCommandToComposite(insertCommand);
}

Position CompositeEditCommand::positionOutsideTabSpan(const Position& pos)
{
    if (!isTabSpanTextNode(pos.node()))
        return pos;
    
    NodeImpl *tabSpan = tabSpanNode(pos.node());
    
    if (pos.offset() <= pos.node()->caretMinOffset())
        return positionBeforeNode(tabSpan);
        
    if (pos.offset() >= pos.node()->caretMaxOffset())
        return positionAfterNode(tabSpan);

    splitTextNodeContainingElement(static_cast<TextImpl *>(pos.node()), pos.offset());
    return positionBeforeNode(tabSpan);
}

void CompositeEditCommand::insertNodeAtTabSpanPosition(NodeImpl *node, const Position& pos)
{
    // insert node before, after, or at split of tab span
    Position insertPos = positionOutsideTabSpan(pos);
    insertNodeAt(node, insertPos.node(), insertPos.offset());
}

void CompositeEditCommand::deleteSelection(bool smartDelete, bool mergeBlocksAfterDelete)
{
    if (endingSelection().isRange()) {
        EditCommandPtr cmd(new DeleteSelectionCommand(document(), smartDelete, mergeBlocksAfterDelete));
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommand::deleteSelection(const Selection &selection, bool smartDelete, bool mergeBlocksAfterDelete)
{
    if (selection.isRange()) {
        EditCommandPtr cmd(new DeleteSelectionCommand(document(), selection, smartDelete, mergeBlocksAfterDelete));
        applyCommandToComposite(cmd);
    }
}

void CompositeEditCommand::removeCSSProperty(CSSStyleDeclarationImpl *decl, int property)
{
    EditCommandPtr cmd(new RemoveCSSPropertyCommand(document(), decl, property));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeNodeAttribute(ElementImpl *element, const QualifiedName& attribute)
{
    DOMString value = element->getAttribute(attribute);
    if (value.isEmpty())
        return;
    EditCommandPtr cmd(new RemoveNodeAttributeCommand(document(), element, attribute));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::setNodeAttribute(ElementImpl *element, const QualifiedName& attribute, const DOMString &value)
{
    EditCommandPtr cmd(new SetNodeAttributeCommand(document(), element, attribute, value));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::rebalanceWhitespaceAt(const Position &position)
{
    EditCommandPtr cmd(new RebalanceWhitespaceCommand(document(), position));
    applyCommandToComposite(cmd);    
}

void CompositeEditCommand::rebalanceWhitespace()
{
    Selection selection = endingSelection();
    if (selection.isCaretOrRange()) {
        EditCommandPtr startCmd(new RebalanceWhitespaceCommand(document(), endingSelection().start()));
        applyCommandToComposite(startCmd);
        if (selection.isRange()) {
            EditCommandPtr endCmd(new RebalanceWhitespaceCommand(document(), endingSelection().end()));
            applyCommandToComposite(endCmd);
        }
    }
}

void CompositeEditCommand::deleteInsignificantText(TextImpl *textNode, int start, int end)
{
    if (!textNode || !textNode->renderer() || start >= end)
        return;

    RenderText *textRenderer = static_cast<RenderText *>(textNode->renderer());
    InlineTextBox *box = textRenderer->firstTextBox();
    if (!box) {
        // whole text node is empty
        removeNode(textNode);
        return;    
    }
    
    int length = textNode->length();
    if (start >= length || end > length)
        return;

    int removed = 0;
    InlineTextBox *prevBox = 0;
    RefPtr<DOMStringImpl> str;

    // This loop structure works to process all gaps preceding a box,
    // and also will look at the gap after the last box.
    while (prevBox || box) {
        int gapStart = prevBox ? prevBox->m_start + prevBox->m_len : 0;
        if (end < gapStart)
            // No more chance for any intersections
            break;

        int gapEnd = box ? box->m_start : length;
        bool indicesIntersect = start <= gapEnd && end >= gapStart;
        int gapLen = gapEnd - gapStart;
        if (indicesIntersect && gapLen > 0) {
            gapStart = kMax(gapStart, start);
            gapEnd = kMin(gapEnd, end);
            if (!str)
                str = textNode->string()->substring(start, end - start);
            // remove text in the gap
            str->remove(gapStart - start - removed, gapLen);
            removed += gapLen;
        }
        
        prevBox = box;
        if (box)
            box = box->nextTextBox();
    }

    if (str) {
        // Replace the text between start and end with our pruned version.
        if (str->l > 0) {
            replaceTextInNode(textNode, start, end - start, str.get());
        }
        else {
            // Assert that we are not going to delete all of the text in the node.
            // If we were, that should have been done above with the call to 
            // removeNode and return.
            ASSERT(start > 0 || (unsigned)end - start < textNode->length());
            deleteTextFromNode(textNode, start, end - start);
        }
    }
}

void CompositeEditCommand::deleteInsignificantText(const Position &start, const Position &end)
{
    if (start.isNull() || end.isNull())
        return;

    if (RangeImpl::compareBoundaryPoints(start, end) >= 0)
        return;

    NodeImpl *node = start.node();
    while (node) {
        NodeImpl *next = node->traverseNextNode();
    
        if (node->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(node);
            bool isStartNode = node == start.node();
            bool isEndNode = node == end.node();
            int startOffset = isStartNode ? start.offset() : 0;
            int endOffset = isEndNode ? end.offset() : textNode->length();
            deleteInsignificantText(textNode, startOffset, endOffset);
        }
            
        if (node == end.node())
            break;
        node = next;
    }
}

void CompositeEditCommand::deleteInsignificantTextDownstream(const DOM::Position &pos)
{
    Position end = VisiblePosition(pos, VP_DEFAULT_AFFINITY).next().deepEquivalent().downstream();
    deleteInsignificantText(pos, end);
}

NodeImpl *CompositeEditCommand::appendBlockPlaceholder(NodeImpl *node)
{
    if (!node)
        return NULL;
    
    // Should assert isBlockFlow || isInlineFlow when deletion improves.  See 4244964.
    ASSERT(node->renderer());

    NodeImpl *placeholder = createBlockPlaceholderElement(document());
    appendNode(placeholder, node);
    return placeholder;
}

NodeImpl *CompositeEditCommand::insertBlockPlaceholder(const Position &pos)
{
    if (pos.isNull())
        return NULL;

    // Should assert isBlockFlow || isInlineFlow when deletion improves.  See 4244964.
    ASSERT(pos.node()->renderer());

    NodeImpl *placeholder = createBlockPlaceholderElement(document());
    insertNodeAt(placeholder, pos.node(), pos.offset());
    return placeholder;
}

NodeImpl *CompositeEditCommand::addBlockPlaceholderIfNeeded(NodeImpl *node)
{
    if (!node)
        return false;

    updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return false;
    
    // append the placeholder to make sure it follows
    // any unrendered blocks
    // FIXME: use contentHeight() instead so to not be confused by bordere, e.g.
    if (renderer->height() == 0) {
        return appendBlockPlaceholder(node);
    }

    return NULL;
}

bool CompositeEditCommand::removeBlockPlaceholder(NodeImpl *node)
{
    NodeImpl *placeholder = findBlockPlaceholder(node);
    if (placeholder) {
        removeNode(placeholder);
        return true;
    }
    return false;
}

NodeImpl *CompositeEditCommand::findBlockPlaceholder(NodeImpl *node)
{
    if (!node)
        return 0;

    updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return 0;

    for (NodeImpl *checkMe = node; checkMe; checkMe = checkMe->traverseNextNode(node)) {
        if (checkMe->isElementNode()) {
            ElementImpl *element = static_cast<ElementImpl *>(checkMe);
            if (element->enclosingBlockFlowElement() == node && 
                element->getAttribute(classAttr) == blockPlaceholderClassString()) {
                return element;
            }
        }
    }
    
    return 0;
}

void CompositeEditCommand::moveParagraphContentsToNewBlockIfNecessary(const Position &pos)
{
    if (pos.isNull())
        return;
    
    updateLayout();
    
    VisiblePosition visiblePos(pos, VP_DEFAULT_AFFINITY);
    VisiblePosition visibleParagraphStart(startOfParagraph(visiblePos));
    VisiblePosition visibleParagraphEnd = endOfParagraph(visiblePos);
    VisiblePosition next = visibleParagraphEnd.next();
    VisiblePosition visibleEnd = next.isNotNull() ? next : visibleParagraphEnd;
    
    Position paragraphStart = visibleParagraphStart.deepEquivalent().upstream();
    Position end = visibleEnd.deepEquivalent().upstream();
    
    // Perform some checks to see if we need to perform work in this function.
    if (paragraphStart.node()->isBlockFlow()) {
        if (end.node()->isBlockFlow()) {
            if (!end.node()->isAncestor(paragraphStart.node())) {
                // If the paragraph end is a descendant of paragraph start, then we need to run
                // the rest of this function. If not, we can bail here.
                return;
            }
        }
        else if (end.node()->enclosingBlockFlowElement() != paragraphStart.node()) {
            // The paragraph end is in another block that is an ancestor of the paragraph start.
            // We can bail as we have a full block to work with.
            ASSERT(paragraphStart.node()->isAncestor(end.node()->enclosingBlockFlowElement()));
            return;
        }
        else if (isEndOfDocument(visibleEnd)) {
            // At the end of the document. We can bail here as well.
            return;
        }
    }

    NodeImpl *newBlock = createDefaultParagraphElement(document());

    NodeImpl *moveNode = paragraphStart.node();
    if (paragraphStart.offset() >= paragraphStart.node()->caretMaxOffset())
        moveNode = moveNode->traverseNextNode();
    NodeImpl *endNode = end.node();
    
    insertNodeAt(newBlock, paragraphStart.node(), paragraphStart.offset());

    while (moveNode && !moveNode->isBlockFlow()) {
        NodeImpl *next = moveNode->traverseNextSibling();
        removeNode(moveNode);
        appendNode(moveNode, newBlock);
        if (moveNode == endNode)
            break;
        moveNode = next;
    }
}

ElementImpl *createBlockPlaceholderElement(DocumentImpl *document)
{
    int exceptionCode = 0;
    ElementImpl *breakNode = document->createElementNS(xhtmlNamespaceURI, "br", exceptionCode);
    ASSERT(exceptionCode == 0);
    breakNode->setAttribute(classAttr, blockPlaceholderClassString());
    return breakNode;
}

static const DOMString &blockPlaceholderClassString()
{
    static DOMString blockPlaceholderClassString = "khtml-block-placeholder";
    return blockPlaceholderClassString;
}

} // namespace khtml
