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

FormatBlockCommand::FormatBlockCommand(Document* document, const String& tagName) 
    : CompositeEditCommand(document), m_tagName(tagName)
{}



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
    setEndingSelection(Selection(visibleStart.deepEquivalent(), visibleEnd.deepEquivalent(), DOWNSTREAM));

    return true;
}

void FormatBlockCommand::doApply()
{
    if (endingSelection().isNone())
        return;

    if (endingSelection().isRange() && modifyRange())
        return;
    
    if (!endingSelection().rootEditableElement())
        return;
    
    String localName, prefix;
    if (!Document::parseQualifiedName(m_tagName, prefix, localName))
        return;
    QualifiedName qTypeOfBlock = QualifiedName(AtomicString(prefix), AtomicString(localName), xhtmlNamespaceURI);
    
    Node* enclosingBlock = enclosingBlockFlowElement(endingSelection().visibleStart());
    if (enclosingBlock->hasTagName(qTypeOfBlock))
        // We're already in a block with the format we want, so we don't have to do anything
        return;
    
    VisiblePosition paragraphStart = startOfParagraph(endingSelection().visibleStart());
    VisiblePosition paragraphEnd = endOfParagraph(endingSelection().visibleStart());
    VisiblePosition blockStart = startOfBlock(endingSelection().visibleStart());
    VisiblePosition blockEnd = endOfBlock(endingSelection().visibleStart());
    RefPtr<Node> blockNode = createElement(document(), m_tagName);
    RefPtr<Node> placeholder = createBreakElement(document());
    
    if (validBlockTag(enclosingBlock->nodeName().lower()) && paragraphStart == blockStart && paragraphEnd == blockEnd)
        // Already in a valid block tag that only contains the current paragraph, so we can swap with the new tag
        insertNodeBefore(blockNode.get(), enclosingBlock);
    else {
        Position upstreamStart = paragraphStart.deepEquivalent().upstream();
        insertNodeAt(blockNode.get(), upstreamStart.node(), upstreamStart.offset());
    }
    appendNode(placeholder.get(), blockNode.get());
    moveParagraph(paragraphStart, paragraphEnd, VisiblePosition(Position(placeholder.get(), 0)), true, false);
}

}
