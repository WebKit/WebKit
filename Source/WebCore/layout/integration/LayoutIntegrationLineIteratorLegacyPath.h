/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "LayoutIntegrationInlineContent.h"
#include "LayoutIntegrationRunIteratorLegacyPath.h"
#include "RootInlineBox.h"

namespace WebCore {

namespace LayoutIntegration {

class LineIteratorLegacyPath {
public:
    LineIteratorLegacyPath(const RootInlineBox* rootInlineBox)
        : m_rootInlineBox(rootInlineBox)
    {
    }
    LineIteratorLegacyPath(LineIteratorLegacyPath&&) = default;
    LineIteratorLegacyPath(const LineIteratorLegacyPath&) = default;
    LineIteratorLegacyPath& operator=(const LineIteratorLegacyPath&) = default;
    LineIteratorLegacyPath& operator=(LineIteratorLegacyPath&&) = default;

    LayoutUnit top() const { return m_rootInlineBox->lineTop(); }
    LayoutUnit bottom() const { return m_rootInlineBox->lineBottom(); }
    LayoutUnit selectionTop() const { return m_rootInlineBox->selectionTop(); }
    LayoutUnit selectionTopForHitTesting() const { return m_rootInlineBox->selectionTop(RootInlineBox::ForHitTesting::Yes); }
    LayoutUnit selectionBottom() const { return m_rootInlineBox->selectionBottom(); }
    LayoutUnit lineBoxTop() const { return m_rootInlineBox->lineBoxTop(); }
    LayoutUnit lineBoxBottom() const { return m_rootInlineBox->lineBoxBottom(); }

    float y() const { return m_rootInlineBox->y(); }
    float contentLogicalLeft() const { return m_rootInlineBox->logicalLeft(); }
    float contentLogicalRight() const { return m_rootInlineBox->logicalRight(); }
    float logicalHeight() const { return m_rootInlineBox->logicalHeight(); }
    bool isHorizontal() const { return m_rootInlineBox->isHorizontal(); }

    const RenderBlockFlow& containingBlock() const { return m_rootInlineBox->blockFlow(); }
    const RootInlineBox* legacyRootInlineBox() const { return m_rootInlineBox; }

    void traverseNext()
    {
        m_rootInlineBox = m_rootInlineBox->nextRootBox();
    }

    void traversePrevious()
    {
        m_rootInlineBox = m_rootInlineBox->prevRootBox();
    }

    bool operator==(const LineIteratorLegacyPath& other) const { return m_rootInlineBox == other.m_rootInlineBox; }

    bool atEnd() const { return !m_rootInlineBox; }

    RunIteratorLegacyPath firstRun() const
    {
        return { m_rootInlineBox->firstLeafDescendant() };
    }

    RunIteratorLegacyPath lastRun() const
    {
        return { m_rootInlineBox->lastLeafDescendant() };
    }


    RunIteratorLegacyPath logicalStartRun() const
    {
        return { *m_rootInlineBox, RunIteratorLegacyPath::LogicalOrder::Start };
    }

    RunIteratorLegacyPath logicalEndRun() const
    {
        return { *m_rootInlineBox, RunIteratorLegacyPath::LogicalOrder::End };
    }

private:
    const RootInlineBox* m_rootInlineBox;
};

}
}
