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
#include "HighlightRegistry.h"

#include "Document.h"
#include "FloatRect.h"
#include "HighlightHitResult.h"
#include "HighlightsFromPointOptions.h"
#include "IDLTypes.h"
#include "JSDOMMapLike.h"
#include "JSHighlight.h"
#include "NodeDocument.h"
#include "RenderObject.h"
#include "ShadowRoot.h"
#include "SimpleRange.h"
#include "StaticRange.h"
#include <algorithm>

namespace WebCore {
    
void HighlightRegistry::initializeMapLike(DOMMapAdapter& map)
{
    for (auto& keyValue : m_map)
        map.set<IDLDOMString, IDLInterface<Highlight>>(keyValue.key, keyValue.value);
}

Vector<HighlightHitResult> HighlightRegistry::highlightsFromPoint(Document& document, float x, float y, HighlightsFromPointOptions&& options)
{
    document.updateLayout();

    FloatPoint point { x, y };

    auto reachableGivenShadowRoots = [&](const Ref<Node>& node) {
        for (RefPtr shadowRoot = node->containingShadowRoot(); shadowRoot;) {
            if (!options.shadowRoots.containsIf([&](auto& allowed) { return allowed.ptr() == shadowRoot.get(); }))
                return false;
            RefPtr host = shadowRoot->host();
            shadowRoot = host ? host->containingShadowRoot() : nullptr;
        }
        return true;
    };

    struct HitCandidate {
        int priority { 0 };
        size_t registrationIndex { 0 };
        Vector<Ref<AbstractRange>> ranges;
        RefPtr<Highlight> highlight;
    };
    Vector<HitCandidate> candidates;

    for (size_t index = 0; index < m_highlightNames.size(); ++index) {
        RefPtr highlight = m_map.get(m_highlightNames[index]);
        if (!highlight)
            continue;

        Vector<Ref<AbstractRange>> matchingRanges;
        auto highlightRanges = highlight->highlightRanges();
        for (auto& highlightRange : highlightRanges) {
            Ref abstractRange = highlightRange->range();
            if (abstractRange->collapsed())
                continue;

            Ref startContainer = abstractRange->startContainer();
            if (&startContainer->document() != &document)
                continue;

            if (!reachableGivenShadowRoots(startContainer))
                continue;

            if (RefPtr staticRange = dynamicDowncast<StaticRange>(abstractRange.get()); staticRange && !staticRange->computeValidity())
                continue;

            for (auto& rect : RenderObject::clientTextRects(makeSimpleRange(abstractRange))) {
                if (rect.contains(point)) {
                    matchingRanges.append(WTF::move(abstractRange));
                    break;
                }
            }
        }

        if (!matchingRanges.isEmpty())
            candidates.append({ highlight->priority(), index, WTF::move(matchingRanges), WTF::move(highlight) });
    }

    std::ranges::sort(candidates, [](auto& a, auto& b) {
        if (a.priority != b.priority)
            return a.priority > b.priority;
        return a.registrationIndex > b.registrationIndex;
    });

    return WTF::map(WTF::move(candidates), [](auto&& candidate) {
        return HighlightHitResult { WTF::move(candidate.highlight), WTF::move(candidate.ranges) };
    });
}

void HighlightRegistry::setFromMapLike(AtomString&& key, Ref<Highlight>&& value)
{
    auto addResult = m_map.set(key, WTF::move(value));
    if (addResult.isNewEntry) {
        ASSERT(!m_highlightNames.contains(key));
        m_highlightNames.append(WTF::move(key));
        protect(addResult.iterator->value)->repaint();
    }
}

void HighlightRegistry::clear()
{
    m_highlightNames.clear();
    auto map = std::exchange(m_map, { });
    for (auto& highlight : map.values())
        highlight->clearFromSetLike();
}

bool HighlightRegistry::remove(const AtomString& key)
{
    auto highlight = m_map.take(key);
    if (!highlight)
        return false;

    m_highlightNames.removeFirst(key);
    highlight->repaint();
    return true;
}

#if ENABLE(APP_HIGHLIGHTS)
void HighlightRegistry::setHighlightVisibility(HighlightVisibility highlightVisibility)
{
    if (m_highlightVisibility == highlightVisibility)
        return;
    
    m_highlightVisibility = highlightVisibility;
    
    for (auto& highlight : m_map)
        protect(highlight.value)->repaint();
}
#endif

static ASCIILiteral annotationHighlightKey()
{
    return "annotationHighlightKey"_s;
}

void HighlightRegistry::addAnnotationHighlightWithRange(Ref<StaticRange>&& value)
{
    if (m_map.contains(annotationHighlightKey()))
        protect(*m_map.get(annotationHighlightKey()))->addToSetLike(value);
    else
        setFromMapLike(annotationHighlightKey(), Highlight::create({ std::ref<AbstractRange>(value.get()) }));
}

}
