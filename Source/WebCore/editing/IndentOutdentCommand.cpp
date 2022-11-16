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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "IndentOutdentCommand.h"

#include "Document.h"
#include "Editing.h"
#include "ElementTraversal.h"
#include "HTMLBRElement.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLUListElement.h"
#include "InsertLineBreakCommand.h"
#include "InsertListCommand.h"
#include "RenderElement.h"
#include "SplitElementCommand.h"
#include "Text.h"
#include "VisibleUnits.h"

namespace WebCore {

using namespace HTMLNames;

static bool isListOrIndentBlockquote(const Node* node)
{
    return node && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(blockquoteTag));
}

IndentOutdentCommand::IndentOutdentCommand(Document& document, EIndentType typeOfAction)
    : ApplyBlockElementCommand(document, blockquoteTag, "margin: 0 0 0 40px; border: none; padding: 0px;"_s)
    , m_typeOfAction(typeOfAction)
{
}

bool IndentOutdentCommand::tryIndentingAsListItem(const Position& start, const Position& end)
{
    // If our selection is not inside a list, bail out.
    RefPtr lastNodeInSelectedParagraph = start.deprecatedNode();
    RefPtr<Element> listNode = enclosingList(lastNodeInSelectedParagraph.get());
    if (!listNode)
        return false;

    // Find the block that we want to indent.  If it's not a list item (e.g., a div inside a list item), we bail out.
    RefPtr<Element> selectedListItem = enclosingBlock(lastNodeInSelectedParagraph.get());

    if (!selectedListItem || !selectedListItem->hasTagName(liTag))
        return false;
    
    // FIXME: previousElementSibling does not ignore non-rendered content like <span></span>.  Should we?
    RefPtr<Element> previousList = ElementTraversal::previousSibling(*selectedListItem);
    RefPtr<Element> nextList = ElementTraversal::nextSibling(*selectedListItem);

    RefPtr<Element> newList;
    if (is<HTMLUListElement>(*listNode))
        newList = HTMLUListElement::create(document());
    else
        newList = HTMLOListElement::create(document());
    insertNodeBefore(*newList, *selectedListItem);

    moveParagraphWithClones(start, end, newList.get(), selectedListItem.get());

    if (canMergeLists(previousList.get(), newList.get()))
        mergeIdenticalElements(*previousList, *newList);
    if (canMergeLists(newList.get(), nextList.get()))
        mergeIdenticalElements(*newList, *nextList);

    return true;
}

void IndentOutdentCommand::indentIntoBlockquote(const Position& start, const Position& end, RefPtr<Element>& targetBlockquote)
{
    RefPtr enclosingCell = enclosingNodeOfType(start, &isTableCell);
    auto nodeToSplitTo = [&]() -> RefPtr<Node> {
        if (enclosingCell)
            return enclosingCell;
        if (enclosingList(start.containerNode()))
            return enclosingBlock(start.containerNode());
        return editableRootForPosition(start);
    }();

    if (!nodeToSplitTo)
        return;

    RefPtr<Node> nodeAfterStart = start.computeNodeAfterPosition();
    RefPtr<Node> outerBlock = (start.containerNode() == nodeToSplitTo) ? start.containerNode() : splitTreeToNode(*start.containerNode(), *nodeToSplitTo);
    if (!outerBlock)
        return;

    VisiblePosition startOfContents = start;
    if (!targetBlockquote) {
        // Create a new blockquote and insert it as a child of the root editable element. We accomplish
        // this by splitting all parents of the current paragraph up to that point.
        targetBlockquote = createBlockElement();
        if (outerBlock == nodeToSplitTo)
            insertNodeAt(*targetBlockquote, start);
        else if (!insertNodeBefore(*targetBlockquote, *outerBlock))
            return;
        if (!targetBlockquote->hasEditableStyle()) {
            removeNode(*targetBlockquote);
            return;
        }
        startOfContents = positionInParentAfterNode(targetBlockquote.get());
    }
    
    if (startOfContents.deepEquivalent().containerNode() && !startOfContents.deepEquivalent().containerNode()->isDescendantOf(outerBlock.get()) && startOfContents.deepEquivalent().containerNode()->parentNode() != outerBlock->parentNode())
        return;

    moveParagraphWithClones(startOfContents, end, targetBlockquote.get(), outerBlock.get());
}

void IndentOutdentCommand::outdentParagraph()
{
    VisiblePosition visibleStartOfParagraph = startOfParagraph(endingSelection().visibleStart());
    VisiblePosition visibleEndOfParagraph = endOfParagraph(visibleStartOfParagraph);

    RefPtr enclosingNode = downcast<HTMLElement>(enclosingNodeOfType(visibleStartOfParagraph.deepEquivalent(), &isListOrIndentBlockquote));
    if (!enclosingNode || !enclosingNode->parentNode() || !enclosingNode->parentNode()->hasEditableStyle()) // We can't outdent if there is no place to go!
        return;

    // Use InsertListCommand to remove the selection from the list
    if (enclosingNode->hasTagName(olTag)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::Type::OrderedList));
        return;        
    }
    if (enclosingNode->hasTagName(ulTag)) {
        applyCommandToComposite(InsertListCommand::create(document(), InsertListCommand::Type::UnorderedList));
        return;
    }
    
    // The selection is inside a blockquote i.e. enclosingNode is a blockquote
    VisiblePosition positionInEnclosingBlock = VisiblePosition(firstPositionInNode(enclosingNode.get()));
    // If the blockquote is inline, the start of the enclosing block coincides with
    // positionInEnclosingBlock.
    VisiblePosition startOfEnclosingBlock = (enclosingNode->renderer() && enclosingNode->renderer()->isInline()) ? positionInEnclosingBlock : startOfBlock(positionInEnclosingBlock);
    VisiblePosition lastPositionInEnclosingBlock = VisiblePosition(lastPositionInNode(enclosingNode.get()));
    VisiblePosition endOfEnclosingBlock = endOfBlock(lastPositionInEnclosingBlock);
    if (visibleStartOfParagraph == startOfEnclosingBlock &&
        visibleEndOfParagraph == endOfEnclosingBlock) {
        // The blockquote doesn't contain anything outside the paragraph, so it can be totally removed.
        RefPtr splitPoint = enclosingNode->nextSibling();
        removeNodePreservingChildren(*enclosingNode);
        // outdentRegion() assumes it is operating on the first paragraph of an enclosing blockquote, but if there are multiply nested blockquotes and we've
        // just removed one, then this assumption isn't true. By splitting the next containing blockquote after this node, we keep this assumption true
        if (splitPoint) {
            if (ContainerNode* splitPointParent = splitPoint->parentNode()) {
                if (splitPointParent->hasTagName(blockquoteTag)
                    && !splitPoint->hasTagName(blockquoteTag)
                    && splitPointParent->parentNode()
                    && splitPointParent->parentNode()->hasEditableStyle()) // We can't outdent if there is no place to go!
                    splitElement(downcast<Element>(*splitPointParent), *splitPoint);
            }
        }

        document().updateLayoutIgnorePendingStylesheets();
        visibleStartOfParagraph = VisiblePosition(visibleStartOfParagraph.deepEquivalent());
        visibleEndOfParagraph = VisiblePosition(visibleEndOfParagraph.deepEquivalent());
        if (visibleStartOfParagraph.isNotNull() && !isStartOfParagraph(visibleStartOfParagraph))
            insertNodeAt(HTMLBRElement::create(document()), visibleStartOfParagraph.deepEquivalent());
        if (visibleEndOfParagraph.isNotNull() && !isEndOfParagraph(visibleEndOfParagraph))
            insertNodeAt(HTMLBRElement::create(document()), visibleEndOfParagraph.deepEquivalent());

        return;
    }

    RefPtr startOfParagraphNode = visibleStartOfParagraph.deepEquivalent().deprecatedNode();
    RefPtr enclosingBlockFlow = enclosingBlock(startOfParagraphNode.get());
    RefPtr<Node> splitBlockquoteNode = enclosingNode;
    if (enclosingBlockFlow != enclosingNode)
        splitBlockquoteNode = splitTreeToNode(*startOfParagraphNode, *enclosingNode, true);
    else {
        // We split the blockquote at where we start outdenting.
        RefPtr highestInlineNode = highestEnclosingNodeOfType(visibleStartOfParagraph.deepEquivalent(), isInline, CannotCrossEditingBoundary, enclosingBlockFlow.get());
        splitElement(*enclosingNode, highestInlineNode ? *highestInlineNode : *visibleStartOfParagraph.deepEquivalent().deprecatedNode());
    }
    auto placeholder = HTMLBRElement::create(document());
    insertNodeBefore(placeholder, *splitBlockquoteNode);
    if (!placeholder->isConnected())
        return;
    auto visibleStartOfParagraphToMove = startOfParagraph(visibleStartOfParagraph);
    auto visibleEndOfParagraphToMove = endOfParagraph(visibleEndOfParagraph);
    if (visibleStartOfParagraphToMove.isNull() || visibleEndOfParagraphToMove.isNull())
        return;
    moveParagraph(visibleStartOfParagraphToMove, visibleEndOfParagraphToMove, positionBeforeNode(placeholder.ptr()), true);
}

// FIXME: We should merge this function with ApplyBlockElementCommand::formatSelection
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
            setEndingSelection(VisibleSelection(originalSelectionEnd));
        else
            setEndingSelection(endOfCurrentParagraph);

        outdentParagraph();

        // outdentParagraph could move more than one paragraph if the paragraph
        // is in a list item. As a result, endAfterSelection and endOfNextParagraph
        // could refer to positions no longer in the document.
        if (endAfterSelection.isNotNull() && !endAfterSelection.deepEquivalent().anchorNode()->isConnected())
            break;

        if (endOfNextParagraph.isNotNull() && !endOfNextParagraph.deepEquivalent().anchorNode()->isConnected()) {
            endOfCurrentParagraph = endingSelection().end();
            endOfNextParagraph = endOfParagraph(endOfCurrentParagraph.next());
        }
        endOfCurrentParagraph = endOfNextParagraph;

        if (endOfCurrentParagraph.isNull()) {
            // If the end of the current paragraph is null, we'll end up looping infinitely, since the end of the next paragraph
            // (and the paragraph after that, and so on) will always be null. To avoid this infinite loop, just bail.
            break;
        }
    }
}

void IndentOutdentCommand::formatSelection(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection)
{
    if (m_typeOfAction == Indent)
        ApplyBlockElementCommand::formatSelection(startOfSelection, endOfSelection);
    else
        outdentRegion(startOfSelection, endOfSelection);
}

void IndentOutdentCommand::formatRange(const Position& start, const Position& end, const Position&, RefPtr<Element>& blockquoteForNextIndent)
{
    if (tryIndentingAsListItem(start, end))
        blockquoteForNextIndent = nullptr;
    else
        indentIntoBlockquote(start, end, blockquoteForNextIndent);
}

}
