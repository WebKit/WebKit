/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Highlight.h"

#include "ComposedTreeIterator.h"
#include "Element.h"
#include "IDLTypes.h"
#include "JSDOMSetLike.h"
#include "JSStaticRange.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RenderBlockFlow.h"
#include "StaticRange.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

void Highlight::repaintRange(const AbstractRange& range)
{
    auto sortedRange = makeSimpleRange(range);
    if (is_gt(treeOrder<ComposedTree>(sortedRange.start, sortedRange.end)))
        std::swap(sortedRange.start, sortedRange.end);
    // Highlight decorations can paint outside a renderer's ink overflow, so also repaint containing blocks (collected to avoid redundant repaints of shared containers).
    SingleThreadWeakHashSet<RenderBlock> containingBlocks;
    for (Ref node : intersectingNodes(sortedRange)) {
        if (CheckedPtr renderer = node->renderer()) {
            renderer->repaint();
            if (CheckedPtr containingBlock = renderer->containingBlock())
                containingBlocks.add(*containingBlock);
        }
    }
    for (CheckedRef containingBlock : containingBlocks)
        containingBlock->repaint();
}

Ref<Highlight> Highlight::create(FixedVector<std::reference_wrapper<AbstractRange>>&& initialRanges)
{
    return adoptRef(*new Highlight(WTF::move(initialRanges)));
}

Highlight::Highlight(FixedVector<std::reference_wrapper<AbstractRange>>&& initialRanges)
{
    m_highlightRanges = WTF::map(initialRanges, [&](std::reference_wrapper<AbstractRange>& range) {
        return HighlightRange::create(range.get());
    });
}

void Highlight::initializeSetLike(DOMSetAdapter& set)
{
    for (auto& highlightRange : m_highlightRanges)
        set.add<IDLInterface<AbstractRange>>(highlightRange->range());
}

bool Highlight::removeFromSetLike(const AbstractRange& range)
{
    invalidateHighlightRangesForNode();
    return m_highlightRanges.removeFirstMatching([&range](const Ref<HighlightRange>& current) {
        repaintRange(range);
        return &current->range() == &range;
    });
}

void Highlight::clearFromSetLike()
{
    invalidateHighlightRangesForNode();
    for (auto& highlightRange : std::exchange(m_highlightRanges, { }))
        repaintRange(highlightRange->range());
}

bool Highlight::addToSetLike(AbstractRange& range)
{
    invalidateHighlightRangesForNode();
    auto index = m_highlightRanges.findIf([&range](const Ref<HighlightRange>& current) {
        return &current->range() == &range;
    });
    if (index == notFound) {
        repaintRange(range);
        m_highlightRanges.append(HighlightRange::create(range));
        return true;
    }
    // Move to last since SetLike is an ordered set.
    m_highlightRanges.append(WTF::move(m_highlightRanges[index]));
    m_highlightRanges.removeAt(index);
    return false;
}

void Highlight::repaint()
{
    for (auto& highlightRange : m_highlightRanges)
        repaintRange(highlightRange->range());
}

void Highlight::setAllRangesNeedPositionUpdate()
{
    for (auto& highlightRange : m_highlightRanges)
        highlightRange->setNeedsPositionUpdate();
}

void Highlight::invalidateHighlightRangesForNode()
{
    m_hasValidHighlightRangesForNode = false;
    m_highlightRangesForNode.clear();
}

void Highlight::rebuildHighlightRangesForNode()
{
    m_highlightRangesForNode.clear();
    m_hasValidHighlightRangesForNode = true;

    for (auto& highlightRange : m_highlightRanges) {
        auto addHighlightRangeForNode = [&](Node& node) {
            if (!node.renderer())
                return;
            auto addResult = m_highlightRangesForNode.ensure(node, [] {
                return Vector<Ref<HighlightRange>> { };
            });
            auto& rangesForNode = addResult.iterator->value;
            if (rangesForNode.isEmpty() || rangesForNode.last().ptr() != highlightRange.ptr())
                rangesForNode.append(highlightRange);
        };

        auto sortedRange = makeSimpleRange(highlightRange->range());
        if (is_gt(treeOrder<ComposedTree>(sortedRange.start, sortedRange.end)))
            std::swap(sortedRange.start, sortedRange.end);
        for (Ref node : intersectingNodes(sortedRange)) {
            addHighlightRangeForNode(node);
            if (RefPtr host = dynamicDowncast<Element>(node.get()); host && host->shadowRoot()) {
                for (Ref descendant : composedTreeDescendants(*host))
                    addHighlightRangeForNode(descendant);
            }
        }
    }
}

const Vector<Ref<HighlightRange>>& Highlight::highlightRangesFor(const Node& node)
{
    if (!m_hasValidHighlightRangesForNode)
        rebuildHighlightRangesForNode();

    auto it = m_highlightRangesForNode.find(node);
    if (it == m_highlightRangesForNode.end()) {
        static NeverDestroyed<Vector<Ref<HighlightRange>>> emptyVector;
        return emptyVector.get();
    }
    return it->value;
}

void Highlight::setPriority(int priority)
{
    if (m_priority == priority)
        return;
    m_priority = priority;
    repaint();
}

} // namespace WebCore
