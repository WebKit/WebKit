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

#include "config.h"
#include "InlineTextItem.h"

#include "FontCascade.h"
#include "InlineSoftLineBreakItem.h"
#include "TextUtil.h"
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {
namespace Layout {

static_assert(sizeof(InlineItem) == sizeof(InlineTextItem));

InlineTextItem::InlineTextItem(const InlineTextBox& inlineTextBox, unsigned start, unsigned length, UBiDiLevel bidiLevel, bool hasTrailingSoftHyphen, bool isWordSeparator, std::optional<InlineLayoutUnit> width, TextItemType textItemType)
    : InlineItem(inlineTextBox, Type::Text, bidiLevel)
{
    m_startOrPosition = start;
    m_length = length;
    m_hasWidth = !!width;
    m_hasTrailingSoftHyphen = hasTrailingSoftHyphen;
    m_isWordSeparator = isWordSeparator;
    m_width = width.value_or(0);
    m_textItemType = textItemType;
}

InlineTextItem::InlineTextItem(const InlineTextBox& inlineTextBox)
    : InlineItem(inlineTextBox, Type::Text, UBIDI_DEFAULT_LTR)
{
}

InlineTextItem InlineTextItem::left(unsigned length) const
{
    RELEASE_ASSERT(length <= this->length());
    ASSERT(m_textItemType != TextItemType::Undefined);
    ASSERT(length);
    return { inlineTextBox(), start(), length, bidiLevel(), false, isWordSeparator(), std::nullopt, m_textItemType };
}

InlineTextItem InlineTextItem::right(unsigned length, std::optional<InlineLayoutUnit> width) const
{
    RELEASE_ASSERT(length <= this->length());
    ASSERT(m_textItemType != TextItemType::Undefined);
    ASSERT(length);
    return { inlineTextBox(), end() - length, length, bidiLevel(), hasTrailingSoftHyphen(), isWordSeparator(), width, m_textItemType };
}

InlineTextItem InlineTextItem::split(size_t leftSideLength)
{
    RELEASE_ASSERT(length() > 1);
    RELEASE_ASSERT(leftSideLength && leftSideLength < length());
    auto rightSide = right(length() - leftSideLength, { });
    m_length = length() - rightSide.length();
    m_hasWidth = false;
    m_width = { };
    return rightSide;
}

bool InlineTextItem::isZeroWidthSpaceSeparator() const
{
    // FIXME: We should check for more zero width content and not just U+200B.
    return !m_length || (m_length == 1 && inlineTextBox().content()[start()] == zeroWidthSpace); 
}

bool InlineTextItem::isQuirkNonBreakingSpace() const
{
    if (style().nbspMode() != NBSPMode::Space || style().whiteSpace() == WhiteSpace::Pre || style().whiteSpace() == WhiteSpace::NoWrap || style().whiteSpace() == WhiteSpace::BreakSpaces)
        return false;
    return m_length && inlineTextBox().content()[start()] == noBreakSpace;
}

bool InlineTextItem::isFullyTrimmable() const
{
    return isWhitespace() && !TextUtil::shouldPreserveSpacesAndTabs(layoutBox());
}

bool InlineTextItem::shouldPreserveSpacesAndTabs(const InlineTextItem& inlineTextItem)
{
    ASSERT(inlineTextItem.isWhitespace());
    return TextUtil::shouldPreserveSpacesAndTabs(inlineTextItem.layoutBox());
}

}
}
