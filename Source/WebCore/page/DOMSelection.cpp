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

DOMSelection::DOMSelection(DOMWindow& window)
    : DOMWindowProperty(&window)
{
}

const VisibleSelection& DOMSelection::visibleSelection() const
{
    ASSERT(frame());
    return frame()->selection().selection();
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
    if (!frame())
        return nullptr;
    return shadowAdjustedNode(anchorPosition(visibleSelection()));
}

unsigned DOMSelection::anchorOffset() const
{
    if (!frame())
        return 0;
    return shadowAdjustedOffset(anchorPosition(visibleSelection()));
}

Node* DOMSelection::focusNode() const
{
    if (!frame())
        return nullptr;
    return shadowAdjustedNode(focusPosition(visibleSelection()));
}

unsigned DOMSelection::focusOffset() const
{
    if (!frame())
        return 0;
    return shadowAdjustedOffset(focusPosition(visibleSelection()));
}

Node* DOMSelection::baseNode() const
{
    if (!frame())
        return nullptr;
    return shadowAdjustedNode(basePosition(visibleSelection()));
}

unsigned DOMSelection::baseOffset() const
{
    if (!frame())
        return 0;
    return shadowAdjustedOffset(basePosition(visibleSelection()));
}

Node* DOMSelection::extentNode() const
{
    if (!frame())
        return nullptr;
    return shadowAdjustedNode(extentPosition(visibleSelection()));
}

unsigned DOMSelection::extentOffset() const
{
    if (!frame())
        return 0;
    return shadowAdjustedOffset(extentPosition(visibleSelection()));
}

bool DOMSelection::isCollapsed() const
{
    auto* frame = this->frame();
    if (!frame || selectionShadowAncestor(*frame))
        return true;
    return !frame->selection().isRange();
}

String DOMSelection::type() const
{
    auto* frame = this->frame();
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
    auto* frame = this->frame();
    return !frame || frame->selection().isNone() ? 0 : 1;
}

void DOMSelection::collapse(Node* node, unsigned offset)
{
    if (!isValidForPosition(node))
        return;

    Ref<Frame> protectedFrame(*frame());
    protectedFrame->selection().moveTo(createLegacyEditingPosition(node, offset), DOWNSTREAM);
}

ExceptionOr<void> DOMSelection::collapseToEnd()
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    auto& selection = frame->selection();
    if (selection.isNone())
        return Exception { InvalidStateError };

    Ref<Frame> protector(*frame);
    selection.moveTo(selection.selection().end(), DOWNSTREAM);
    return { };
}

ExceptionOr<void> DOMSelection::collapseToStart()
{
    auto* frame = this->frame();
    if (!frame)
        return { };
    auto& selection = frame->selection();
    if (selection.isNone())
        return Exception { InvalidStateError };

    Ref<Frame> protector(*frame);
    selection.moveTo(selection.selection().start(), DOWNSTREAM);
    return { };
}

void DOMSelection::empty()
{
    auto* frame = this->frame();
    if (!frame)
        return;
    frame->selection().clear();
}

void DOMSelection::setBaseAndExtent(Node* baseNode, unsigned baseOffset, Node* extentNode, unsigned extentOffset)
{
    if (!isValidForPosition(baseNode) || !isValidForPosition(extentNode))
        return;

    Ref<Frame> protectedFrame(*frame());
    protectedFrame->selection().moveTo(createLegacyEditingPosition(baseNode, baseOffset), createLegacyEditingPosition(extentNode, extentOffset), DOWNSTREAM);
}

void DOMSelection::setPosition(Node* node, unsigned offset)
{
    if (!isValidForPosition(node))
        return;

    Ref<Frame> protectedFrame(*frame());
    protectedFrame->selection().moveTo(createLegacyEditingPosition(node, offset), DOWNSTREAM);
}

void DOMSelection::modify(const String& alterString, const String& directionString, const String& granularityString)
{
    auto* frame = this->frame();
    if (!frame)
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

    Ref<Frame> protector(*frame);
    frame->selection().modify(alter, direction, granularity);
}

ExceptionOr<void> DOMSelection::extend(Node& node, unsigned offset)
{
    auto frame = makeRefPtr(this->frame());
    if (!frame)
        return { };
    if (offset > node.length())
        return Exception { IndexSizeError };
    if (!isValidForPosition(&node))
        return { };
    frame->selection().setExtent(createLegacyEditingPosition(&node, offset), DOWNSTREAM);
    return { };
}

ExceptionOr<Ref<Range>> DOMSelection::getRangeAt(unsigned index)
{
    if (index >= rangeCount())
        return Exception { IndexSizeError };
    auto& frame = *this->frame();
    if (auto shadowAncestor = selectionShadowAncestor(frame))
        return createLiveRange(makeSimpleRange(*makeBoundaryPointBeforeNode(*shadowAncestor)));
    return createLiveRange(*frame.selection().selection().firstRange());
}

void DOMSelection::removeAllRanges()
{
    auto* frame = this->frame();
    if (!frame)
        return;
    frame->selection().clear();
}

void DOMSelection::addRange(Range& liveRange)
{
    auto frame = makeRefPtr(this->frame());
    if (!frame)
        return;

    auto range = makeSimpleRange(liveRange);
    auto& selection = frame->selection();

    if (auto selectedRange = selection.selection().toNormalizedRange()) {
        if (!selectedRange->start.container->containingShadowRoot() && intersects(*selectedRange, range))
            selection.setSelection(unionRange(*selectedRange, range));
        return;
    }

    selection.setSelection(range);
}

void DOMSelection::deleteFromDocument()
{
    auto* frame = this->frame();
    if (!frame)
        return;

    auto selectedRange = frame->selection().selection().toNormalizedRange();
    if (!selectedRange || selectedRange->start.container->containingShadowRoot())
        return;

    Ref<Frame> protector(*frame);
    createLiveRange(*selectedRange)->deleteContents();
    auto container = selectedRange->start.container.ptr();
    auto offset = selectedRange->start.offset;
    setBaseAndExtent(container, offset, container, offset);
}

bool DOMSelection::containsNode(Node& node, bool allowPartial) const
{
    // FIXME: This behavior does not match what the selection API standard specifies.
    if (node.isTextNode())
        allowPartial = true;

    auto* frame = this->frame();
    if (!frame)
        return false;

    auto selectedRange = frame->selection().selection().firstRange();
    if (!selectedRange || selectedRange->start.container->containingShadowRoot())
        return false;

    return allowPartial ? intersects(*selectedRange, node) : contains(*selectedRange, node);
}

void DOMSelection::selectAllChildren(Node& node)
{
    // This doesn't (and shouldn't) select text node characters.
    setBaseAndExtent(&node, 0, &node, node.countChildNodes());
}

String DOMSelection::toString()
{
    auto* frame = this->frame();
    if (!frame)
        return String();
    auto range = frame->selection().selection().toNormalizedRange();
    return range ? plainText(*range) : emptyString();
}

Node* DOMSelection::shadowAdjustedNode(const Position& position) const
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
    auto* frame = this->frame();
    return frame && (!node || &node->document() == frame->document());
}

} // namespace WebCore
