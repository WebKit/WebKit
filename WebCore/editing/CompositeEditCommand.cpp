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
#include "CompositeEditCommand.h"

#include "AppendNodeCommand.h"
#include "ApplyStyleCommand.h"
#include "DeleteFromTextNodeCommand.h"
#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Element.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "InsertIntoTextNodeCommand.h"
#include "InsertNodeBeforeCommand.h"
#include "InsertParagraphSeparatorCommand.h"
#include "InsertTextCommand.h"
#include "JoinTextNodesCommand.h"
#include "markup.h"
#include "MergeIdenticalElementsCommand.h"
#include "Range.h"
#include "RebalanceWhitespaceCommand.h"
#include "RemoveCSSPropertyCommand.h"
#include "RemoveNodeAttributeCommand.h"
#include "RemoveNodeCommand.h"
#include "RemoveNodePreservingChildrenCommand.h"
#include "ReplaceSelectionCommand.h"
#include "SetNodeAttributeCommand.h"
#include "SplitElementCommand.h"
#include "SplitTextNodeCommand.h"
#include "SplitTextNodeContainingElementCommand.h"
#include "TextIterator.h"
#include "WrapContentsInDummySpanCommand.h"
#include "htmlediting.h"
#include "visible_units.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static const String &blockPlaceholderClassString();

CompositeEditCommand::CompositeEditCommand(Document *document) 
    : EditCommand(document)
{
}

void CompositeEditCommand::doUnapply()
{
    if (m_cmds.count() == 0)
        return;
    
    DeprecatedValueList<EditCommandPtr>::ConstIterator end;
    for (DeprecatedValueList<EditCommandPtr>::ConstIterator it = m_cmds.fromLast(); it != end; --it)
        (*it)->unapply();

    setState(NotApplied);
}

void CompositeEditCommand::doReapply()
{
    if (m_cmds.count() == 0)
        return;

    for (DeprecatedValueList<EditCommandPtr>::ConstIterator it = m_cmds.begin(); it != m_cmds.end(); ++it)
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

void CompositeEditCommand::applyStyle(CSSStyleDeclaration *style, EditAction editingAction)
{
    EditCommandPtr cmd(new ApplyStyleCommand(document(), style, editingAction));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::applyStyle(CSSStyleDeclaration *style, Position start, Position end, EditAction editingAction)
{
    EditCommandPtr cmd(new ApplyStyleCommand(document(), style, start, end, editingAction));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::applyStyledElement(Element* element)
{
    EditCommandPtr cmd(new ApplyStyleCommand(document(), element, false));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeStyledElement(Element* element)
{
    EditCommandPtr cmd(new ApplyStyleCommand(document(), element, true));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertParagraphSeparator()
{
    EditCommandPtr cmd(new InsertParagraphSeparatorCommand(document()));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertNodeBefore(Node *insertChild, Node *refChild)
{
    ASSERT(!refChild->hasTagName(bodyTag));
    EditCommandPtr cmd(new InsertNodeBeforeCommand(document(), insertChild, refChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::insertNodeAfter(Node *insertChild, Node *refChild)
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

void CompositeEditCommand::insertNodeAt(Node *insertChild, Node *refChild, int offset)
{
    if (canHaveChildrenForEditing(refChild)) {
        Node *child = refChild->firstChild();
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
        splitTextNode(static_cast<Text *>(refChild), offset);
        insertNodeBefore(insertChild, refChild);
    } 
    else {
        insertNodeAfter(insertChild, refChild);
    }
}

void CompositeEditCommand::appendNode(Node *appendChild, Node *parent)
{
    ASSERT(canHaveChildrenForEditing(parent));
    EditCommandPtr cmd(new AppendNodeCommand(document(), appendChild, parent));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeChildrenInRange(Node *node, int from, int to)
{
    Node *nodeToRemove = node->childNode(from);
    for (int i = from; i < to; i++) {
        ASSERT(nodeToRemove);
        Node *next = nodeToRemove->nextSibling();
        removeNode(nodeToRemove);
        nodeToRemove = next;
    }
}

void CompositeEditCommand::removeNode(Node *removeChild)
{
    EditCommandPtr cmd(new RemoveNodeCommand(document(), removeChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeNodePreservingChildren(Node *removeChild)
{
    EditCommandPtr cmd(new RemoveNodePreservingChildrenCommand(document(), removeChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeNodeAndPruneAncestors(Node* node)
{
    RefPtr<Node> parent = node->parentNode();
    removeNode(node);
    prune(parent);
}

bool hasARenderedDescendant(Node* node)
{
    Node* n = node->traverseNextNode(node);
    while (n) {
        if (n->renderer())
            return true;
        n = n->traverseNextNode(node);
    }
    
    return false;
}

void CompositeEditCommand::prune(PassRefPtr<Node> node)
{
    while (node) {
        // If you change this rule you may have to add an updateLayout() here.
        RenderObject* renderer = node->renderer();
        if (renderer && (!renderer->canHaveChildren() || hasARenderedDescendant(node.get()) || node->rootEditableElement() == node))
            return;
            
        RefPtr<Node> next = node->parentNode();
        
        if (renderer) {
            RenderObject* p = renderer->parent();
            while (p && !p->element())
                p = p->parent();
            ASSERT(p);
            if (!p)
                return;
            next = p->element();        
        }

        removeNode(node.get());
        node = next;
    }
}

void CompositeEditCommand::splitTextNode(Text *text, int offset)
{
    EditCommandPtr cmd(new SplitTextNodeCommand(document(), text, offset));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::splitElement(Element *element, Node *atChild)
{
    EditCommandPtr cmd(new SplitElementCommand(document(), element, atChild));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::mergeIdenticalElements(WebCore::Element *first, WebCore::Element *second)
{
    ASSERT(!first->isAncestor(second) && second != first);
    if (first->nextSibling() != second) {
        removeNode(second);
        insertNodeAfter(second, first);
    }
    EditCommandPtr cmd(new MergeIdenticalElementsCommand(document(), first, second));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::wrapContentsInDummySpan(WebCore::Element *element)
{
    EditCommandPtr cmd(new WrapContentsInDummySpanCommand(document(), element));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::splitTextNodeContainingElement(WebCore::Text *text, int offset)
{
    EditCommandPtr cmd(new SplitTextNodeContainingElementCommand(document(), text, offset));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::joinTextNodes(Text *text1, Text *text2)
{
    EditCommandPtr cmd(new JoinTextNodesCommand(document(), text1, text2));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::inputText(const String &text, bool selectInsertedText)
{
    InsertTextCommand *impl = new InsertTextCommand(document());
    EditCommandPtr cmd(impl);
    applyCommandToComposite(cmd);
    impl->input(text, selectInsertedText);
}

void CompositeEditCommand::insertTextIntoNode(Text *node, int offset, const String &text)
{
    EditCommandPtr cmd(new InsertIntoTextNodeCommand(document(), node, offset, text));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::deleteTextFromNode(Text *node, int offset, int count)
{
    EditCommandPtr cmd(new DeleteFromTextNodeCommand(document(), node, offset, count));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::replaceTextInNode(Text *node, int offset, int count, const String &replacementText)
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
    
    Node *tabSpan = tabSpanNode(pos.node());
    
    if (pos.offset() <= pos.node()->caretMinOffset())
        return positionBeforeNode(tabSpan);
        
    if (pos.offset() >= pos.node()->caretMaxOffset())
        return positionAfterNode(tabSpan);

    splitTextNodeContainingElement(static_cast<Text *>(pos.node()), pos.offset());
    return positionBeforeNode(tabSpan);
}

void CompositeEditCommand::insertNodeAtTabSpanPosition(Node *node, const Position& pos)
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

void CompositeEditCommand::removeCSSProperty(CSSStyleDeclaration *decl, int property)
{
    EditCommandPtr cmd(new RemoveCSSPropertyCommand(document(), decl, property));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::removeNodeAttribute(Element *element, const QualifiedName& attribute)
{
    String value = element->getAttribute(attribute);
    if (value.isEmpty())
        return;
    EditCommandPtr cmd(new RemoveNodeAttributeCommand(document(), element, attribute));
    applyCommandToComposite(cmd);
}

void CompositeEditCommand::setNodeAttribute(Element *element, const QualifiedName& attribute, const String &value)
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

void CompositeEditCommand::deleteInsignificantText(Text *textNode, int start, int end)
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
    RefPtr<StringImpl> str;

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
            gapStart = max(gapStart, start);
            gapEnd = min(gapEnd, end);
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
        if (str->length() > 0) {
            replaceTextInNode(textNode, start, end - start, str.get());
        } else {
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

    if (Range::compareBoundaryPoints(start, end) >= 0)
        return;

    Node *node = start.node();
    while (node) {
        Node *next = node->traverseNextNode();
    
        if (node->isTextNode()) {
            Text *textNode = static_cast<Text *>(node);
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

void CompositeEditCommand::deleteInsignificantTextDownstream(const WebCore::Position &pos)
{
    Position end = VisiblePosition(pos, VP_DEFAULT_AFFINITY).next().deepEquivalent().downstream();
    deleteInsignificantText(pos, end);
}

Node *CompositeEditCommand::appendBlockPlaceholder(Node *node)
{
    if (!node)
        return NULL;
    
    // Should assert isBlockFlow || isInlineFlow when deletion improves.  See 4244964.
    ASSERT(node->renderer());

    RefPtr<Node> placeholder = createBlockPlaceholderElement(document());
    appendNode(placeholder.get(), node);
    return placeholder.get();
}

Node *CompositeEditCommand::insertBlockPlaceholder(const Position &pos)
{
    if (pos.isNull())
        return NULL;

    // Should assert isBlockFlow || isInlineFlow when deletion improves.  See 4244964.
    ASSERT(pos.node()->renderer());

    RefPtr<Node> placeholder = createBlockPlaceholderElement(document());
    insertNodeAt(placeholder.get(), pos.node(), pos.offset());
    return placeholder.get();
}

Node *CompositeEditCommand::addBlockPlaceholderIfNeeded(Node *node)
{
    if (!node)
        return false;

    updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return false;
    
    // append the placeholder to make sure it follows
    // any unrendered blocks
    if (renderer->height() == 0 || (renderer->isListItem() && renderer->isEmpty()))
        return appendBlockPlaceholder(node);

    return NULL;
}

void CompositeEditCommand::removeBlockPlaceholder(const VisiblePosition& visiblePosition)
{
    Position p = visiblePosition.deepEquivalent().downstream();
    if (p.node()->hasTagName(brTag) && p.offset() == 0 && isEndOfBlock(visiblePosition) && isStartOfBlock(visiblePosition))
        removeNode(p.node());
        
    return;
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

    RefPtr<Node> newBlock = createDefaultParagraphElement(document());

    Node *moveNode = paragraphStart.node();
    if (paragraphStart.offset() >= paragraphStart.node()->caretMaxOffset())
        moveNode = moveNode->traverseNextNode();
    Node *endNode = end.node();
    
    insertNodeAt(newBlock.get(), paragraphStart.node(), paragraphStart.offset());

    while (moveNode && !moveNode->isBlockFlow()) {
        Node *next = moveNode->traverseNextSibling();
        removeNode(moveNode);
        appendNode(moveNode, newBlock.get());
        if (moveNode == endNode)
            break;
        moveNode = next;
    }
}

Node* enclosingAnchorElement(Node* node)
{
    while (node && !(node->isElementNode() && node->isLink()))
        node = node->parentNode();
    
    return node;
}

void CompositeEditCommand::pushAnchorElementDown(Node* anchorNode)
{
    if (!anchorNode)
        return;
    
    ASSERT(anchorNode->isLink());
    
    setEndingSelection(Selection::selectionFromContentsOfNode(anchorNode));
    applyStyledElement(static_cast<Element*>(anchorNode));
}

// We must push partially selected anchors down before creating or removing
// links from a selection to create fully selected chunks that can be removed.
// ApplyStyleCommand doesn't do this for us because styles can be nested.
// Anchors cannot be nested.
void CompositeEditCommand::pushPartiallySelectedAnchorElementsDown()
{
    Selection originalSelection = endingSelection();
    VisiblePosition visibleStart(originalSelection.start());
    VisiblePosition visibleEnd(originalSelection.end());
    
    Node* startAnchor = enclosingAnchorElement(originalSelection.start().node());
    VisiblePosition startOfStartAnchor(Position(startAnchor, 0));
    if (startAnchor && startOfStartAnchor != visibleStart)
        pushAnchorElementDown(startAnchor);

    Node* endAnchor = enclosingAnchorElement(originalSelection.end().node());
    VisiblePosition endOfEndAnchor(Position(endAnchor, 0));
    if (endAnchor && endOfEndAnchor != visibleEnd)
        pushAnchorElementDown(endAnchor);

    ASSERT(originalSelection.start().node()->inDocument() && originalSelection.end().node()->inDocument());
    setEndingSelection(originalSelection);
}

// This moves a paragraph preserving its style.
void CompositeEditCommand::moveParagraph(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, const VisiblePosition& destination, bool preserveSelection, bool preserveStyle)
{
    ASSERT(isStartOfParagraph(startOfParagraphToMove));
    ASSERT(isEndOfParagraph(endOfParagraphToMove));
    moveParagraphs(startOfParagraphToMove, endOfParagraphToMove, destination, preserveSelection, preserveStyle);
}

void CompositeEditCommand::moveParagraphs(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, const VisiblePosition& destination, bool preserveSelection, bool preserveStyle)
{
    if (startOfParagraphToMove == destination)
        return;
    
    int startIndex = -1;
    int endIndex = -1;
    int destinationIndex = -1;
    if (preserveSelection && !endingSelection().isNone()) {
        VisiblePosition visibleStart = endingSelection().visibleStart();
        VisiblePosition visibleEnd = endingSelection().visibleEnd();
        
        bool startAfterParagraph = Range::compareBoundaryPoints(visibleStart.deepEquivalent(), endOfParagraphToMove.deepEquivalent()) > 0;
        bool endBeforeParagraph = Range::compareBoundaryPoints(visibleEnd.deepEquivalent(), startOfParagraphToMove.deepEquivalent()) < 0;
        
        if (!startAfterParagraph && !endBeforeParagraph) {
            bool startInParagraph = Range::compareBoundaryPoints(visibleStart.deepEquivalent(), startOfParagraphToMove.deepEquivalent()) >= 0;
            bool endInParagraph = Range::compareBoundaryPoints(visibleEnd.deepEquivalent(), endOfParagraphToMove.deepEquivalent()) <= 0;
            
            startIndex = startInParagraph ? TextIterator::rangeLength(new Range(document(), startOfParagraphToMove.deepEquivalent(), visibleStart.deepEquivalent())) : 0;
            endIndex = endInParagraph ? TextIterator::rangeLength(new Range(document(), startOfParagraphToMove.deepEquivalent(), visibleEnd.deepEquivalent())) : 0;
        }
    }
    
    VisiblePosition beforeParagraph = startOfParagraphToMove.previous();

    // We upstream() the end and downstream() the start so that we don't include collapsed whitespace in the move.
    // When we paste a fragment, spaces after the end and before the start are treated as though they were rendered.    
    Position start = startOfParagraphToMove.deepEquivalent().downstream();
    Position end = endOfParagraphToMove.deepEquivalent().upstream();
    RefPtr<Range> range = new Range(document(), start.node(), start.offset(), end.node(), end.offset());

    // FIXME: This is an inefficient way to preserve style on nodes in the paragraph to move.  It 
    // shouldn't matter though, since moved paragraphs will usually be quite small.
    RefPtr<DocumentFragment> fragment = startOfParagraphToMove != endOfParagraphToMove ? createFragmentFromMarkup(document(), range->toHTML(), "") : 0;
    
    setEndingSelection(Selection(start, end, DOWNSTREAM));
    deleteSelection(false, false);
    
    ASSERT(destination.deepEquivalent().node()->inDocument());
    
    // Deleting a paragraph leaves a placeholder (it always does when a whole paragraph is deleted).
    // We remove it and prune its parents since we want to remove all traces of the paragraph we're moving.
    Node* placeholder = endingSelection().end().node();
    if (placeholder->hasTagName(brTag))
        removeNodeAndPruneAncestors(placeholder);
    // FIXME: Deletion has bugs and it doesn't always add a placeholder.  If it fails, still do pruning.
    else
        prune(placeholder);

    // Add a br if pruning an empty block level element caused a collapse.  For example:
    // foo^
    // <div>bar</div>
    // baz
    // Imagine moving 'bar' to ^.  'bar' will be deleted and its div pruned.  That would
    // cause 'baz' to collapse onto the line with 'foobar' unless we insert a br.
    if (beforeParagraph.isNotNull() && !isEndOfParagraph(beforeParagraph))
        insertNodeAt(createBreakElement(document()).get(), beforeParagraph.deepEquivalent().node(), beforeParagraph.deepEquivalent().offset());
        
    destinationIndex = TextIterator::rangeLength(new Range(document(), Position(document(), 0), destination.deepEquivalent()));
    
    setEndingSelection(destination);
    EditCommandPtr cmd(new ReplaceSelectionCommand(document(), fragment.get(), true, false, !preserveStyle, true));
    applyCommandToComposite(cmd);
    
    if (preserveSelection && startIndex != -1) {
        setEndingSelection(Selection(TextIterator::rangeFromLocationAndLength(document(), destinationIndex + startIndex, 0)->startPosition(), 
                                     TextIterator::rangeFromLocationAndLength(document(), destinationIndex + endIndex, 0)->startPosition(), 
                                     DOWNSTREAM));
    }
}

PassRefPtr<Element> createBlockPlaceholderElement(Document* document)
{
    ExceptionCode ec = 0;
    RefPtr<Element> breakNode = document->createElementNS(xhtmlNamespaceURI, "br", ec);
    ASSERT(ec == 0);
    breakNode->setAttribute(classAttr, blockPlaceholderClassString());
    return breakNode.release();
}

static const String &blockPlaceholderClassString()
{
    static String blockPlaceholderClassString = "webkit-block-placeholder";
    return blockPlaceholderClassString;
}

} // namespace WebCore
