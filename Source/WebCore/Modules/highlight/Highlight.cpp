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

#include "IDLTypes.h"
#include "JSDOMSetLike.h"
#include "JSStaticRange.h"
#include "NodeTraversal.h"
#include "Range.h"
#include "RenderBlockFlow.h"
#include "StaticRange.h"

namespace WebCore {

void Highlight::repaintRange(const AbstractRange& range)
{
    auto sortedRange = makeSimpleRange(range);
    if (is_gt(treeOrder<ComposedTree>(sortedRange.start, sortedRange.end)))
        std::swap(sortedRange.start, sortedRange.end);
    for (Ref node : intersectingNodes(sortedRange)) {
        if (auto renderer = node->renderer())
            renderer->repaint();
    }
}

Ref<Highlight> Highlight::create(FixedVector<std::reference_wrapper<WebCore::AbstractRange>>&& initialRanges)
{
    return adoptRef(*new Highlight(WTFMove(initialRanges)));
}

Highlight::Highlight(FixedVector<std::reference_wrapper<WebCore::AbstractRange>>&& initialRanges)
{
    m_highlightRanges = WTF::map(initialRanges, [&](auto&& range) {
        repaintRange(range.get());
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
    return m_highlightRanges.removeFirstMatching([&range](const Ref<HighlightRange>& current) {
        repaintRange(range);
        return &current->range() == &range;
    });
}

void Highlight::clearFromSetLike()
{
    for (auto& highlightRange : m_highlightRanges)
        repaintRange(highlightRange->range());
    m_highlightRanges.clear();
}

bool Highlight::addToSetLike(AbstractRange& range)
{
    auto index = m_highlightRanges.findIf([&range](const Ref<HighlightRange>& current) {
        return &current->range() == &range;
    });
    if (index == notFound) {
        repaintRange(range);
        m_highlightRanges.append(HighlightRange::create(range));
        return true;
    }
    // Move to last since SetLike is an ordered set.
    m_highlightRanges.append(WTFMove(m_highlightRanges[index]));
    m_highlightRanges.remove(index);
    return false;
}

void Highlight::repaint()
{
    for (auto& highlightRange : m_highlightRanges)
        repaintRange(highlightRange->range());
}

void Highlight::setPriority(int priority)
{
    if (m_priority == priority)
        return;
    m_priority = priority;
    repaint();
}

} // namespace WebCore
