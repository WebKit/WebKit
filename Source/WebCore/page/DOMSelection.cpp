/*
 * Copyright (C) 2007, 2009, 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "DOMSelection.h"

#include "Document.h"
#include "Editing.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "Range.h"
#include "TextIterator.h"

namespace WebCore {

static Node* selectionShadowAncestor(Frame& frame)
{
    auto* node = frame.selection().selection().base().anchorNode();
    if (!node)
        return nullptr;
    if (!node->isInShadowTree())
        return nullptr;
    // FIXME: Unclear on why this needs to be the possibly null frame.document() instead of the never null node->document().
    return frame.document()->ancestorNodeInThisScope(node);
}

DOMSelection::DOMSelection(Frame& frame)
    : DOMWindowProperty(&frame)
{
}

const VisibleSelection& DOMSelection::visibleSelection() const
{
    ASSERT(m_frame);
    return m_frame->selection().selection();
}

static Position anchorPosition(const VisibleSelection& selection)
{
    auto anchor = selection.isBaseFirst() ? selection.start() : selection.end();
    return anchor.parentAnchoredEquivalent();
}

static Position focusPosition(const VisibleSelection& selection)
{
    auto focus = selection.isBaseFirst() ? selection.end() : selection.start();
    return focus.parentAnchoredEquivalent();
}

static Position basePosition(const VisibleSelection& selection)
{
    return selection.base().parentAnchoredEquivalent();
}

static Position extentPosition(const VisibleSelection& selection)
{
    return selection.extent().parentAnchoredEquivalent();
}

Node* DOMSelection::anchorNode() const
{
    if (!m_frame)
        return 0;
    return shadowAdjustedNode(anchorPosition(visibleSelection()));
}

unsigned DOMSelection::anchorOffset() const
{
    if (!m_frame)
        return 0;
    return shadowAdjustedOffset(anchorPosition(visibleSelection()));
}

Node* DOMSelection::focusNode() const
{
    if (!m_frame)
        return nullptr;
    return shadowAdjustedNode(focusPosition(visibleSelection()));
}

unsigned DOMSelection::focusOffset() const
{
    if (!m_frame)
        return 0;
    return shadowAdjustedOffset(focusPosition(visibleSelection()));
}

Node* DOMSelection::baseNode() const
{
    if (!m_frame)
        return 0;
    return shadowAdjustedNode(basePosition(visibleSelection()));
}

unsigned DOMSelection::baseOffset() const
{
    if (!m_frame)
        return 0;
    return shadowAdjustedOffset(basePosition(visibleSelection()));
}

Node* DOMSelection::extentNode() const
{
    if (!m_frame)
        return 0;
    return shadowAdjustedNode(extentPosition(visibleSelection()));
}

unsigned DOMSelection::extentOffset() const
{
    if (!m_frame)
        return 0;
    return shadowAdjustedOffset(extentPosition(visibleSelection()));
}

bool DOMSelection::isCollapsed() const
{
    if (!m_frame || selectionShadowAncestor(*m_frame))
        return true;
    return !m_frame->selection().isRange();
}

String DOMSelection::type() const
{
    if (!m_frame)
        return "None"_s;
    auto& selection = m_frame->selection();
    if (selection.isNone())
        return "None"_s;
    if (selection.isCaret())
        return "Caret"_s;
    return "Range"_s;
}

unsigned DOMSelection::rangeCount() const
{
    return !m_frame || m_frame->selection().isNone() ? 0 : 1;
}

void DOMSelection::collapse(Node* node, unsigned offset)
{
    if (!isValidForPosition(node))
        return;

    Ref<Frame> protector(*m_frame);
    m_frame->selection().moveTo(createLegacyEditingPosition(node, offset), DOWNSTREAM);
}

ExceptionOr<void> DOMSelection::collapseToEnd()
{
    if (!m_frame)
        return { };
    auto& selection = m_frame->selection();
    if (selection.isNone())
        return Exception { InvalidStateError };

    Ref<Frame> protector(*m_frame);
    selection.moveTo(selection.selection().end(), DOWNSTREAM);
    return { };
}

ExceptionOr<void> DOMSelection::collapseToStart()
{
    if (!m_frame)
        return { };
    auto& selection = m_frame->selection();
    if (selection.isNone())
        return Exception { InvalidStateError };

    Ref<Frame> protector(*m_frame);
    selection.moveTo(selection.selection().start(), DOWNSTREAM);
    return { };
}

void DOMSelection::empty()
{
    if (!m_frame)
        return;
    m_frame->selection().clear();
}

void DOMSelection::setBaseAndExtent(Node* baseNode, unsigned baseOffset, Node* extentNode, unsigned extentOffset)
{
    if (!isValidForPosition(baseNode) || !isValidForPosition(extentNode))
        return;

    Ref<Frame> protector(*m_frame);
    m_frame->selection().moveTo(createLegacyEditingPosition(baseNode, baseOffset), createLegacyEditingPosition(extentNode, extentOffset), DOWNSTREAM);
}

void DOMSelection::setPosition(Node* node, unsigned offset)
{
    if (!isValidForPosition(node))
        return;

    Ref<Frame> protector(*m_frame);
    m_frame->selection().moveTo(createLegacyEditingPosition(node, offset), DOWNSTREAM);
}

void DOMSelection::modify(const String& alterString, const String& directionString, const String& granularityString)
{
    if (!m_frame)
        return;

    FrameSelection::EAlteration alter;
    if (equalLettersIgnoringASCIICase(alterString, "extend"))
        alter = FrameSelection::AlterationExtend;
    else if (equalLettersIgnoringASCIICase(alterString, "move"))
        alter = FrameSelection::AlterationMove;
    else
        return;

    SelectionDirection direction;
    if (equalLettersIgnoringASCIICase(directionString, "forward"))
        direction = DirectionForward;
    else if (equalLettersIgnoringASCIICase(directionString, "backward"))
        direction = DirectionBackward;
    else if (equalLettersIgnoringASCIICase(directionString, "left"))
        direction = DirectionLeft;
    else if (equalLettersIgnoringASCIICase(directionString, "right"))
        direction = DirectionRight;
    else
        return;

    TextGranularity granularity;
    if (equalLettersIgnoringASCIICase(granularityString, "character"))
        granularity = CharacterGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "word"))
        granularity = WordGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentence"))
        granularity = SentenceGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "line"))
        granularity = LineGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraph"))
        granularity = ParagraphGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "lineboundary"))
        granularity = LineBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentenceboundary"))
        granularity = SentenceBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraphboundary"))
        granularity = ParagraphBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "documentboundary"))
        granularity = DocumentBoundary;
    else
        return;

    Ref<Frame> protector(*m_frame);
    m_frame->selection().modify(alter, direction, granularity);
}

ExceptionOr<void> DOMSelection::extend(Node& node, unsigned offset)
{
    if (!m_frame)
        return { };
    if (offset > (node.isCharacterDataNode() ? caretMaxOffset(node) : node.countChildNodes()))
        return Exception { IndexSizeError };
    if (!isValidForPosition(&node))
        return { };

    Ref<Frame> protector(*m_frame);
    m_frame->selection().setExtent(createLegacyEditingPosition(&node, offset), DOWNSTREAM);
    return { };
}

ExceptionOr<Ref<Range>> DOMSelection::getRangeAt(unsigned index)
{
    if (index >= rangeCount())
        return Exception { IndexSizeError };

    // If you're hitting this, you've added broken multi-range selection support.
    ASSERT(rangeCount() == 1);

    if (auto* shadowAncestor = selectionShadowAncestor(*m_frame)) {
        auto* container = shadowAncestor->parentNodeGuaranteedHostFree();
        unsigned offset = shadowAncestor->computeNodeIndex();
        return Range::create(shadowAncestor->document(), container, offset, container, offset);
    }

    auto firstRange = m_frame->selection().selection().firstRange();
    ASSERT(firstRange);
    if (!firstRange)
        return Exception { IndexSizeError };
    return firstRange.releaseNonNull();
}

void DOMSelection::removeAllRanges()
{
    if (!m_frame)
        return;
    m_frame->selection().clear();
}

void DOMSelection::addRange(Range& range)
{
    if (!m_frame)
        return;

    Ref<Frame> protector(*m_frame);

    auto& selection = m_frame->selection();
    if (selection.isNone()) {
        selection.moveTo(&range);
        return;
    }

    auto normalizedRange = selection.selection().toNormalizedRange();
    if (!normalizedRange)
        return;

    auto result = range.compareBoundaryPoints(Range::START_TO_START, *normalizedRange);
    if (!result.hasException() && result.releaseReturnValue() == -1) {
        // We don't support discontiguous selection. We don't do anything if the two ranges don't intersect.
        result = range.compareBoundaryPoints(Range::START_TO_END, *normalizedRange);
        if (!result.hasException() && result.releaseReturnValue() > -1) {
            result = range.compareBoundaryPoints(Range::END_TO_END, *normalizedRange);
            if (!result.hasException() && result.releaseReturnValue() == -1) {
                // The ranges intersect.
                selection.moveTo(range.startPosition(), normalizedRange->endPosition(), DOWNSTREAM);
            } else {
                // The new range contains the original range.
                selection.moveTo(&range);
            }
        }
    } else {
        // We don't support discontiguous selection. We don't do anything if the two ranges don't intersect.
        result = range.compareBoundaryPoints(Range::END_TO_START, *normalizedRange);
        if (!result.hasException() && result.releaseReturnValue() < 1) {
            result = range.compareBoundaryPoints(Range::END_TO_END, *normalizedRange);
            if (!result.hasException() && result.releaseReturnValue() == -1) {
                // The original range contains the new range.
                selection.moveTo(normalizedRange.get());
            } else {
                // The ranges intersect.
                selection.moveTo(normalizedRange->startPosition(), range.endPosition(), DOWNSTREAM);
            }
        }
    }
}

void DOMSelection::deleteFromDocument()
{
    if (!m_frame)
        return;

    auto& selection = m_frame->selection();
    if (selection.isNone())
        return;

    auto selectedRange = selection.selection().toNormalizedRange();
    if (!selectedRange || selectedRange->shadowRoot())
        return;

    Ref<Frame> protector(*m_frame);
    selectedRange->deleteContents();
    setBaseAndExtent(&selectedRange->startContainer(), selectedRange->startOffset(), &selectedRange->startContainer(), selectedRange->startOffset());
}

bool DOMSelection::containsNode(Node& node, bool allowPartial) const
{
    if (!m_frame)
        return false;

    auto& selection = m_frame->selection();
    if (m_frame->document() != &node.document() || selection.isNone())
        return false;

    Ref<Node> protectedNode(node);
    auto selectedRange = selection.selection().toNormalizedRange();
    if (!selectedRange)
        return false;

    ContainerNode* parentNode = node.parentNode();
    if (!parentNode || !parentNode->isConnected())
        return false;
    unsigned nodeIndex = node.computeNodeIndex();

    auto startsResult = Range::compareBoundaryPoints(parentNode, nodeIndex, &selectedRange->startContainer(), selectedRange->startOffset());
    ASSERT(!startsResult.hasException());
    auto endsResult = Range::compareBoundaryPoints(parentNode, nodeIndex + 1, &selectedRange->endContainer(), selectedRange->endOffset());
    ASSERT(!endsResult.hasException());
    bool isNodeFullySelected = !startsResult.hasException() && startsResult.releaseReturnValue() >= 0
        && !endsResult.hasException() && endsResult.releaseReturnValue() <= 0;
    if (isNodeFullySelected)
        return true;

    auto startEndResult = Range::compareBoundaryPoints(parentNode, nodeIndex, &selectedRange->endContainer(), selectedRange->endOffset());
    ASSERT(!startEndResult.hasException());
    auto endStartResult = Range::compareBoundaryPoints(parentNode, nodeIndex + 1, &selectedRange->startContainer(), selectedRange->startOffset());
    ASSERT(!endStartResult.hasException());
    bool isNodeFullyUnselected = (!startEndResult.hasException() && startEndResult.releaseReturnValue() > 0)
        || (!endStartResult.hasException() && endStartResult.releaseReturnValue() < 0);
    if (isNodeFullyUnselected)
        return false;

    return allowPartial || node.isTextNode();
}

void DOMSelection::selectAllChildren(Node& node)
{
    // This doesn't (and shouldn't) select text node characters.
    setBaseAndExtent(&node, 0, &node, node.countChildNodes());
}

String DOMSelection::toString()
{
    if (!m_frame)
        return String();
    return plainText(m_frame->selection().selection().toNormalizedRange().get());
}

Node* DOMSelection::shadowAdjustedNode(const Position& position) const
{
    if (position.isNull())
        return nullptr;

    auto* containerNode = position.containerNode();
    auto* adjustedNode = m_frame->document()->ancestorNodeInThisScope(containerNode);
    if (!adjustedNode)
        return nullptr;

    if (containerNode == adjustedNode)
        return containerNode;

    return adjustedNode->parentNodeGuaranteedHostFree();
}

unsigned DOMSelection::shadowAdjustedOffset(const Position& position) const
{
    if (position.isNull())
        return 0;

    auto* containerNode = position.containerNode();
    auto* adjustedNode = m_frame->document()->ancestorNodeInThisScope(containerNode);
    if (!adjustedNode)
        return 0;

    if (containerNode == adjustedNode)
        return position.computeOffsetInContainerNode();

    return adjustedNode->computeNodeIndex();
}

bool DOMSelection::isValidForPosition(Node* node) const
{
    if (!m_frame)
        return false;
    if (!node)
        return true;
    return &node->document() == m_frame->document();
}

} // namespace WebCore
