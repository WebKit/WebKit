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
#include "LayoutUnits.h"

namespace WebCore {
namespace Layout {

class InlineItem {
public:
    enum class Type : uint8_t { Text, HardLineBreak, SoftLineBreak, Box, Float, ContainerStart, ContainerEnd };
    InlineItem(const Box& layoutBox, Type);

    Type type() const { return m_type; }
    const Box& layoutBox() const { return *m_layoutBox; }
    const RenderStyle& style() const { return layoutBox().style(); }

    bool isText() const { return type() == Type::Text; }
    bool isBox() const { return type() == Type::Box; }
    bool isFloat() const { return type() == Type::Float; }
    bool isLineBreak() const { return isSoftLineBreak() || isHardLineBreak(); }
    bool isSoftLineBreak() const { return type() == Type::SoftLineBreak; }
    bool isHardLineBreak() const { return type() == Type::HardLineBreak; }
    bool isContainerStart() const { return type() == Type::ContainerStart; }
    bool isContainerEnd() const { return type() == Type::ContainerEnd; }

private:
    const Box* m_layoutBox { nullptr };
    Type m_type { };

protected:
    // For InlineTextItem
    enum class TextItemType  : uint8_t { Undefined, Whitespace, NonWhitespace };
    TextItemType m_textItemType { TextItemType::Undefined };
    bool m_hasWidth { false };
    bool m_isCollapsible { false };
    InlineLayoutUnit m_width { };
    unsigned m_length { 0 };

    // For InlineTextItem and InlineSoftLineBreakItem
    unsigned m_startOrPosition { 0 };
};

inline InlineItem::InlineItem(const Box& layoutBox, Type type)
    : m_layoutBox(&layoutBox)
    , m_type(type)
{
}

#define SPECIALIZE_TYPE_TRAITS_INLINE_ITEM(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Layout::ToValueTypeName) \
    static bool isType(const WebCore::Layout::InlineItem& inlineItem) { return inlineItem.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

}
}
#endif
