/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "PropertySetCSSStyleDeclaration.h"
#include "RenderBlockFlow.h"
#include "RenderObject.h"
#include "StaticRange.h"
#include "StyleProperties.h"
#include <wtf/Ref.h>

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
#include "LayoutIntegrationLineLayout.h"
#endif

namespace WebCore {

Highlight::Highlight(Ref<StaticRange>&& range)
{
    auto myRange = WTFMove(range);
    addToSetLike(myRange.get());
}

Ref<Highlight> Highlight::create(StaticRange& range)
{
    return adoptRef(*new Highlight(range));
}

void Highlight::initializeSetLike(DOMSetAdapter& set)
{
    for (auto& rangeData : m_rangesData)
        set.add<IDLInterface<StaticRange>>(rangeData->range);
}

static void repaintRange(const SimpleRange& range)
{
    // FIXME: Unclear precisely why we need to handle out of order cases here, but not unordered cases.
    auto sortedRange = range;
    if (is_gt(treeOrder<ComposedTree>(range.start, range.end)))
        std::swap(sortedRange.start, sortedRange.end);
    for (auto& node : intersectingNodes(sortedRange)) {
        if (auto renderer = node.renderer()) {
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
            if (auto lineLayout = LayoutIntegration::LineLayout::containing(*renderer))
                lineLayout->flow().ensureLineBoxes();
#endif
            renderer->repaint();
        }
    }
}

bool Highlight::removeFromSetLike(const StaticRange& range)
{
    return m_rangesData.removeFirstMatching([&range](const Ref<HighlightRangeData>& current) {
        repaintRange(range);
        return current.get().range.get() == range;
    });
}

void Highlight::clearFromSetLike()
{
    for (auto& data : m_rangesData)
        repaintRange(data->range);
    m_rangesData.clear();
}

bool Highlight::addToSetLike(StaticRange& range)
{
    if (notFound != m_rangesData.findMatching([&range](const Ref<HighlightRangeData>& current) { return current.get().range.get() == range; }))
        return false;
    repaintRange(range);
    m_rangesData.append(HighlightRangeData::create(range));
    return true;
}

void Highlight::repaint()
{
    for (auto& data : m_rangesData)
        repaintRange(data->range);
}

}

