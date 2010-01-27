/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (IndentOutdentCommandINCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "IndentOutdentCommand.h"

#include "Document.h"
#include "Element.h"
#include "HTMLBlockquoteElement.h"
#include "HTMLNames.h"
#include "InsertLineBreakCommand.h"
#include "InsertListCommand.h"
#include "Range.h"
#include "SplitElementCommand.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

static String indentBlockquoteString()
{
    DEFINE_STATIC_LOCAL(String, string, ("webkit-indent-blockquote"));
    return string;
}

static PassRefPtr<HTMLBlockquoteElement> createIndentBlockquoteElement(Document* document)
{
    RefPtr<HTMLBlockquoteElement> element = new HTMLBlockquoteElement(blockquoteTag, document);
    element->setAttribute(classAttr, indentBlockquoteString());
    element->setAttribute(styleAttr, "margin: 0 0 0 40px; border: none; padding: 0px;");
    return element.release();
}

static bool isListOrIndentBlockquote(const Node* node)
{
    return node && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(blockquoteTag));
}

IndentOutdentCommand::IndentOutdentCommand(Document* document, EIndentType typeOfAction, int marginInPixels)
    : CompositeEditCommand(document), m_typeOfAction(typeOfAction), m_marginInPixels(marginInPixels)
{
}

bool IndentOutdentCommand::tryIndentingAsListItem(const VisiblePosition& endOfCurrentParagraph)
{
    // If our selection is not inside a list, bail out.
    Node* lastNodeInSelectedParagraph = endOfCurrentParagraph.deepEquivalent().node();
    RefPtr<Element> listNode = enclosingList(lastNodeInSelectedParagraph);
    if (!listNode)
        return false;

    // Find the list item enclosing the current paragraph
    Element* selectedListItem = static_cast<Element*>(enclosingBlock(lastNodeInSelectedParagraph));
    // FIXME: enclosingBlock shouldn't return the passed in element.  See the
    // comment on the function about how to fix rather than having to adjust here.
    if (selectedListItem == lastNodeInSelectedParagraph)
        selectedListItem = static_cast<Element*>(enclosingBlock(lastNodeInSelectedParagraph->parentNode()));

    // FIXME: we need to deal with the case where there is no li (malformed HTML)
    if (!selectedListItem->hasTagName(liTag))
        return false;
    
    // FIXME: previousElementSibling does not ignore non-rendered content like <span></span>.  Should we?
    Element* previousList = selectedListItem->previousElementSibling();
    Element* nextList = selectedListItem->nextElementSibling();

    RefPtr<Element> newList = document()->createElement(listNode->tagQName(), false);
    insertNodeBefore(newList, selectedListItem);

    moveParagraphWithClones(startOfParagraph(endOfCurrentParagraph), endOfCurrentParagraph, newList.get(), selectedListItem);

    if (canMergeLists(previousList, newList.get()))
        mergeIdenticalElements(previousList, newList);
    if (canMergeLists(newList.get(), nextList))
        mergeIdenticalElements(newList, nextList);

    return true;
}
    
void IndentOutdentCommand::indentIntoBlockquote(const VisiblePosition& endOfCurrentParagraph, const VisiblePosition& endOfNextParagraph, RefPtr<Element>& targetBlockquote)
{
    Node* enclosingCell = 0;

    Position start = startOfParagraph(endOfCurrentParagraph).deepEquivalent();
    enclosingCell = enclosingNodeOfType(start, &isTableCell);
    Node* nodeToSplitTo;
    if (enclosingCell)
        nodeToSplitTo = enclosingCell;
    else if (enclosingList(start.node()))
        nodeToSplitTo = enclosingBlock(start.node());
    else
        nodeToSplitTo = editableRootForPosition(start);

    RefPtr<Node> outerBlock = splitTreeToNode(start.node(), nodeToSplitTo);

    if (!targetBlockquote) {
        // Create a new blockquote and insert it as a child of the root editable element. We accomplish
        // this by splitting all parents of the current paragraph up to that point.
        targetBlockquote = createIndentBlockquoteElement(document());
        insertNodeBefore(targetBlockquote, outerBlock);
    }

    moveParagraphWithClones(startOfParagraph(endOfCurrentParagraph), endOfCurrentParagraph, targetBlockquote.get(), outerBlock.get());
    
    // Don't put the next paragraph in the blockquote we just created for this paragraph unless 
    // the next paragraph is in the same cell.
    if (enclosingCell && enclosingCell != enclosingNodeOfType(endOfNextParagraph.deepEquivalent(), &isTableCell))
        targetBlockquote = 0;
}

void IndentOutdentCommand::indentRegion(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection)
{
    // Special case empty unsplittable elements because there's nothing to split
    // and there's nothing to move.
    Position start = startOfSelection.deepEquivalent().downstream();
    if (isAtUnsplittableElement(start)) {
        RefPtr<Element> blockquote = createIndentBlockquoteElement(document());
        insertNodeAt(blockquote, start);
        RefPtr<Element> placeholder = createBreakElement(document());
        appendNode(placeholder, blockquote);
        setEndingSelection(VisibleSelection(Position(placeholder.get(), 0), DOWNSTREAM));
        return;
    }

    RefPtr<Element> blockquoteForNextIndent;
    VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
    VisiblePosition endAfterSelection = endOfParagraph(endOfParagraph(endOfSelection).next());
    while (endOfCurrentParagraph != endAfterSelection) {
        // Iterate across the selected paragraphs...
        VisiblePosition endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        if (tryIndentingAsListItem(endOfCurrentParagraph))
            blockquoteForNextIndent = 0;
        else
            indentIntoBlockquote(endOfCurrentParagraph, endOfNextParagraph, blockquoteForNextIndent);

        // indentIntoBlockquote could move more than one paragraph if the paragraph
        // is in a list item or a table. As a result, endAfterSelection could refer to a position
        // no longer in the document.
        if (endAfterSelection.isNotNull() && !endAfterSelection.deepEquivalent().node()->inDocument())
            break;
        // Sanity check: Make sure our moveParagraph calls didn't remove endOfNextParagraph.deepEquivalent().node()
        // If somehow we did, return to prevent crashes.
        if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.deepEquivalent().node()->inDocument()) {
            ASSERT_NOT_REACHED();
            return;
        }
        endOfCurrentParagraph = endOfNextParagraph;
    }   
}

void IndentOutdentCommand::outdentParagraph()
{
    VisiblePosition visibleStartOfParagraph = startOfParagraph(endingSelection().visibleStart());
    VisiblePosition visibleEndOfParagraph = endOfParagraph(visibleStartOfParagraph);

    Node* enclosingNode = enclosingNodeOfType(visibleStartOfParagraph.deepEquivalent(), &isListOrIndentBlockquote);
    if (!enclosingNode || !enclosingNode->parentNode()->isContentEditable())  // We can't outdent if there is no place to go!
        return;

    // Use InsertListCommand to remove the selection from the list
    if (enclosingNode->hasTagName(olTag)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::OrderedList));
        return;        
    }
    if (enclosingNode->hasTagName(ulTag)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::UnorderedList));
        return;
    }
    
    // The selection is inside a blockquote i.e. enclosingNode is a blockquote
    VisiblePosition positionInEnclosingBlock = VisiblePosition(Position(enclosingNode, 0));
    VisiblePosition startOfEnclosingBlock = startOfBlock(positionInEnclosingBlock);
    VisiblePosition lastPositionInEnclosingBlock = VisiblePosition(Position(enclosingNode, enclosingNode->childNodeCount()));
    VisiblePosition endOfEnclosingBlock = endOfBlock(lastPositionInEnclosingBlock);
    if (visibleStartOfParagraph == startOfEnclosingBlock &&
        visibleEndOfParagraph == endOfEnclosingBlock) {
        // The blockquote doesn't contain anything outside the paragraph, so it can be totally removed.
        Node* splitPoint = enclosingNode->nextSibling();
        removeNodePreservingChildren(enclosingNode);
        // outdentRegion() assumes it is operating on the first paragraph of an enclosing blockquote, but if there are multiply nested blockquotes and we've
        // just removed one, then this assumption isn't true. By splitting the next containing blockquote after this node, we keep this assumption true
        if (splitPoint) {
            if (Node* splitPointParent = splitPoint->parentNode()) {
                if (splitPointParent->hasTagName(blockquoteTag)
                    && !splitPoint->hasTagName(blockquoteTag)
                    && splitPointParent->parentNode()->isContentEditable()) // We can't outdent if there is no place to go!
                    splitElement(static_cast<Element*>(splitPointParent), splitPoint);
            }
        }
        
        updateLayout();
        visibleStartOfParagraph = VisiblePosition(visibleStartOfParagraph.deepEquivalent());
        visibleEndOfParagraph = VisiblePosition(visibleEndOfParagraph.deepEquivalent());
        if (visibleStartOfParagraph.isNotNull() && !isStartOfParagraph(visibleStartOfParagraph))
            insertNodeAt(createBreakElement(document()), visibleStartOfParagraph.deepEquivalent());
        if (visibleEndOfParagraph.isNotNull() && !isEndOfParagraph(visibleEndOfParagraph))
            insertNodeAt(createBreakElement(document()), visibleEndOfParagraph.deepEquivalent());

        return;
    }
    Node* enclosingBlockFlow = enclosingBlock(visibleStartOfParagraph.deepEquivalent().node());
    RefPtr<Node> splitBlockquoteNode = enclosingNode;
    if (enclosingBlockFlow != enclosingNode)
        splitBlockquoteNode = splitTreeToNode(enclosingBlockFlow, enclosingNode, true);
    else {
        // We split the blockquote at where we start outdenting.
        splitElement(static_cast<Element*>(enclosingNode), visibleStartOfParagraph.deepEquivalent().node());
    }
    RefPtr<Node> placeholder = createBreakElement(document());
    insertNodeBefore(placeholder, splitBlockquoteNode);
    moveParagraph(startOfParagraph(visibleStartOfParagraph), endOfParagraph(visibleEndOfParagraph), VisiblePosition(Position(placeholder.get(), 0)), true);
}

void IndentOutdentCommand::outdentRegion(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection)
{
    VisiblePosition endOfLastParagraph = endOfParagraph(endOfSelection);

    if (endOfParagraph(startOfSelection) == endOfLastParagraph) {
        outdentParagraph();
        return;
    }
    
    Position originalSelectionEnd = endingSelection().end();
    VisiblePosition endOfCurrentParagraph = endOfParagraph(startOfSelection);
    VisiblePosition endAfterSelection = endOfParagraph(endOfParagraph(endOfSelection).next());

    while (endOfCurrentParagraph != endAfterSelection) {
        VisiblePosition endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        if (endOfCurrentParagraph == endOfLastParagraph)
            setEndingSelection(VisibleSelection(originalSelectionEnd, DOWNSTREAM));
        else
            setEndingSelection(endOfCurrentParagraph);
        
        outdentParagraph();
        
        // outdentParagraph could move more than one paragraph if the paragraph
        // is in a list item. As a result, endAfterSelection and endOfNextParagraph
        // could refer to positions no longer in the document.
        if (endAfterSelection.isNotNull() && !endAfterSelection.deepEquivalent().node()->inDocument())
            break;
            
        if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.deepEquivalent().node()->inDocument()) {
            endOfCurrentParagraph = endingSelection().end();
            endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        }
        endOfCurrentParagraph = endOfNextParagraph;
    }
}

void IndentOutdentCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    if (!endingSelection().rootEditableElement())
        return;
        
    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition visibleStart = endingSelection().visibleStart();
    // When a selection ends at the start of a paragraph, we rarely paint 
    // the selection gap before that paragraph, because there often is no gap.  
    // In a case like this, it's not obvious to the user that the selection 
    // ends "inside" that paragraph, so it would be confusing if Indent/Outdent 
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left 
    // margin/padding, but not others.  We should make the gap painting more consistent and 
    // then use a left margin/padding rule here.
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd))
        setEndingSelection(VisibleSelection(visibleStart, visibleEnd.previous(true)));

    VisibleSelection selection = selectionForParagraphIteration(endingSelection());
    VisiblePosition startOfSelection = selection.visibleStart();
    VisiblePosition endOfSelection = selection.visibleEnd();
    
    int startIndex = indexForVisiblePosition(startOfSelection);
    int endIndex = indexForVisiblePosition(endOfSelection);
    
    ASSERT(!startOfSelection.isNull());
    ASSERT(!endOfSelection.isNull());
    
    if (m_typeOfAction == Indent)
        indentRegion(startOfSelection, endOfSelection);
    else
        outdentRegion(startOfSelection, endOfSelection);

    updateLayout();
    
    RefPtr<Range> startRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), startIndex, 0, true);
    RefPtr<Range> endRange = TextIterator::rangeFromLocationAndLength(document()->documentElement(), endIndex, 0, true);
    if (startRange && endRange)
        setEndingSelection(VisibleSelection(startRange->startPosition(), endRange->startPosition(), DOWNSTREAM));
}

}
