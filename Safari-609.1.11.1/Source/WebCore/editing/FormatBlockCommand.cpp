/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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
#include "FormatBlockCommand.h"

#include "Document.h"
#include "Editing.h"
#include "Element.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "Range.h"
#include "VisibleUnits.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

static Node* enclosingBlockToSplitTreeTo(Node* startNode);
static bool isElementForFormatBlock(const QualifiedName& tagName);

static inline bool isElementForFormatBlock(Node* node)
{
    return is<Element>(*node) && isElementForFormatBlock(downcast<Element>(*node).tagQName());
}

FormatBlockCommand::FormatBlockCommand(Document& document, const QualifiedName& tagName)
    : ApplyBlockElementCommand(document, tagName)
    , m_didApply(false)
{
}

void FormatBlockCommand::formatSelection(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection)
{
    if (!isElementForFormatBlock(tagName()))
        return;
    ApplyBlockElementCommand::formatSelection(startOfSelection, endOfSelection);
    m_didApply = true;
}

void FormatBlockCommand::formatRange(const Position& start, const Position& end, const Position& endOfSelection, RefPtr<Element>& blockNode)
{
    Node* nodeToSplitTo = enclosingBlockToSplitTreeTo(start.deprecatedNode());
    ASSERT(nodeToSplitTo);
    RefPtr<Node> outerBlock = (start.deprecatedNode() == nodeToSplitTo) ? start.deprecatedNode() : splitTreeToNode(*start.deprecatedNode(), *nodeToSplitTo);
    RefPtr<Node> nodeAfterInsertionPosition = outerBlock;

    auto range = Range::create(document(), start, endOfSelection);
    Element* refNode = enclosingBlockFlowElement(end);
    Element* root = editableRootForPosition(start);
    // Root is null for elements with contenteditable=false.
    if (!root || !refNode)
        return;
    if (isElementForFormatBlock(refNode->tagQName()) && start == startOfBlock(start)
        && (end == endOfBlock(end) || isNodeVisiblyContainedWithin(*refNode, range.get()))
        && refNode != root && !root->isDescendantOf(*refNode)) {
        // Already in a block element that only contains the current paragraph
        if (refNode->hasTagName(tagName()))
            return;
        nodeAfterInsertionPosition = refNode;
    }

    if (!blockNode) {
        // Create a new blockquote and insert it as a child of the root editable element. We accomplish
        // this by splitting all parents of the current paragraph up to that point.
        blockNode = createBlockElement();
        insertNodeBefore(*blockNode, *nodeAfterInsertionPosition);
    }

    Position lastParagraphInBlockNode = blockNode->lastChild() ? positionAfterNode(blockNode->lastChild()) : Position();
    bool wasEndOfParagraph = isEndOfParagraph(lastParagraphInBlockNode);

    moveParagraphWithClones(start, end, blockNode.get(), outerBlock.get());

    if (wasEndOfParagraph && !isEndOfParagraph(lastParagraphInBlockNode) && !isStartOfParagraph(lastParagraphInBlockNode))
        insertBlockPlaceholder(lastParagraphInBlockNode);
}
    
Element* FormatBlockCommand::elementForFormatBlockCommand(Range* range)
{
    if (!range)
        return nullptr;

    Node* commonAncestor = range->commonAncestorContainer();
    while (commonAncestor && !isElementForFormatBlock(commonAncestor))
        commonAncestor = commonAncestor->parentNode();

    if (!commonAncestor)
        return nullptr;

    Element* rootEditableElement = range->startContainer().rootEditableElement();
    if (!rootEditableElement || commonAncestor->contains(rootEditableElement))
        return nullptr;

    return commonAncestor->isElementNode() ? downcast<Element>(commonAncestor) : nullptr;
}

bool isElementForFormatBlock(const QualifiedName& tagName)
{
    static const auto blockTags = makeNeverDestroyed(HashSet<QualifiedName> {
        addressTag,
        articleTag,
        asideTag,
        blockquoteTag,
        ddTag,
        divTag,
        dlTag,
        dtTag,
        footerTag,
        h1Tag,
        h2Tag,
        h3Tag,
        h4Tag,
        h5Tag,
        h6Tag,
        headerTag,
        hgroupTag,
        mainTag,
        navTag,
        pTag,
        preTag,
        sectionTag,
    });
    return blockTags.get().contains(tagName);
}

Node* enclosingBlockToSplitTreeTo(Node* startNode)
{
    Node* lastBlock = startNode;
    for (Node* n = startNode; n; n = n->parentNode()) {
        if (!n->hasEditableStyle())
            return lastBlock;
        if (isTableCell(n) || n->hasTagName(bodyTag) || !n->parentNode() || !n->parentNode()->hasEditableStyle() || isElementForFormatBlock(n))
            return n;
        if (isBlock(n))
            lastBlock = n;
        if (isListHTMLElement(n))
            return n->parentNode()->hasEditableStyle() ? n->parentNode() : n;
    }
    return lastBlock;
}

}
