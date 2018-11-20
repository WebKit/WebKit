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

#include "LayoutBox.h"
#include "LayoutInlineBox.h"
#include "LayoutLineBreakBox.h"

namespace WebCore {
namespace Layout {

class InlineItem {
public:
    InlineItem(const Box& layoutBox);

    enum class Type { Text, HardLineBreak, InlineBox, Float };
    Type type() const;
    const Box& layoutBox() const { return m_layoutBox; }
    const RenderStyle& style() const { return m_layoutBox.style(); }
    String textContent() const;
    // DetachingRule indicates whether the inline element needs to be wrapped in a dediceted run or break from previous/next runs.
    // <span>start</span><span style="position: relative;"> middle </span><span>end</span>
    // input to line breaking -> <start middle end>
    // output of line breaking (considering infinite constraint) -> <start middle end>
    // due to the in-flow positioning, the final runs are: <start>< middle ><end>
    // "start" -> n/a
    // " middle " -> BreakAtStart and BreakAtEnd
    // "end" -> n/a
    //
    // <span>parent </span><span style="padding: 10px;">start<span> middle </span>end</span><span> parent</span>
    // input to line breaking -> <parent start middle end parent>
    // output of line breaking (considering infinite constraint) -> <parent start middle end parent>
    // due to padding, final runs -> <parent><start middle end><parent>
    // "parent" -> n/a
    // "start" -> BreakAtStart
    // " middle " -> n/a
    // "end" -> BreakAtEnd
    // "parent" -> n/a
    enum class DetachingRule {
        BreakAtStart = 1 << 0,
        BreakAtEnd = 1 << 1
    };
    void addDetachingRule(OptionSet<DetachingRule> detachingRule) { m_detachingRules.add(detachingRule); }
    OptionSet<DetachingRule> detachingRules() const { return m_detachingRules; }

private:
    const Box& m_layoutBox;
    OptionSet<DetachingRule> m_detachingRules;
};

// FIXME: Fix HashSet/ListHashSet to support smart pointer types.
struct InlineItemHashFunctions {
    static unsigned hash(const std::unique_ptr<InlineItem>& key) { return PtrHash<InlineItem*>::hash(key.get()); }
    static bool equal(const std::unique_ptr<InlineItem>& a, const std::unique_ptr<InlineItem>& b) { return a.get() == b.get(); }
};

struct InlineItemHashTranslator {
    static unsigned hash(const InlineItem& key) { return PtrHash<const InlineItem*>::hash(&key); }
    static bool equal(const std::unique_ptr<InlineItem>& a, const InlineItem& b) { return a.get() == &b; }
};
using InlineContent = ListHashSet<std::unique_ptr<InlineItem>, InlineItemHashFunctions>;

inline InlineItem::InlineItem(const Box& layoutBox)
    : m_layoutBox(layoutBox)
{
}

inline InlineItem::Type InlineItem::type() const
{
    if (is<InlineBox>(m_layoutBox) && downcast<InlineBox>(m_layoutBox).hasTextContent())
        return Type::Text;

    if (is<LineBreakBox>(m_layoutBox))
        return Type::HardLineBreak;

    if (m_layoutBox.isFloatingPositioned())
        return Type::Float;

    ASSERT(m_layoutBox.isInlineLevelBox());
    return Type::InlineBox;
}

inline String InlineItem::textContent() const
{
    if (type() != Type::Text)
        return { };

    return downcast<InlineBox>(m_layoutBox).textContent();
}

}
}
#endif
