/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
#include "Settings.h"
#include "TextIterator.h"

namespace WebCore {

static RefPtr<Node> selectionShadowAncestor(Frame& frame)
{
    ASSERT(!frame.settings().liveRangeSelectionEnabled());
    auto* node = frame.selection().selection().base().anchorNode();
    if (!node || !node->isInShadowTree())
        return nullptr;
    return node->document().ancestorNodeInThisScope(node);
}

DOMSelection::DOMSelection(DOMWindow& window)
    : DOMWindowProperty(&window)
{
}

Ref<DOMSelection> DOMSelection::create(DOMWindow& window)
{
    return adoptRef(*new DOMSelection(window));
}

RefPtr<Frame> DOMSelection::frame() const
{
    return DOMWindowProperty::frame();
}

Optional<SimpleRange> DOMSelection::range() const
{
    auto frame = this->frame();
    if (!frame)
        return WTF::nullopt;
    auto range = frame->selection().selection().firstRange();
    if (!range || range->start.container->isInShadowTree())
        return WTF::nullopt;
    return range;
}

Position DOMSelection::anchorPosition() const
{
    auto frame = this->frame();
    if (!frame)
        return { };
    auto& selection = frame->selection().selection();
    return (selection.isBaseFirst() ? selection.start() : selection.end()).parentAnchoredEquivalent();
}

Position DOMSelection::focusPosition() const
{
    auto frame = this->frame();
    if (!frame)
        return { };
    auto& selection = frame->selection().selection();
    return (selection.isBaseFirst() ? selection.end() : selection.start()).parentAnchoredEquivalent();
}

Position DOMSelection::basePosition() const
{
    auto frame = this->frame();
    if (!frame)
        return { };
    return frame->selection().selection().base().parentAnchoredEquivalent();
}

Position DOMSelection::extentPosition() const
{
    auto frame = this->frame();
    if (!frame)
        return { };
    return frame->selection().selection().extent().parentAnchoredEquivalent();
}

RefPtr<Node> DOMSelection::anchorNode() const
{
    return shadowAdjustedNode(anchorPosition());
}

unsigned DOMSelection::anchorOffset() const
{
    return shadowAdjustedOffset(anchorPosition());
}

RefPtr<Node> DOMSelection::focusNode() const
{
    return shadowAdjustedNode(focusPosition());
}

unsigned DOMSelection::focusOffset() const
{
    return shadowAdjustedOffset(focusPosition());
}

RefPtr<Node> DOMSelection::baseNode() const
{
    return shadowAdjustedNode(basePosition());
}

unsigned DOMSelection::baseOffset() const
{
    return shadowAdjustedOffset(basePosition());
}

RefPtr<Node> DOMSelection::extentNode() const
{
    return shadowAdjustedNode(extentPosition());
}

unsigned DOMSelection::extentOffset() const
{
    return shadowAdjustedOffset(extentPosition());
}

bool DOMSelection::isCollapsed() const
{
    auto frame = this->frame();
    if (!frame)
        return true;
    auto range = this->range();
    return !range || range->collapsed();
}

String DOMSelection::type() const
{
    auto frame = this->frame();
    if (!frame)
        return "None"_s;
    auto& selection = frame->selection();
    if (selection.isNone())
        return "None"_s;
    if (selection.isCaret())
        return "Caret"_s;
    return "Range"_s;
}

unsigned DOMSelection::rangeCount() const
{
    auto frame = this->frame();
    return !frame || frame->selection().isNone() ? 0 : 1;
}

ExceptionOr<void> DOMSelection::collapse(Node* node, unsigned offset)
{
    auto frame = this->frame();
    if (!frame)
        return { };
    if (frame->settings().liveRangeSelectionEnabled()) {
        if (!node) {
            removeAllRanges();
            return { };
        }
        auto& document = *frame->document();
        if (!document.contains(*node))
            return { };
        if (auto result = Range::checkNodeOffsetPair(*node, offset); result.hasException())
            return result.releaseException();
    } else {
        if (!isValidForPosition(node))
            return { };
    }
    auto& selection = frame->selection();
    selection.disassociateLiveRange();
    selection.moveTo(makeContainerOffsetPosition(node, offset), Affinity::Downstream);
    return { };
}

ExceptionOr<void> DOMSelection::collapseToEnd()
{
    auto frame = this->frame();
    if (!frame)
        return { };
    auto& selection = frame->selection();
    if (selection.isNone())
        return Exception { InvalidStateError };
    selection.disassociateLiveRange();
    selection.moveTo(selection.selection().end(), Affinity::Downstream);
    return { };
}

ExceptionOr<void> DOMSelection::collapseToStart()
{
    auto frame = this->frame();
    if (!frame)
        return { };
    auto& selection = frame->selection();
    if (selection.isNone())
        return Exception { InvalidStateError };
    selection.disassociateLiveRange();
    selection.moveTo(selection.selection().start(), Affinity::Downstream);
    return { };
}

void DOMSelection::empty()
{
    removeAllRanges();
}

ExceptionOr<void> DOMSelection::setBaseAndExtent(Node* baseNode, unsigned baseOffset, Node* extentNode, unsigned extentOffset)
{
    auto frame = this->frame();
    if (!frame)
        return { };
    if (frame->settings().liveRangeSelectionEnabled()) {
        // FIXME: We should do this by making the arguments non-nullable in the IDL file, once liveRangeSelectionEnabled is always true.
        if (!baseNode || !extentNode)
            return Exception { TypeError };
        auto& document = *frame->document();
        if (!document.contains(*baseNode) || !document.contains(*extentNode))
            return { };
        if (auto result = Range::checkNodeOffsetPair(*baseNode, baseOffset); result.hasException())
            return result.releaseException();
        if (auto result = Range::checkNodeOffsetPair(*extentNode, extentOffset); result.hasException())
            return result.releaseException();
    } else {
        if (!isValidForPosition(baseNode) || !isValidForPosition(extentNode))
            return { };
    }
    auto& selection = frame->selection();
    selection.disassociateLiveRange();
    selection.moveTo(makeContainerOffsetPosition(baseNode, baseOffset), makeContainerOffsetPosition(extentNode, extentOffset), Affinity::Downstream);
    return { };
}

ExceptionOr<void> DOMSelection::setPosition(Node* node, unsigned offset)
{
    return collapse(node, offset);
}

void DOMSelection::modify(const String& alterString, const String& directionString, const String& granularityString)
{
    FrameSelection::EAlteration alter;
    if (equalLettersIgnoringASCIICase(alterString, "extend"))
        alter = FrameSelection::AlterationExtend;
    else if (equalLettersIgnoringASCIICase(alterString, "move"))
        alter = FrameSelection::AlterationMove;
    else
        return;

    SelectionDirection direction;
    if (equalLettersIgnoringASCIICase(directionString, "forward"))
        direction = SelectionDirection::Forward;
    else if (equalLettersIgnoringASCIICase(directionString, "backward"))
        direction = SelectionDirection::Backward;
    else if (equalLettersIgnoringASCIICase(directionString, "left"))
        direction = SelectionDirection::Left;
    else if (equalLettersIgnoringASCIICase(directionString, "right"))
        direction = SelectionDirection::Right;
    else
        return;

    TextGranularity granularity;
    if (equalLettersIgnoringASCIICase(granularityString, "character"))
        granularity = TextGranularity::CharacterGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "word"))
        granularity = TextGranularity::WordGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentence"))
        granularity = TextGranularity::SentenceGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "line"))
        granularity = TextGranularity::LineGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraph"))
        granularity = TextGranularity::ParagraphGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "lineboundary"))
        granularity = TextGranularity::LineBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentenceboundary"))
        granularity = TextGranularity::SentenceBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraphboundary"))
        granularity = TextGranularity::ParagraphBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "documentboundary"))
        granularity = TextGranularity::DocumentBoundary;
    else
        return;

    if (auto frame = this->frame())
        frame->selection().modify(alter, direction, granularity);
}

ExceptionOr<void> DOMSelection::extend(Node& node, unsigned offset)
{
    auto frame = this->frame();
    if (!frame)
        return { };
    if (frame->settings().liveRangeSelectionEnabled()) {
        if (!frame->document()->contains(node))
            return { };
        if (auto result = Range::checkNodeOffsetPair(node, offset); result.hasException())
            return result.releaseException();
    } else {
        if (offset > node.length())
            return Exception { IndexSizeError };
        if (!isValidForPosition(&node))
            return { };
    }
    auto& selection = frame->selection();
    selection.disassociateLiveRange();
    selection.setExtent(makeContainerOffsetPosition(&node, offset), Affinity::Downstream);
    return { };
}

ExceptionOr<Ref<Range>> DOMSelection::getRangeAt(unsigned index)
{
    if (index >= rangeCount())
        return Exception { IndexSizeError };
    auto frame = this->frame().releaseNonNull();
    if (frame->settings().liveRangeSelectionEnabled())
        return frame->selection().associatedLiveRange().releaseNonNull();
    if (auto shadowAncestor = selectionShadowAncestor(frame))
        return createLiveRange(makeSimpleRange(*makeBoundaryPointBeforeNode(*shadowAncestor)));
    return createLiveRange(*frame->selection().selection().firstRange());
}

void DOMSelection::removeAllRanges()
{
    auto frame = this->frame();
    if (!frame)
        return;
    frame->selection().clear();
}

void DOMSelection::addRange(Range& liveRange)
{
    auto frame = this->frame();
    if (!frame)
        return;
    auto& selection = frame->selection();
    if (frame->settings().liveRangeSelectionEnabled()) {
        if (selection.isNone())
            selection.associateLiveRange(liveRange);
        return;
    }
    auto range = makeSimpleRange(liveRange);
    if (auto selectedRange = selection.selection().toNormalizedRange()) {
        if (!selectedRange->start.container->containingShadowRoot() && intersects(*selectedRange, range))
            selection.setSelection(unionRange(*selectedRange, range));
        return;
    }
    selection.setSelection(range);
}

ExceptionOr<void> DOMSelection::removeRange(Range& liveRange)
{
    auto frame = this->frame();
    if (!frame)
        return { };
    ASSERT(frame->settings().liveRangeSelectionEnabled());
    if (&liveRange != frame->selection().associatedLiveRange())
        return Exception { NotFoundError };
    removeAllRanges();
    return { };
}

void DOMSelection::deleteFromDocument()
{
    auto frame = this->frame();
    if (!frame)
        return;
    if (frame->settings().liveRangeSelectionEnabled()) {
        if (auto range = frame->selection().associatedLiveRange())
            range->deleteContents();
        return;
    }
    auto selectedRange = frame->selection().selection().toNormalizedRange();
    if (!selectedRange || selectedRange->start.container->containingShadowRoot())
        return;
    createLiveRange(*selectedRange)->deleteContents();
    frame->selection().setSelectedRange(makeSimpleRange(selectedRange->start), Affinity::Downstream, FrameSelection::ShouldCloseTyping::No);
}

bool DOMSelection::containsNode(Node& node, bool allowPartial) const
{
    // FIXME: This is wrong, and was added to work around anomalies caused when we canonicalize selection endpoints. We should fix that and remove this.
    if (node.isTextNode() && !node.document().settings().liveRangeSelectionEnabled())
        allowPartial = true;
    auto range = this->range();
    return range && (allowPartial ? intersects(*range, node) : contains(*range, node));
}

ExceptionOr<void> DOMSelection::selectAllChildren(Node& node)
{
    // This doesn't (and shouldn't) select the characters in a node if passed a text node.
    // Selection API specification seems to have this wrong: https://github.com/w3c/selection-api/issues/125
    return setBaseAndExtent(&node, 0, &node, node.countChildNodes());
}

String DOMSelection::toString() const
{
    auto frame = this->frame();
    if (!frame)
        return String();
    if (frame->settings().liveRangeSelectionEnabled()) {
        auto range = this->range();
        return range ? plainText(*range) : emptyString();
    }
    auto range = frame->selection().selection().firstRange();
    return range ? plainText(*range) : emptyString();
}

RefPtr<Node> DOMSelection::shadowAdjustedNode(const Position& position) const
{
    if (position.isNull())
        return nullptr;

    if (frame()->settings().liveRangeSelectionEnabled()) {
        auto node = position.containerNode();
        return !node || node->isInShadowTree() ? nullptr : node;
    }

    auto* containerNode = position.containerNode();
    auto* adjustedNode = frame()->document()->ancestorNodeInThisScope(containerNode);
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

    if (frame()->settings().liveRangeSelectionEnabled())
        return shadowAdjustedNode(position) ? position.computeOffsetInContainerNode() : 0;

    auto* containerNode = position.containerNode();
    auto* adjustedNode = frame()->document()->ancestorNodeInThisScope(containerNode);
    if (!adjustedNode)
        return 0;

    if (containerNode == adjustedNode)
        return position.computeOffsetInContainerNode();

    return adjustedNode->computeNodeIndex();
}

bool DOMSelection::isValidForPosition(Node* node) const
{
    auto frame = this->frame();
    ASSERT(!frame->settings().liveRangeSelectionEnabled());
    return frame && (!node || &node->document() == frame->document());
}

} // namespace WebCore
