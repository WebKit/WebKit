/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "LayoutIntegrationInlineContent.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutIntegrationLineLayout.h"
#include "LayoutIntegrationRunIterator.h"
#include "RuntimeEnabledFeatures.h"
#include "TextPainter.h"

namespace WebCore {
namespace LayoutIntegration {

InlineContent::InlineContent(const LineLayout& lineLayout)
    : m_lineLayout(makeWeakPtr(lineLayout))
{
}

bool InlineContent::hasContent() const
{
    ASSERT(runs.isEmpty() || runs[0].isRootInlineBox());
    return runs.size() > 1;
};

WTF::IteratorRange<const Run*> InlineContent::runsForRect(const LayoutRect&) const
{
    // FIXME: Do something efficient e.g. using line boxes.
    if (runs.isEmpty())
        return { nullptr, nullptr };
    return { &runs.first(), &runs.last() + 1 };
}

InlineContent::~InlineContent()
{
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        for (auto& run : runs)
            TextPainter::removeGlyphDisplayList(run);
    }
}

const LineLayout& InlineContent::lineLayout() const
{
    return *m_lineLayout;
}

const RenderObject& InlineContent::rendererForLayoutBox(const Layout::Box& layoutBox) const
{
    return m_lineLayout->rendererForLayoutBox(layoutBox);
}

const RenderBlockFlow& InlineContent::containingBlock() const
{
    return m_lineLayout->flow();
}

size_t InlineContent::indexForRun(const Run& run) const
{
    auto index = static_cast<size_t>(&run - runs.begin());
    RELEASE_ASSERT(index < runs.size());
    return index;
}

const Run* InlineContent::firstRunForLayoutBox(const Layout::Box& layoutBox) const
{
    auto index = firstRunIndexForLayoutBox(layoutBox);
    return index ? &runs[*index] : nullptr;
}

std::optional<size_t> InlineContent::firstRunIndexForLayoutBox(const Layout::Box& layoutBox) const
{
    constexpr auto cacheThreshold = 16;

    if (runs.size() < cacheThreshold) {
        for (size_t i = 0; i < runs.size(); ++i) {
            auto& run = runs[i];
            if (&run.layoutBox() == &layoutBox)
                return i;
        }
        return { };
    }
    
    if (!m_firstRunIndexCache) {
        m_firstRunIndexCache = makeUnique<FirstRunIndexCache>();
        for (size_t i = 0; i < runs.size(); ++i) {
            auto& run = runs[i];
            if (run.isRootInlineBox())
                continue;
            m_firstRunIndexCache->add(run.layoutBox(), i);
        }
    }

    auto it = m_firstRunIndexCache->find(layoutBox);
    if (it == m_firstRunIndexCache->end())
        return { };

    return it->value;
}

const Vector<size_t>& InlineContent::nonRootInlineBoxIndexesForLayoutBox(const Layout::Box& layoutBox) const
{
    ASSERT(layoutBox.isContainerBox());

    if (!m_inlineBoxIndexCache) {
        m_inlineBoxIndexCache = makeUnique<InlineBoxIndexCache>();
        for (size_t i = 0; i < runs.size(); ++i) {
            auto& run = runs[i];
            if (!run.isNonRootInlineBox())
                continue;
            m_inlineBoxIndexCache->ensure(run.layoutBox(), [&] {
                return Vector<size_t> { };
            }).iterator->value.append(i);
        }
        for (auto entry : *m_inlineBoxIndexCache)
            entry.value.shrinkToFit();
    }

    auto it = m_inlineBoxIndexCache->find(layoutBox);
    if (it == m_inlineBoxIndexCache->end()) {
        static NeverDestroyed<Vector<size_t>> emptyVector;
        return emptyVector.get();
    }

    return it->value;
}

void InlineContent::releaseCaches()
{
    m_firstRunIndexCache = { };
    m_inlineBoxIndexCache = { };
}

void InlineContent::shrinkToFit()
{
    runs.shrinkToFit();
    lines.shrinkToFit();
}

}
}

#endif
