/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "InlineRunProvider.h"
#include "LayoutUnit.h"

namespace WebCore {
namespace Layout {

struct InlineRun {
    InlineRun(LayoutUnit logcialLeft, LayoutUnit width, const InlineItem&);

    LayoutUnit logicalLeft() const { return m_logicalLeft; }
    LayoutUnit logicalRight() const { return logicalLeft() + width(); }
    LayoutUnit width() const { return m_width; }

    void setWidth(LayoutUnit width) { m_width = width; }
    void setLogicalLeft(LayoutUnit logicalLeft) { m_logicalLeft = logicalLeft; }
    void setLogicalRight(LayoutUnit logicalRight) { m_width -= (this->logicalRight() - logicalRight); }
    void moveHorizontally(LayoutUnit delta) { m_logicalLeft += delta; }

    struct ExpansionOpportunity {
        unsigned count { 0 };
        ExpansionBehavior behavior { ForbidLeadingExpansion | ForbidTrailingExpansion };
        LayoutUnit expansion;
    };
    ExpansionOpportunity& expansionOpportunity() { return m_expansionOpportunity; }

    struct TextContext {
    public:
        TextContext(ItemPosition, unsigned length);

        void setStart(ItemPosition start) { m_start = start; }
        void setLength(unsigned length) { m_length = length; }

        ItemPosition start() const { return m_start; }
        unsigned length() const { return m_length; }

    private:
        ItemPosition m_start;
        unsigned m_length;
    };
    void setTextContext(TextContext textContext) { m_textContext.emplace(textContext); }
    std::optional<TextContext>& textContext() { return m_textContext; }
    std::optional<TextContext> textContext() const { return m_textContext; }

    const InlineItem& inlineItem() const { return m_inlineItem; }

private:
    LayoutUnit m_logicalLeft;
    LayoutUnit m_width;
    ExpansionOpportunity m_expansionOpportunity;

    const InlineItem& m_inlineItem;
    std::optional<TextContext> m_textContext;
};

using InlineRuns = Vector<InlineRun>;

inline InlineRun::InlineRun(LayoutUnit logicalLeft, LayoutUnit width, const InlineItem& inlineItem)
    : m_logicalLeft(logicalLeft)
    , m_width(width)
    , m_inlineItem(inlineItem)
{
}

inline InlineRun::TextContext::TextContext(ItemPosition start, unsigned length)
    : m_start(start)
    , m_length(length)
{
}

}
}
#endif
