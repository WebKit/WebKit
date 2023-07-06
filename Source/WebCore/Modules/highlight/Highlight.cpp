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

static void repaintRange(const AbstractRange& range)
{
    // FIXME: Unclear precisely why we need to handle out of order cases here, but not unordered cases.
    SimpleRange sortedRange = makeSimpleRange(range);
    if (is_gt(treeOrder<ComposedTree>(sortedRange.start, sortedRange.end)))
        std::swap(sortedRange.start, sortedRange.end);
    for (auto& node : intersectingNodes(sortedRange)) {
        if (auto renderer = node.renderer())
            renderer->repaint();
    }
}

Ref<Highlight> Highlight::create(FixedVector<std::reference_wrapper<WebCore::AbstractRange>>&& initialRanges)
{
    return adoptRef(*new Highlight(WTFMove(initialRanges)));
}

Highlight::Highlight(FixedVector<std::reference_wrapper<WebCore::AbstractRange>>&& initialRanges)
{
    m_rangesData.reserveInitialCapacity(initialRanges.size());
    for (auto& range : initialRanges) {
        repaintRange(range.get());
        m_rangesData.uncheckedAppend(HighlightRangeData::create(Ref { range.get() }));
    }
}

void Highlight::initializeSetLike(DOMSetAdapter& set)
{
    for (auto& rangeData : m_rangesData)
        set.add<IDLInterface<AbstractRange>>(rangeData->range());
}

bool Highlight::removeFromSetLike(const AbstractRange& range)
{
    return m_rangesData.removeFirstMatching([&range](const Ref<HighlightRangeData>& current) {
        repaintRange(range);
        return &current->range() == &range;
    });
}

void Highlight::clearFromSetLike()
{
    for (auto& data : m_rangesData)
        repaintRange(data->range());
    m_rangesData.clear();
}

bool Highlight::addToSetLike(AbstractRange& range)
{
    auto index = m_rangesData.findIf([&range](const Ref<HighlightRangeData>& current) { return &current->range() == &range; });
    if (index == notFound) {
        repaintRange(range);
        m_rangesData.append(HighlightRangeData::create(range));
        return true;
    }
    // Move to last since SetLike is an ordered set.
    m_rangesData.append(WTFMove(m_rangesData[index]));
    m_rangesData.remove(index);
    return false;
}

void Highlight::repaint()
{
    for (auto& data : m_rangesData)
        repaintRange(data->range());
}

void Highlight::setPriority(int priority)
{
    if (m_priority == priority)
        return;
    m_priority = priority;
    repaint();
}

} // namespace WebCore
