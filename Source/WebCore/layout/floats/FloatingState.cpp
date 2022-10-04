/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "FloatingState.h"

#include "LayoutContainingBlockChainIterator.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatingState);

FloatingState::FloatItem::FloatItem(const Box& layoutBox, Position position, BoxGeometry absoluteBoxGeometry)
    : m_layoutBox(layoutBox)
    , m_position(position)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
{
}

FloatingState::FloatItem::FloatItem(Position position, BoxGeometry absoluteBoxGeometry)
    : m_position(position)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
{
}

FloatingState::FloatingState(LayoutState& layoutState, const ElementBox& formattingContextRoot)
    : m_layoutState(layoutState)
    , m_formattingContextRoot(formattingContextRoot)
    , m_isLeftToRightDirection(formattingContextRoot.style().isLeftToRightDirection())
{
}

void FloatingState::append(FloatItem floatItem)
{
    auto isLeftPositioned = floatItem.isLeftPositioned();
    m_positionTypes.add(isLeftPositioned ? PositionType::Left : PositionType::Right);

    if (m_floats.isEmpty())
        return m_floats.append(floatItem);

    // The integration codepath does not construct a layout box for the float item.
    ASSERT_IMPLIES(floatItem.floatBox(), m_floats.findIf([&] (auto& entry) {
        return entry.floatBox() == floatItem.floatBox();
    }) == notFound);

    // When adding a new float item to the list, we have to ensure that it is definitely the left(right)-most item.
    // Normally it is, but negative horizontal margins can push the float box beyond another float box.
    // Float items in m_floats list should stay in horizontal position order (left/right edge) on the same vertical position.
    auto horizontalMargin = floatItem.horizontalMargin();
    auto hasNegativeHorizontalMargin = (isLeftPositioned && horizontalMargin.start < 0) || (!isLeftPositioned && horizontalMargin.end < 0);
    if (!hasNegativeHorizontalMargin)
        return m_floats.append(floatItem);

    for (int i = m_floats.size() - 1; i >= 0; --i) {
        auto& floatItem = m_floats[i];
        if (isLeftPositioned != floatItem.isLeftPositioned())
            continue;
        if (floatItem.rectWithMargin().top() < floatItem.rectWithMargin().bottom())
            continue;
        if ((isLeftPositioned && floatItem.rectWithMargin().right() >= floatItem.rectWithMargin().right())
            || (!isLeftPositioned && floatItem.rectWithMargin().left() <= floatItem.rectWithMargin().left()))
            return m_floats.insert(i + 1, floatItem);
    }
    m_floats.insert(0, floatItem);
}

bool FloatingState::FloatItem::isInFormattingContextOf(const ElementBox& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    ASSERT(!is<InitialContainingBlock>(m_layoutBox));
    for (auto& containingBlock : containingBlockChain(*m_layoutBox)) {
        if (&containingBlock == &formattingContextRoot)
            return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void FloatingState::clear()
{
    m_floats.clear();
    m_positionTypes = { };
}

}
}
