/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Element.h"
#include "FormatBlockCommand.h"
#include "Document.h"
#include "htmlediting.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "visible_units.h"

namespace WebCore {

using namespace HTMLNames;

FormatBlockCommand::FormatBlockCommand(Document* document, const AtomicString& tagName) 
    : CompositeEditCommand(document), m_tagName(tagName)
{
}

bool FormatBlockCommand::modifyRange()
{
    ASSERT(endingSelection().isRange());
    VisiblePosition visibleStart = endingSelection().visibleStart();
    VisiblePosition visibleEnd = endingSelection().visibleEnd();
    VisiblePosition startOfLastParagraph = startOfParagraph(visibleEnd);
    
    if (startOfParagraph(visibleStart) == startOfLastParagraph)
        return false;

    setEndingSelection(visibleStart);
    doApply();
    visibleStart = endingSelection().visibleStart();
    VisiblePosition nextParagraph = endOfParagraph(visibleStart).next();
    while (nextParagraph.isNotNull() && nextParagraph != startOfLastParagraph) {
        setEndingSelection(nextParagraph);
        doApply();
        nextParagraph = endOfParagraph(endingSelection().visibleStart()).next();
    }
    setEndingSelection(visibleEnd);
    doApply();
    visibleEnd = endingSelection().visibleEnd();
    setEndingSelection(VisibleSelection(visibleStart.deepEquivalent(), visibleEnd.deepEquivalent(), DOWNSTREAM));

    return true;
}

void FormatBlockCommand::doApply()
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
    // ends "inside" that paragraph, so it would be confusing if FormatBlock
    // operated on that paragraph.
    // FIXME: We paint the gap before some paragraphs that are indented with left 
    // margin/padding, but not others.  We should make the gap painting more consistent and 
    // then use a left margin/padding rule here.
    if (visibleEnd != visibleStart && isStartOfParagraph(visibleEnd))
        setEndingSelection(VisibleSelection(visibleStart, visibleEnd.previous(true)));

    if (endingSelection().isRange() && modifyRange())
        return;

    ExceptionCode ec;
    String localName, prefix;
    if (!Document::parseQualifiedName(m_tagName, prefix, localName, ec))
        return;
    QualifiedName qTypeOfBlock(prefix, localName, xhtmlNamespaceURI);

    Node* refNode = enclosingBlockFlowElement(endingSelection().visibleStart());
    if (refNode->hasTagName(qTypeOfBlock))
        // We're already in a block with the format we want, so we don't have to do anything
        return;
    
    VisiblePosition paragraphStart = startOfParagraph(endingSelection().visibleStart());
    VisiblePosition paragraphEnd = endOfParagraph(endingSelection().visibleStart());
    VisiblePosition blockStart = startOfBlock(endingSelection().visibleStart());
    VisiblePosition blockEnd = endOfBlock(endingSelection().visibleStart());
    RefPtr<Element> blockNode = createHTMLElement(document(), m_tagName);
    RefPtr<Element> placeholder = createBreakElement(document());
    
    Node* root = endingSelection().start().node()->rootEditableElement();
    if (validBlockTag(refNode->nodeName().lower()) && 
        paragraphStart == blockStart && paragraphEnd == blockEnd && 
        refNode != root && !root->isDescendantOf(refNode))
        // Already in a valid block tag that only contains the current paragraph, so we can swap with the new tag
        insertNodeBefore(blockNode, refNode);
    else {
        // Avoid inserting inside inline elements that surround paragraphStart with upstream().
        // This is only to avoid creating bloated markup.
        insertNodeAt(blockNode, paragraphStart.deepEquivalent().upstream());
    }
    appendNode(placeholder, blockNode);
    
    VisiblePosition destination(Position(placeholder.get(), 0));
    if (paragraphStart == paragraphEnd && !lineBreakExistsAtVisiblePosition(paragraphStart)) {
        setEndingSelection(destination);
        return;
    }
    moveParagraph(paragraphStart, paragraphEnd, destination, true, false);
}

}
