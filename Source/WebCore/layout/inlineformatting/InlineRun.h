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
    InlineRun(LayoutUnit logcialLeft, LayoutUnit width, ItemPosition, unsigned length, const InlineItem&);

    LayoutUnit logicalLeft() const { return m_logicalLeft; }
    LayoutUnit logicalRight() const { return logicalLeft() + width(); }
    LayoutUnit width() const { return m_width; }

    void setWidth(LayoutUnit width) { m_width = width; }
    void setLogicalRight(LayoutUnit logicalRight) { m_width -= (this->logicalRight() - logicalRight); }

    struct TextContext {
    public:
        TextContext(ItemPosition, unsigned length);

        ItemPosition position() const { return m_position; }
        unsigned length() const { return m_length; }

        void setLength(unsigned length) { m_length = length; }

    private:
        ItemPosition m_position;
        unsigned m_length;
    };
    std::optional<TextContext>& textContext() { return m_textContext; }
    const InlineItem& inlineItem() const { return m_inlineItem; }

private:
    LayoutUnit m_logicalLeft;
    LayoutUnit m_width;

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

inline InlineRun::InlineRun(LayoutUnit logicalLeft, LayoutUnit width, ItemPosition position, unsigned length, const InlineItem& inlineItem)
    : m_logicalLeft(logicalLeft)
    , m_width(width)
    , m_inlineItem(inlineItem)
    , m_textContext(TextContext { position, length })
{
}

inline InlineRun::TextContext::TextContext(ItemPosition position, unsigned length)
    : m_position(position)
    , m_length(length)
{
}

}
}
#endif
