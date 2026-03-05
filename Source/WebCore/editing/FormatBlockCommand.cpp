/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#include "CSSSerializationContext.h"
#include "Document.h"
#include "Editing.h"
#include "EditingStyle.h"
#include "Element.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
#include "VisibleUnits.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashSet.h>

namespace WebCore {

using namespace HTMLNames;

static RefPtr<Node> enclosingBlockToSplitTreeTo(Node* startNode);
static bool isElementForFormatBlock(const QualifiedName& tagName);

static inline bool isElementForFormatBlock(Node& node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element && isElementForFormatBlock(element->tagQName());
}

FormatBlockCommand::FormatBlockCommand(Ref<Document>&& document, const QualifiedName& tagName)
    : ApplyBlockElementCommand(WTF::move(document), tagName)
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
    RefPtr nodeToSplitTo = enclosingBlockToSplitTreeTo(start.deprecatedNode());
    ASSERT(nodeToSplitTo);
    RefPtr<Node> outerBlock = (start.deprecatedNode() == nodeToSplitTo) ? RefPtr { start.deprecatedNode() } : splitTreeToNode(*start.deprecatedNode(), *nodeToSplitTo);
    if (!outerBlock)
        return;

    RefPtr<Node> nodeAfterInsertionPosition = outerBlock;

    auto range = makeSimpleRange(start, endOfSelection);
    RefPtr refNode = enclosingBlockFlowElement(end);
    RefPtr root = editableRootForPosition(start);
    // Root is null for elements with contenteditable=false.
    if (!root || !refNode)
        return;
    if (isElementForFormatBlock(refNode->tagQName()) && start == startOfBlock(start)
        && (end == endOfBlock(end) || (range && isNodeVisiblyContainedWithin(*refNode, *range)))
        && refNode != root && !root->isDescendantOf(*refNode)) {
        // Already in a block element that only contains the current paragraph
        if (refNode->hasTagName(tagName()))
            return;
        nodeAfterInsertionPosition = WTF::move(refNode);
    }

    if (!blockNode) {
        // Create a new blockquote and insert it as a child of the root editable element. We accomplish
        // this by splitting all parents of the current paragraph up to that point.
        blockNode = createBlockElement();
        insertNodeBefore(*blockNode, *nodeAfterInsertionPosition);
        // Preserve the inline style of the element being replaced so that formatting
        // such as color and font properties are not lost when the block type changes.
        // See: https://bugs.webkit.org/show_bug.cgi?id=47054
        if (RefPtr element = dynamicDowncast<HTMLElement>(nodeAfterInsertionPosition)) {
            if (element->hasAttribute(styleAttr)) {
                auto style = EditingStyle::create(element->inlineStyle());
                if (RefPtr styledBlockNode = dynamicDowncast<StyledElement>(blockNode))
                    style->removeStyleFromRulesAndContext(*styledBlockNode, styledBlockNode->parentNode());
                if (!style->isEmpty())
                    blockNode->setAttribute(styleAttr, AtomString { style->style()->asText(CSS::defaultSerializationContext()) });
            }
        }
    }

    RefPtr lastChild = blockNode->lastChild();
    Position lastParagraphInBlockNode = lastChild ? positionAfterNode(lastChild.get()) : Position();
    bool wasEndOfParagraph = isEndOfParagraph(lastParagraphInBlockNode);

    moveParagraphWithClones(start, end, blockNode.get(), outerBlock.get());

    if (wasEndOfParagraph && lastParagraphInBlockNode.anchorNode()->isConnected()
        && !isEndOfParagraph(lastParagraphInBlockNode) && !isStartOfParagraph(lastParagraphInBlockNode))
        insertBlockPlaceholder(lastParagraphInBlockNode);
}

RefPtr<Element> FormatBlockCommand::elementForFormatBlockCommand(const std::optional<SimpleRange>& range)
{
    if (!range)
        return nullptr;

    RefPtr commonAncestor = commonInclusiveAncestor<ComposedTree>(*range);
    while (commonAncestor && !isElementForFormatBlock(*commonAncestor))
        commonAncestor = commonAncestor->parentNode();
    RefPtr commonAncestorElement = dynamicDowncast<Element>(commonAncestor);
    if (!commonAncestorElement)
        return nullptr;

    RefPtr rootEditableElement = range->start.container->rootEditableElement();
    if (!rootEditableElement || commonAncestor->contains(rootEditableElement.get()))
        return nullptr;

    return commonAncestorElement;
}

bool isElementForFormatBlock(const QualifiedName& tagName)
{
    using namespace ElementNames;

    switch (tagName.nodeName()) {
    case HTML::address:
    case HTML::article:
    case HTML::aside:
    case HTML::blockquote:
    case HTML::dd:
    case HTML::div:
    case HTML::dl:
    case HTML::dt:
    case HTML::footer:
    case HTML::h1:
    case HTML::h2:
    case HTML::h3:
    case HTML::h4:
    case HTML::h5:
    case HTML::h6:
    case HTML::header:
    case HTML::hgroup:
    case HTML::main:
    case HTML::nav:
    case HTML::p:
    case HTML::pre:
    case HTML::section:
        return true;
    default:
        break;
    }
    return false;
}

RefPtr<Node> enclosingBlockToSplitTreeTo(Node* startNode)
{
    RefPtr lastBlock = startNode;
    for (RefPtr node = startNode; node; node = node->parentNode()) {
        if (!node->hasEditableStyle())
            return lastBlock;
        RefPtr parentNode = node->parentNode();
        if (isTableCell(*node) || node->hasTagName(bodyTag) || !parentNode || !parentNode->hasEditableStyle() || isElementForFormatBlock(*node))
            return node;
        if (isBlock(*node))
            lastBlock = node;
        if (isListHTMLElement(node.get())) {
            if (parentNode->hasEditableStyle())
                return parentNode;
            return node;
        }
    }
    return lastBlock;
}

}
