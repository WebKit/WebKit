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

#include "InlineIteratorBox.h"
#include "LayoutIntegrationLineLayout.h"
#include "TextPainter.h"

namespace WebCore {
namespace LayoutIntegration {

InlineContent::InlineContent(const LineLayout& lineLayout)
    : m_lineLayout(lineLayout)
{
}

bool InlineContent::hasContent() const
{
    ASSERT(boxes.isEmpty() || boxes[0].isRootInlineBox());
    return boxes.size() > 1;
}

IteratorRange<const InlineDisplay::Box*> InlineContent::boxesForRect(const LayoutRect& rect) const
{
    if (boxes.isEmpty())
        return { nullptr, nullptr };

    if (lines.first().inkOverflow().maxY() > rect.y() && lines.last().inkOverflow().y() < rect.maxY())
        return { &boxes.first(), &boxes.last() + 1 };

    // The optimization below relies on line paint bounds not exeeding those of the neighboring lines
    if (hasMultilinePaintOverlap)
        return { &boxes.first(), &boxes.last() + 1 };

    auto height = lines.last().lineBoxBottom() - lines.first().lineBoxTop();
    auto averageLineHeight = height / lines.size();

    auto approximateLine = [&](LayoutUnit y) {
        y = std::max(y, 0_lu);
        return std::min(static_cast<size_t>(y / averageLineHeight), lines.size() - 1);
    };

    auto startLine = approximateLine(rect.y());
    for (; startLine; --startLine) {
        if (lines[startLine - 1].inkOverflow().maxY() < rect.y())
            break;
    }

    auto endLine = approximateLine(rect.maxY());
    for (; endLine < lines.size() - 1; ++endLine) {
        if (lines[endLine + 1].inkOverflow().y() > rect.maxY())
            break;
    }

    auto firstBox = lines[startLine].firstBoxIndex();
    auto lastBox = lines[endLine].firstBoxIndex() + lines[endLine].boxCount() - 1;

    return { &boxes[firstBox], &boxes[lastBox] + 1 };
}

InlineContent::~InlineContent()
{
    for (auto& box : boxes)
        TextPainter::removeGlyphDisplayList(box);
}

const RenderObject& InlineContent::rendererForLayoutBox(const Layout::Box& layoutBox) const
{
    return lineLayout().rendererForLayoutBox(layoutBox);
}

const RenderBlockFlow& InlineContent::formattingContextRoot() const
{
    return lineLayout().flow();
}

size_t InlineContent::indexForBox(const InlineDisplay::Box& box) const
{
    auto index = static_cast<size_t>(&box - boxes.begin());
    RELEASE_ASSERT(index < boxes.size());
    return index;
}

const InlineDisplay::Box* InlineContent::firstBoxForLayoutBox(const Layout::Box& layoutBox) const
{
    auto index = firstBoxIndexForLayoutBox(layoutBox);
    return index ? &boxes[*index] : nullptr;
}

std::optional<size_t> InlineContent::firstBoxIndexForLayoutBox(const Layout::Box& layoutBox) const
{
    constexpr auto cacheThreshold = 16;

    if (boxes.size() < cacheThreshold) {
        for (size_t i = 0; i < boxes.size(); ++i) {
            auto& box = boxes[i];
            if (&box.layoutBox() == &layoutBox)
                return i;
        }
        return { };
    }
    
    if (!m_firstBoxIndexCache) {
        m_firstBoxIndexCache = makeUnique<FirstBoxIndexCache>();
        for (size_t i = 0; i < boxes.size(); ++i) {
            auto& box = boxes[i];
            if (box.isRootInlineBox())
                continue;
            m_firstBoxIndexCache->add(box.layoutBox(), i);
        }
    }

    auto it = m_firstBoxIndexCache->find(layoutBox);
    if (it == m_firstBoxIndexCache->end())
        return { };

    return it->value;
}

const Vector<size_t>& InlineContent::nonRootInlineBoxIndexesForLayoutBox(const Layout::Box& layoutBox) const
{
    ASSERT(layoutBox.isElementBox());

    if (!m_inlineBoxIndexCache) {
        m_inlineBoxIndexCache = makeUnique<InlineBoxIndexCache>();
        for (size_t i = 0; i < boxes.size(); ++i) {
            auto& box = boxes[i];
            if (!box.isNonRootInlineBox())
                continue;
            m_inlineBoxIndexCache->ensure(box.layoutBox(), [&] {
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
    m_firstBoxIndexCache = { };
    m_inlineBoxIndexCache = { };
}

void InlineContent::shrinkToFit()
{
    boxes.shrinkToFit();
    lines.shrinkToFit();
}

}
}

