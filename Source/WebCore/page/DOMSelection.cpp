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

#include "CommonAtomStrings.h"
#include "Document.h"
#include "Editing.h"
#include "FrameSelection.h"
#include "LocalFrame.h"
#include "Quirks.h"
#include "Range.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StaticRange.h"
#include "TextIterator.h"

namespace WebCore {

static RefPtr<Node> selectionShadowAncestor(LocalFrame& frame)
{
    auto* node = frame.selection().selection().base().anchorNode();
    if (!node || !node->isInShadowTree())
        return nullptr;
    return node->document().ancestorNodeInThisScope(node);
}

DOMSelection::DOMSelection(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

Ref<DOMSelection> DOMSelection::create(LocalDOMWindow& window)
{
    return adoptRef(*new DOMSelection(window));
}

RefPtr<LocalFrame> DOMSelection::frame() const
{
    return LocalDOMWindowProperty::frame();
}

std::optional<SimpleRange> DOMSelection::range() const
{
    auto frame = this->frame();
    if (!frame)
        return std::nullopt;
    auto range = frame->settings().liveRangeSelectionEnabled()
        ? frame->selection().selection().range()
        : frame->selection().selection().firstRange();
    if (!range || range->start.container->isInShadowTree())
        return std::nullopt;
    return range;
}

Position DOMSelection::anchorPosition() const
{
    auto frame = this->frame();
    if (!frame)
        return { };
    if (frame->settings().liveRangeSelectionEnabled())
        return frame->selection().selection().anchor();
    auto& selection = frame->selection().selection();
    return (selection.isBaseFirst() ? selection.start() : selection.end()).parentAnchoredEquivalent();
}

Position DOMSelection::focusPosition() const
{
    auto frame = this->frame();
    if (!frame)
        return { };
    if (frame->settings().liveRangeSelectionEnabled())
        return frame->selection().selection().focus();
    auto& selection = frame->selection().selection();
    return (selection.isBaseFirst() ? selection.end() : selection.start()).parentAnchoredEquivalent();
}

Position DOMSelection::basePosition() const
{
    // FIXME: Remove this once liveRangeSelectionEnabled is always on, since base and anchor should be the same thing.
    auto frame = this->frame();
    if (!frame)
        return { };
    if (frame->settings().liveRangeSelectionEnabled())
        return frame->selection().selection().anchor();
    return frame->selection().selection().base().parentAnchoredEquivalent();
}

Position DOMSelection::extentPosition() const
{
    // FIXME: Remove this once liveRangeSelectionEnabled is always on, since extent and focus should be the same thing.
    auto frame = this->frame();
    if (!frame)
        return { };
    if (frame->settings().liveRangeSelectionEnabled())
        return frame->selection().selection().focus();
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
    if (frame->settings().liveRangeSelectionEnabled()) {
        auto range = frame->selection().selection().range();
        if (!range)
            return "None"_s;
        if (range->collapsed())
            return "Caret"_s;
        return "Range"_s;
    }
    if (selection.isNone())
        return "None"_s;
    if (selection.isCaret())
        return "Caret"_s;
    return "Range"_s;
}

String DOMSelection::direction() const
{
    auto frame = this->frame();
    if (!frame)
        return noneAtom();
    auto& selection = frame->selection().selection();
    if (!selection.isDirectional() || selection.isNone())
        return noneAtom();
    return selection.isBaseFirst() ? "forward"_s : "backward"_s;
}

unsigned DOMSelection::rangeCount() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;
    if (frame->settings().liveRangeSelectionEnabled()) {
        if (frame->selection().associatedLiveRange())
            return 1;
        if (selectionShadowAncestor(*frame))
            return 1;
        return 0;
    }
    return frame->selection().isNone() ? 0 : 1;
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
        if (auto result = Range::checkNodeOffsetPair(*node, offset); result.hasException())
            return result.releaseException();
        if (!(frame->settings().selectionAPIForShadowDOMEnabled() && node->isConnected() && frame->document() == &node->document())
            && &node->rootNode() != frame->document())
            return { };
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
        return Exception { ExceptionCode::InvalidStateError };
    if (frame->settings().liveRangeSelectionEnabled()) {
        selection.disassociateLiveRange();
        selection.moveTo(selection.selection().uncanonicalizedEnd(), Affinity::Downstream);
    } else
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
        return Exception { ExceptionCode::InvalidStateError };
    if (frame->settings().liveRangeSelectionEnabled()) {
        selection.disassociateLiveRange();
        selection.moveTo(selection.selection().uncanonicalizedStart(), Affinity::Downstream);
    } else
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
            return Exception { ExceptionCode::TypeError };
        if (auto result = Range::checkNodeOffsetPair(*baseNode, baseOffset); result.hasException())
            return result.releaseException();
        if (auto result = Range::checkNodeOffsetPair(*extentNode, extentOffset); result.hasException())
            return result.releaseException();
        auto& document = *frame->document();
        if (frame->settings().selectionAPIForShadowDOMEnabled()) {
            if (!document.containsIncludingShadowDOM(baseNode) || !document.containsIncludingShadowDOM(extentNode))
                return { };
        } else {
            if (!document.contains(*baseNode) || !document.contains(*extentNode))
                return { };
        }
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
    FrameSelection::Alteration alter;
    if (equalLettersIgnoringASCIICase(alterString, "extend"_s))
        alter = FrameSelection::Alteration::Extend;
    else if (equalLettersIgnoringASCIICase(alterString, "move"_s))
        alter = FrameSelection::Alteration::Move;
    else
        return;

    SelectionDirection direction;
    if (equalLettersIgnoringASCIICase(directionString, "forward"_s))
        direction = SelectionDirection::Forward;
    else if (equalLettersIgnoringASCIICase(directionString, "backward"_s))
        direction = SelectionDirection::Backward;
    else if (equalLettersIgnoringASCIICase(directionString, "left"_s))
        direction = SelectionDirection::Left;
    else if (equalLettersIgnoringASCIICase(directionString, "right"_s))
        direction = SelectionDirection::Right;
    else
        return;

    TextGranularity granularity;
    if (equalLettersIgnoringASCIICase(granularityString, "character"_s))
        granularity = TextGranularity::CharacterGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "word"_s))
        granularity = TextGranularity::WordGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentence"_s))
        granularity = TextGranularity::SentenceGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "line"_s))
        granularity = TextGranularity::LineGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraph"_s))
        granularity = TextGranularity::ParagraphGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "lineboundary"_s))
        granularity = TextGranularity::LineBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentenceboundary"_s))
        granularity = TextGranularity::SentenceBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraphboundary"_s))
        granularity = TextGranularity::ParagraphBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "documentboundary"_s))
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

    if (rangeCount() < 1 && !(frame->settings().liveRangeSelectionEnabled() && frame->selection().isCaretOrRange()))
        return Exception { ExceptionCode::InvalidStateError, "extend() requires a Range to be added to the Selection"_s };

    if (frame->settings().liveRangeSelectionEnabled()) {
        if (!(frame->settings().selectionAPIForShadowDOMEnabled() && node.isConnected() && frame->document() == &node.document())
            && &node.rootNode() != frame->document())
            return { };
        if (auto result = Range::checkNodeOffsetPair(node, offset); result.hasException())
            return result.releaseException();
        auto& selection = frame->selection();
        auto newSelection = selection.selection();
        newSelection.setExtent(makeContainerOffsetPosition(&node, offset));
        selection.disassociateLiveRange();
        selection.setSelection(newSelection);
    } else {
        if (offset > node.length())
            return Exception { ExceptionCode::IndexSizeError };
        if (!isValidForPosition(&node))
            return { };
        frame->selection().setExtent(makeContainerOffsetPosition(&node, offset), Affinity::Downstream);
    }
    return { };
}

static RefPtr<Range> createLiveRangeBeforeShadowHostWithSelection(LocalFrame& frame)
{
    if (auto shadowAncestor = selectionShadowAncestor(frame))
        return createLiveRange(makeSimpleRange(*makeBoundaryPointBeforeNode(*shadowAncestor)));

    return nullptr;
}

ExceptionOr<Ref<Range>> DOMSelection::getRangeAt(unsigned index)
{
    if (index >= rangeCount())
        return Exception { ExceptionCode::IndexSizeError };
    auto frame = this->frame().releaseNonNull();
    if (frame->settings().liveRangeSelectionEnabled()) {
        if (auto liveRange = frame->selection().associatedLiveRange())
            return liveRange.releaseNonNull();
        return createLiveRangeBeforeShadowHostWithSelection(frame.get()).releaseNonNull();
    }
    if (auto liveRange = createLiveRangeBeforeShadowHostWithSelection(frame.get()))
        return liveRange.releaseNonNull();
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
        return Exception { ExceptionCode::NotFoundError };
    removeAllRanges();
    return { };
}

Vector<Ref<StaticRange>> DOMSelection::getComposedRanges(FixedVector<std::reference_wrapper<ShadowRoot>>&& shadowRoots)
{
    auto frame = this->frame();
    if (!frame)
        return { };
    auto range = frame->selection().selection().range();
    if (!range)
        return { };

    HashSet<Ref<ShadowRoot>> shadowRootSet;
    shadowRootSet.reserveInitialCapacity(shadowRoots.size());
    for (auto& root : shadowRoots)
        shadowRootSet.add(root.get());

    Ref<Node> startNode = range->startContainer();
    unsigned startOffset = range->startOffset();
    while (startNode->isInShadowTree() && !shadowRootSet.contains(startNode->containingShadowRoot())) {
        RefPtr host = startNode->shadowHost();
        ASSERT(host && host->parentNode());
        startNode = *host->parentNode();
        startOffset = host->computeNodeIndex();
    }

    Ref<Node> endNode = range->endContainer();
    unsigned endOffset = range->endOffset();
    while (endNode->isInShadowTree() && !shadowRootSet.contains(endNode->containingShadowRoot())) {
        RefPtr host = endNode->shadowHost();
        ASSERT(host && host->parentNode());
        endNode = *host->parentNode();
        endOffset = host->computeNodeIndex() + 1;
    }

    return { StaticRange::create(SimpleRange { BoundaryPoint { WTFMove(startNode), startOffset }, BoundaryPoint { WTFMove(endNode), endOffset } }) };
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
    // FIXME: The rule implemented here for text nodes is wrong, and was added to work around anomalies caused when we canonicalize selection endpoints.
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

    OptionSet<TextIteratorBehavior> options;
    if (!frame->document()->quirks().needsToCopyUserSelectNoneQuirk())
        options.add(TextIteratorBehavior::IgnoresUserSelectNone);

    if (frame->settings().liveRangeSelectionEnabled()) {
        auto range = frame->selection().selection().range();
        return range ? plainText(*range, options) : emptyString();
    }
    auto range = frame->selection().selection().firstRange();
    return range ? plainText(*range, options) : emptyString();
}

RefPtr<Node> DOMSelection::shadowAdjustedNode(const Position& position) const
{
    if (position.isNull())
        return nullptr;

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
