/*
 * Copyright (C) 2005, 2006 Apple Inc.  All rights reserved.
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
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "InsertLineBreakCommand.h"

#include "Document.h"
#include "Editing.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLBRElement.h"
#include "HTMLHRElement.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "RenderElement.h"
#include "RenderText.h"
#include "Text.h"
#include "VisibleUnits.h"

namespace WebCore {

using namespace HTMLNames;

InsertLineBreakCommand::InsertLineBreakCommand(Document& document)
    : CompositeEditCommand(document)
{
}

bool InsertLineBreakCommand::preservesTypingStyle() const
{
    return true;
}

// Whether we should insert a break element or a '\n'.
bool InsertLineBreakCommand::shouldUseBreakElement(const Position& position)
{
    // An editing position like [input, 0] actually refers to the position before
    // the input element, and in that case we need to check the input element's
    // parent's renderer.
    auto* node = position.parentAnchoredEquivalent().deprecatedNode();
    return node && node->renderer() && !node->renderer()->style().preserveNewline();
}

void InsertLineBreakCommand::doApply()
{
    deleteSelection();
    VisibleSelection selection = endingSelection();
    if (selection.isNoneOrOrphaned())
        return;
    
    VisiblePosition caret(selection.visibleStart());
    // FIXME: If the node is hidden, we should still be able to insert text. 
    // For now, we return to avoid a crash.  https://bugs.webkit.org/show_bug.cgi?id=40342
    if (caret.isNull())
        return;

    Position position(caret.deepEquivalent());

    position = positionAvoidingSpecialElementBoundary(position);
    position = positionOutsideTabSpan(position);

    if (!isEditablePosition(position))
        return;

    RefPtr<Node> nodeToInsert;
    if (shouldUseBreakElement(position))
        nodeToInsert = HTMLBRElement::create(document());
    else
        nodeToInsert = document().createTextNode("\n"_s);
    
    // FIXME: Need to merge text nodes when inserting just after or before text.
    document().updateLayoutIgnorePendingStylesheets();
    if (isEndOfParagraph(caret) && !lineBreakExistsAtVisiblePosition(caret)) {
        bool needExtraLineBreak = !is<HTMLHRElement>(*position.deprecatedNode()) && !is<HTMLTableElement>(*position.deprecatedNode());

        insertNodeAt(*nodeToInsert, position);
        
        if (needExtraLineBreak)
            insertNodeBefore(nodeToInsert->cloneNode(false), *nodeToInsert);
        
        VisiblePosition endingPosition(positionBeforeNode(nodeToInsert.get()));
        setEndingSelection(VisibleSelection(endingPosition, endingSelection().isDirectional()));
    } else if (position.deprecatedEditingOffset() <= caretMinOffset(*position.deprecatedNode())) {
        insertNodeAt(*nodeToInsert, position);
        
        // Insert an extra br or '\n' if the just inserted one collapsed.
        if (!isStartOfParagraph(positionBeforeNode(nodeToInsert.get())))
            insertNodeBefore(nodeToInsert->cloneNode(false), *nodeToInsert);
        
        setEndingSelection(VisibleSelection(positionInParentAfterNode(nodeToInsert.get()), Affinity::Downstream, endingSelection().isDirectional()));
    // If we're inserting after all of the rendered text in a text node, or into a non-text node,
    // a simple insertion is sufficient.
    } else if (position.deprecatedEditingOffset() >= caretMaxOffset(*position.deprecatedNode()) || !is<Text>(*position.deprecatedNode())) {
        insertNodeAt(*nodeToInsert, position);
        setEndingSelection(VisibleSelection(positionInParentAfterNode(nodeToInsert.get()), Affinity::Downstream, endingSelection().isDirectional()));
    } else if (is<Text>(*position.deprecatedNode())) {
        // Split a text node
        Text& textNode = downcast<Text>(*position.deprecatedNode());
        splitTextNode(textNode, position.deprecatedEditingOffset());
        insertNodeBefore(*nodeToInsert, textNode);
        Position endingPosition = firstPositionInNode(&textNode);
        
        // Handle whitespace that occurs after the split
        document().updateLayoutIgnorePendingStylesheets();
        if (!endingPosition.isRenderedCharacter()) {
            Position positionBeforeTextNode(positionInParentBeforeNode(&textNode));
            // Clear out all whitespace and insert one non-breaking space
            deleteInsignificantTextDownstream(endingPosition);
            ASSERT(!textNode.renderer() || textNode.renderer()->style().collapseWhiteSpace());
            // Deleting insignificant whitespace will remove textNode if it contains nothing but insignificant whitespace.
            if (textNode.isConnected())
                insertTextIntoNode(textNode, 0, nonBreakingSpaceString());
            else {
                auto nbspNode = document().createTextNode(String { nonBreakingSpaceString() });
                auto* nbspNodePtr = nbspNode.ptr();
                insertNodeAt(WTFMove(nbspNode), positionBeforeTextNode);
                endingPosition = firstPositionInNode(nbspNodePtr);
            }
        }
        
        setEndingSelection(VisibleSelection(endingPosition, Affinity::Downstream, endingSelection().isDirectional()));
    }

    // Handle the case where there is a typing style.

    RefPtr<EditingStyle> typingStyle = document().selection().typingStyle();

    if (typingStyle && !typingStyle->isEmpty()) {
        // Apply the typing style to the inserted line break, so that if the selection
        // leaves and then comes back, new input will have the right style.
        // FIXME: We shouldn't always apply the typing style to the line break here,
        // see <rdar://problem/5794462>.
        applyStyle(typingStyle.get(), firstPositionInOrBeforeNode(nodeToInsert.get()), lastPositionInOrAfterNode(nodeToInsert.get()));
        // Even though this applyStyle operates on a Range, it still sets an endingSelection().
        // It tries to set a VisibleSelection around the content it operated on. So, that VisibleSelection
        // will either (a) select the line break we inserted, or it will (b) be a caret just 
        // before the line break (if the line break is at the end of a block it isn't selectable).
        // So, this next call sets the endingSelection() to a caret just after the line break 
        // that we inserted, or just before it if it's at the end of a block.
        setEndingSelection(endingSelection().visibleEnd());
    }

    rebalanceWhitespace();
}

}
