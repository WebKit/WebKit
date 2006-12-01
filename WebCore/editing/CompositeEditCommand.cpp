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
#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
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

CompositeEditCommand::CompositeEditCommand(Document *document) 
    : EditCommand(document)
{
}

void CompositeEditCommand::doUnapply()
{
    size_t size = m_commands.size();
    for (size_t i = size; i != 0; --i)
        m_commands[i - 1]->unapply();
}

void CompositeEditCommand::doReapply()
{
    size_t size = m_commands.size();
    for (size_t i = 0; i != size; ++i)
        m_commands[i]->reapply();
}

//
// sugary-sweet convenience functions to help create and apply edit commands in composite commands
//
void CompositeEditCommand::applyCommandToComposite(PassRefPtr<EditCommand> cmd)
{
    cmd->setParent(this);
    cmd->apply();
    m_commands.append(cmd);
}

void CompositeEditCommand::applyStyle(CSSStyleDeclaration* style, EditAction editingAction)
{
    applyCommandToComposite(new ApplyStyleCommand(document(), style, editingAction));
}

void CompositeEditCommand::applyStyle(CSSStyleDeclaration* style, const Position& start, const Position& end, EditAction editingAction)
{
    applyCommandToComposite(new ApplyStyleCommand(document(), style, start, end, editingAction));
}

void CompositeEditCommand::applyStyledElement(Element* element)
{
    applyCommandToComposite(new ApplyStyleCommand(element, false));
}

void CompositeEditCommand::removeStyledElement(Element* element)
{
    applyCommandToComposite(new ApplyStyleCommand(element, true));
}

void CompositeEditCommand::insertParagraphSeparator()
{
    applyCommandToComposite(new InsertParagraphSeparatorCommand(document()));
}

void CompositeEditCommand::insertNodeBefore(Node* insertChild, Node* refChild)
{
    ASSERT(!refChild->hasTagName(bodyTag));
    applyCommandToComposite(new InsertNodeBeforeCommand(insertChild, refChild));
}

void CompositeEditCommand::insertNodeAfter(Node* insertChild, Node* refChild)
{
    ASSERT(!refChild->hasTagName(bodyTag));
    if (refChild->parentNode()->lastChild() == refChild)
        appendNode(insertChild, refChild->parentNode());
    else {
        ASSERT(refChild->nextSibling());
        insertNodeBefore(insertChild, refChild->nextSibling());
    }
}

void CompositeEditCommand::insertNodeAt(Node* insertChild, Node* refChild, int offset)
{
    if (canHaveChildrenForEditing(refChild)) {
        Node* child = refChild->firstChild();
        for (int i = 0; child && i < offset; i++)
            child = child->nextSibling();
        if (child)
            insertNodeBefore(insertChild, child);
        else
            appendNode(insertChild, refChild);
    } else if (refChild->caretMinOffset() >= offset) {
        insertNodeBefore(insertChild, refChild);
    } else if (refChild->isTextNode() && refChild->caretMaxOffset() > offset) {
        splitTextNode(static_cast<Text *>(refChild), offset);
        insertNodeBefore(insertChild, refChild);
    } else {
        insertNodeAfter(insertChild, refChild);
    }
}

void CompositeEditCommand::appendNode(Node* newChild, Node* parent)
{
    ASSERT(canHaveChildrenForEditing(parent));
    applyCommandToComposite(new AppendNodeCommand(parent, newChild));
}

void CompositeEditCommand::removeChildrenInRange(Node* node, int from, int to)
{
    Node* nodeToRemove = node->childNode(from);
    for (int i = from; i < to; i++) {
        ASSERT(nodeToRemove);
        Node* next = nodeToRemove->nextSibling();
        removeNode(nodeToRemove);
        nodeToRemove = next;
    }
}

void CompositeEditCommand::removeNode(Node* removeChild)
{
    applyCommandToComposite(new RemoveNodeCommand(removeChild));
}

void CompositeEditCommand::removeNodePreservingChildren(Node* removeChild)
{
    applyCommandToComposite(new RemoveNodePreservingChildrenCommand(removeChild));
}

void CompositeEditCommand::removeNodeAndPruneAncestors(Node* node)
{
    RefPtr<Node> parent = node->parentNode();
    removeNode(node);
    prune(parent);
}

bool hasARenderedDescendant(Node* node)
{
    Node* n = node->firstChild();
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
        removeNode(node.get());
        node = next;
    }
}

void CompositeEditCommand::splitTextNode(Text *text, int offset)
{
    applyCommandToComposite(new SplitTextNodeCommand(text, offset));
}

void CompositeEditCommand::splitElement(Element* element, Node* atChild)
{
    applyCommandToComposite(new SplitElementCommand(element, atChild));
}

void CompositeEditCommand::mergeIdenticalElements(Element* first, Element* second)
{
    ASSERT(!first->isDescendantOf(second) && second != first);
    if (first->nextSibling() != second) {
        removeNode(second);
        insertNodeAfter(second, first);
    }
    applyCommandToComposite(new MergeIdenticalElementsCommand(first, second));
}

void CompositeEditCommand::wrapContentsInDummySpan(Element* element)
{
    applyCommandToComposite(new WrapContentsInDummySpanCommand(element));
}

void CompositeEditCommand::splitTextNodeContainingElement(Text *text, int offset)
{
    applyCommandToComposite(new SplitTextNodeContainingElementCommand(text, offset));
}

void CompositeEditCommand::joinTextNodes(Text *text1, Text *text2)
{
    applyCommandToComposite(new JoinTextNodesCommand(text1, text2));
}

void CompositeEditCommand::inputText(const String &text, bool selectInsertedText)
{
    RefPtr<InsertTextCommand> command = new InsertTextCommand(document());
    applyCommandToComposite(command);
    command->input(text, selectInsertedText);
}

void CompositeEditCommand::insertTextIntoNode(Text *node, int offset, const String &text)
{
    applyCommandToComposite(new InsertIntoTextNodeCommand(node, offset, text));
}

void CompositeEditCommand::deleteTextFromNode(Text *node, int offset, int count)
{
    applyCommandToComposite(new DeleteFromTextNodeCommand(node, offset, count));
}

void CompositeEditCommand::replaceTextInNode(Text *node, int offset, int count, const String &replacementText)
{
    applyCommandToComposite(new DeleteFromTextNodeCommand(node, offset, count));
    applyCommandToComposite(new InsertIntoTextNodeCommand(node, offset, replacementText));
}

Position CompositeEditCommand::positionOutsideTabSpan(const Position& pos)
{
    if (!isTabSpanTextNode(pos.node()))
        return pos;
    
    Node* tabSpan = tabSpanNode(pos.node());
    
    if (pos.offset() <= pos.node()->caretMinOffset())
        return positionBeforeNode(tabSpan);
        
    if (pos.offset() >= pos.node()->caretMaxOffset())
        return positionAfterNode(tabSpan);

    splitTextNodeContainingElement(static_cast<Text *>(pos.node()), pos.offset());
    return positionBeforeNode(tabSpan);
}

void CompositeEditCommand::insertNodeAtTabSpanPosition(Node* node, const Position& pos)
{
    // insert node before, after, or at split of tab span
    Position insertPos = positionOutsideTabSpan(pos);
    insertNodeAt(node, insertPos.node(), insertPos.offset());
}

void CompositeEditCommand::deleteSelection(bool smartDelete, bool mergeBlocksAfterDelete, bool replace)
{
    if (endingSelection().isRange())
        applyCommandToComposite(new DeleteSelectionCommand(document(), smartDelete, mergeBlocksAfterDelete, replace));
}

void CompositeEditCommand::deleteSelection(const Selection &selection, bool smartDelete, bool mergeBlocksAfterDelete, bool replace)
{
    if (selection.isRange())
        applyCommandToComposite(new DeleteSelectionCommand(selection, smartDelete, mergeBlocksAfterDelete, replace));
}

void CompositeEditCommand::removeCSSProperty(CSSStyleDeclaration *decl, int property)
{
    applyCommandToComposite(new RemoveCSSPropertyCommand(document(), decl, property));
}

void CompositeEditCommand::removeNodeAttribute(Element* element, const QualifiedName& attribute)
{
    if (element->getAttribute(attribute).isNull())
        return;
    applyCommandToComposite(new RemoveNodeAttributeCommand(element, attribute));
}

void CompositeEditCommand::setNodeAttribute(Element* element, const QualifiedName& attribute, const String &value)
{
    applyCommandToComposite(new SetNodeAttributeCommand(element, attribute, value));
}

void CompositeEditCommand::rebalanceWhitespaceAt(const Position& position)
{
    Node* textNode = position.node();
    if (!textNode || !textNode->isTextNode())
        return;
    if (static_cast<Text*>(textNode)->length() == 0)
        return;
    RenderObject* renderer = textNode->renderer();
    if (renderer && !renderer->style()->collapseWhiteSpace())
        return;
    applyCommandToComposite(new RebalanceWhitespaceCommand(position));    
}

void CompositeEditCommand::rebalanceWhitespace()
{
    Selection selection = endingSelection();
    if (selection.isCaretOrRange()) {
        rebalanceWhitespaceAt(endingSelection().start());
        if (selection.isRange())
            rebalanceWhitespaceAt(endingSelection().end());
    }
}

void CompositeEditCommand::deleteInsignificantText(Text* textNode, int start, int end)
{
    if (!textNode || !textNode->renderer() || start >= end)
        return;

    RenderText* textRenderer = static_cast<RenderText*>(textNode->renderer());
    InlineTextBox* box = textRenderer->firstTextBox();
    if (!box) {
        // whole text node is empty
        removeNode(textNode);
        return;    
    }
    
    int length = textNode->length();
    if (start >= length || end > length)
        return;

    int removed = 0;
    InlineTextBox* prevBox = 0;
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

void CompositeEditCommand::deleteInsignificantText(const Position& start, const Position& end)
{
    if (start.isNull() || end.isNull())
        return;

    if (Range::compareBoundaryPoints(start, end) >= 0)
        return;

    Node* next;
    for (Node* node = start.node(); node; node = next) {
        next = node->traverseNextNode();
        if (node->isTextNode()) {
            Text* textNode = static_cast<Text*>(node);
            int startOffset = node == start.node() ? start.offset() : 0;
            int endOffset = node == end.node() ? end.offset() : textNode->length();
            deleteInsignificantText(textNode, startOffset, endOffset);
        }
        if (node == end.node())
            break;
    }
}

void CompositeEditCommand::deleteInsignificantTextDownstream(const Position& pos)
{
    Position end = VisiblePosition(pos, VP_DEFAULT_AFFINITY).next().deepEquivalent().downstream();
    deleteInsignificantText(pos, end);
}

Node* CompositeEditCommand::appendBlockPlaceholder(Node* node)
{
    if (!node)
        return 0;
    
    // Should assert isBlockFlow || isInlineFlow when deletion improves.  See 4244964.
    ASSERT(node->renderer());

    RefPtr<Node> placeholder = createBlockPlaceholderElement(document());
    appendNode(placeholder.get(), node);
    return placeholder.get();
}

Node* CompositeEditCommand::insertBlockPlaceholder(const Position& pos)
{
    if (pos.isNull())
        return 0;

    // Should assert isBlockFlow || isInlineFlow when deletion improves.  See 4244964.
    ASSERT(pos.node()->renderer());

    RefPtr<Node> placeholder = createBlockPlaceholderElement(document());
    insertNodeAt(placeholder.get(), pos.node(), pos.offset());
    return placeholder.get();
}

Node* CompositeEditCommand::addBlockPlaceholderIfNeeded(Node* node)
{
    if (!node)
        return 0;

    updateLayout();

    RenderObject *renderer = node->renderer();
    if (!renderer || !renderer->isBlockFlow())
        return 0;
    
    // append the placeholder to make sure it follows
    // any unrendered blocks
    if (renderer->height() == 0 || (renderer->isListItem() && renderer->isEmpty()))
        return appendBlockPlaceholder(node);

    return 0;
}

// Removes '\n's and brs that will collapse when content is inserted just before them.
// FIXME: We shouldn't really have to remove placeholders, but removing them is a workaround for 9661.
void CompositeEditCommand::removePlaceholderAt(const VisiblePosition& visiblePosition)
{
    if (visiblePosition.isNull())
        return;
        
    Position p = visiblePosition.deepEquivalent().downstream();
    // If a br or '\n' is at the end of a block and not at the start of a paragraph,
    // then it is superfluous, so adding content before a br or '\n' that is at
    // the start of a paragraph will render it superfluous.
    // FIXME: This doesn't remove placeholders at the end of anonymous blocks.
    if (isEndOfBlock(visiblePosition) && isStartOfParagraph(visiblePosition)) {
        if (p.node()->hasTagName(brTag) && p.offset() == 0)
            removeNode(p.node());
        else if (p.node()->renderer()->style()->preserveNewline() && visiblePosition.characterAfter() == '\n')
            deleteTextFromNode(static_cast<Text*>(p.node()), p.offset(), 1);
    }
}

void CompositeEditCommand::moveParagraphContentsToNewBlockIfNecessary(const Position& pos)
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

    // If there are no VisiblePositions in the same block as pos then 
    // paragraphStart will be outside the paragraph
    if (Range::compareBoundaryPoints(pos, paragraphStart) < 0)
        return;

    // Perform some checks to see if we need to perform work in this function.
    if (isBlock(paragraphStart.node())) {
        if (isBlock(end.node())) {
            if (!end.node()->isDescendantOf(paragraphStart.node())) {
                // If the paragraph end is a descendant of paragraph start, then we need to run
                // the rest of this function. If not, we can bail here.
                return;
            }
        }
        else if (enclosingBlock(end.node()) != paragraphStart.node()) {
            // The visibleEnd.  It must be an ancestor of the paragraph start.
            // We can bail as we have a full block to work with.
            ASSERT(paragraphStart.node()->isDescendantOf(enclosingBlock(end.node())));
            return;
        }
        else if (isEndOfDocument(visibleEnd)) {
            // At the end of the document. We can bail here as well.
            return;
        }
    }

    RefPtr<Node> newBlock = createDefaultParagraphElement(document());

    Node* moveNode = paragraphStart.node();
    if (paragraphStart.offset() >= paragraphStart.node()->caretMaxOffset())
        moveNode = moveNode->traverseNextNode();
    Node* endNode = end.node();
    
    if (!isAtomicNode(paragraphStart.node()))
        insertNodeAt(newBlock.get(), paragraphStart.node(), paragraphStart.offset());
    else {
        ASSERT(paragraphStart.offset() <= 1);
        ASSERT(paragraphStart.node()->parentNode());
        insertNodeAt(newBlock.get(), paragraphStart.node()->parentNode(), paragraphStart.node()->nodeIndex() + paragraphStart.offset());
    }

    while (moveNode && !isBlock(moveNode)) {
        Node* next = moveNode->traverseNextSibling();
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
            
            startIndex = 0;
            if (startInParagraph) {
                RefPtr<Range> startRange = new Range(document(), startOfParagraphToMove.deepEquivalent(), visibleStart.deepEquivalent());
                startIndex = TextIterator::rangeLength(startRange.get());
            }

            endIndex = 0;
            if (endInParagraph) {
                RefPtr<Range> endRange = new Range(document(), startOfParagraphToMove.deepEquivalent(), visibleEnd.deepEquivalent());
                endIndex = TextIterator::rangeLength(endRange.get());
            }
        }
    }
    
    VisiblePosition beforeParagraph = startOfParagraphToMove.previous();

    // We upstream() the end and downstream() the start so that we don't include collapsed whitespace in the move.
    // When we paste a fragment, spaces after the end and before the start are treated as though they were rendered.    
    Position start = startOfParagraphToMove.deepEquivalent().downstream();
    Position end = endOfParagraphToMove.deepEquivalent().upstream();
    
    // start and end can't be used directly to create a Range; they are "editing positions"
    Position startRangeCompliant = rangeCompliantEquivalent(start);
    Position endRangeCompliant = rangeCompliantEquivalent(end);
    RefPtr<Range> range = new Range(document(), startRangeCompliant.node(), startRangeCompliant.offset(), endRangeCompliant.node(), endRangeCompliant.offset());

    // FIXME: This is an inefficient way to preserve style on nodes in the paragraph to move.  It 
    // shouldn't matter though, since moved paragraphs will usually be quite small.
    RefPtr<DocumentFragment> fragment = startOfParagraphToMove != endOfParagraphToMove ? createFragmentFromMarkup(document(), range->toHTML(), "") : 0;
    
    setEndingSelection(Selection(start, end, DOWNSTREAM));
    deleteSelection(false, false);

    ASSERT(destination.deepEquivalent().node()->inDocument());
    
    // There are bugs in deletion when it removes a fully selected table/list.  
    // It expands and removes the entire table/list, but will let content
    // before and after the table/list collapse onto one line.
    
    // Deleting a paragraph will leave a placeholder.  Remove it (and prune
    // empty or unrendered parents).
    VisiblePosition caretAfterDelete = endingSelection().visibleStart();
    if (isStartOfParagraph(caretAfterDelete) && isEndOfParagraph(caretAfterDelete)) {
        // Note: We want the rightmost candidate.
        Position position = caretAfterDelete.deepEquivalent().downstream();
        Node* node = position.node();
        // Normally deletion will leave a br as a placeholder.
        if (node->hasTagName(brTag))
            removeNodeAndPruneAncestors(node);
        // If the selection to move was empty and in an empty block that 
        // doesn't require a placeholder to prop itself open (like a bordered 
        // div or an li), remove it during the move (the list removal code 
        // expects this behavior).
        else if (isBlock(node))
            removeNodeAndPruneAncestors(node);
        else if (node->renderer() && node->renderer()->style()->preserveNewline() && caretAfterDelete.characterAfter() == '\n')
            deleteTextFromNode(static_cast<Text*>(node), position.offset(), 1);
    }

    // Add a br if pruning an empty block level element caused a collapse.  For example:
    // foo^
    // <div>bar</div>
    // baz
    // Imagine moving 'bar' to ^.  'bar' will be deleted and its div pruned.  That would
    // cause 'baz' to collapse onto the line with 'foobar' unless we insert a br.
    if (beforeParagraph.isNotNull() && !isEndOfParagraph(beforeParagraph))
        insertNodeAt(createBreakElement(document()).get(), beforeParagraph.deepEquivalent().node(), beforeParagraph.deepEquivalent().offset());
        
    RefPtr<Range> startToDestinationRange(new Range(document(), Position(document(), 0), rangeCompliantEquivalent(destination.deepEquivalent())));
    destinationIndex = TextIterator::rangeLength(startToDestinationRange.get());
    
    setEndingSelection(destination);
    applyCommandToComposite(new ReplaceSelectionCommand(document(), fragment.get(), true, false, !preserveStyle, false));
    
    if (preserveSelection && startIndex != -1) {
        setEndingSelection(Selection(TextIterator::rangeFromLocationAndLength(document()->documentElement(), destinationIndex + startIndex, 0)->startPosition(), 
                                     TextIterator::rangeFromLocationAndLength(document()->documentElement(), destinationIndex + endIndex, 0)->startPosition(), 
                                     DOWNSTREAM));
    }
}

// FIXME: Send an appropriate shouldDeleteRange call.
bool CompositeEditCommand::breakOutOfEmptyListItem()
{
    Node* emptyListItem = enclosingEmptyListItem(endingSelection().visibleStart());
    if (!emptyListItem)
        return false;
        
    RefPtr<CSSMutableStyleDeclaration> style = styleAtPosition(endingSelection().start());

    Node* listNode = emptyListItem->parentNode();
    RefPtr<Node> newBlock = isListElement(listNode->parentNode()) ? createListItemElement(document()) : createDefaultParagraphElement(document());
    
    if (emptyListItem->renderer()->nextSibling()) {
        if (emptyListItem->renderer()->previousSibling())
            splitElement(static_cast<Element*>(listNode), emptyListItem);
        insertNodeBefore(newBlock.get(), listNode);
        removeNode(emptyListItem);
    } else {
        insertNodeAfter(newBlock.get(), listNode);
        removeNode(emptyListItem->renderer()->previousSibling() ? emptyListItem : listNode);
    }
    
    appendBlockPlaceholder(newBlock.get());
    setEndingSelection(Selection(Position(newBlock.get(), 0), DOWNSTREAM));
    
    CSSComputedStyleDeclaration endingStyle(endingSelection().start().node());
    endingStyle.diff(style.get());
    if (style->length() > 0)
        applyStyle(style.get());
    
    return true;
}

PassRefPtr<Element> createBlockPlaceholderElement(Document* document)
{
    ExceptionCode ec = 0;
    RefPtr<Element> breakNode = document->createElementNS(xhtmlNamespaceURI, "br", ec);
    ASSERT(ec == 0);
    static String classString = "webkit-block-placeholder";
    breakNode->setAttribute(classAttr, classString);
    return breakNode.release();
}

} // namespace WebCore
