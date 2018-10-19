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

private:
    const Box& m_layoutBox;
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
