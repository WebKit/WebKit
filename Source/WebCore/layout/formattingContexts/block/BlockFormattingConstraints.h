/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/FormattingConstraints.h>

namespace WebCore {
namespace Layout {

struct ConstraintsForBlockContent : public ConstraintsForInFlowContent {
    ConstraintsForBlockContent(const ConstraintsForInFlowContent&, std::optional<LayoutUnit> contentBoxBlockSize, LayoutUnit fillAvailableBlockSize, bool relayoutChildren = false);

    // The root's content box block size when it is definite. Percentage block sizes of in-flow boxes resolve against it,
    // and behave as auto when there is none (CSS 2.2 §10.5).
    std::optional<LayoutUnit> contentBoxBlockSize() const { return m_contentBoxBlockSize; }
    // The block size -webkit-fill-available block sizes of in-flow boxes resolve to, which is always there (RenderBox::availableLogicalHeight).
    LayoutUnit fillAvailableBlockSize() const { return m_fillAvailableBlockSize; }
    // The parent formatting context lays the root's children out again, like when the root's inline size changed (RenderBlockFlow::layoutBlock).
    bool relayoutChildren() const { return m_relayoutChildren; }

private:
    std::optional<LayoutUnit> m_contentBoxBlockSize;
    LayoutUnit m_fillAvailableBlockSize;
    bool m_relayoutChildren { false };
};

inline ConstraintsForBlockContent::ConstraintsForBlockContent(const ConstraintsForInFlowContent& genericConstraints, std::optional<LayoutUnit> contentBoxBlockSize, LayoutUnit fillAvailableBlockSize, bool relayoutChildren)
    : ConstraintsForInFlowContent(genericConstraints.horizontal(), genericConstraints.logicalTop(), BlockContent)
    , m_contentBoxBlockSize(contentBoxBlockSize)
    , m_fillAvailableBlockSize(fillAvailableBlockSize)
    , m_relayoutChildren(relayoutChildren)
{
}

}
}

SPECIALIZE_TYPE_TRAITS_LAYOUT_FORMATTING_CONSTRAINTS(ConstraintsForBlockContent, isConstraintsForBlockContent())
