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
#include "FloatingState.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FormattingContext.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutState.h"
#include "RuntimeEnabledFeatures.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatingState);

FloatingState::FloatItem::FloatItem(const Box& layoutBox, BoxGeometry absoluteBoxGeometry)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_position(layoutBox.isLeftFloatingPositioned() ? Position::Left : Position::Right)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
{
}

FloatingState::FloatItem::FloatItem(Position position, BoxGeometry absoluteBoxGeometry)
    : m_position(position)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
{
}

FloatingState::FloatingState(LayoutState& layoutState, const ContainerBox& formattingContextRoot)
    : m_layoutState(layoutState)
    , m_formattingContextRoot(makeWeakPtr(formattingContextRoot))
{
}

void FloatingState::append(FloatItem floatItem)
{
    if (m_floats.isEmpty())
        return m_floats.append(floatItem);

    // The integration codepath does not construct a layout box for the float item.
    ASSERT_IMPLIES(floatItem.floatBox(), m_floats.findMatching([&] (auto& entry) {
        return entry.floatBox() == floatItem.floatBox();
    }) == notFound);

    auto isLeftPositioned = floatItem.isLeftPositioned();
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
    return m_floats.insert(0, floatItem);
}

Optional<PositionInContextRoot> FloatingState::bottom(const ContainerBox& formattingContextRoot, Clear type) const
{
    if (m_floats.isEmpty())
        return { };

    // TODO: Currently this is only called once for each formatting context root with floats per layout.
    // Cache the value if we end up calling it more frequently (and update it at append/remove).
    Optional<PositionInContextRoot> bottom;
    for (auto& floatItem : m_floats) {
        // Ignore floats from ancestor formatting contexts when the floating state is inherited.
        if (!floatItem.isInFormattingContextOf(formattingContextRoot))
            continue;

        if ((type == Clear::Left && !floatItem.isLeftPositioned())
            || (type == Clear::Right && floatItem.isLeftPositioned()))
            continue;

        auto floatsBottom = floatItem.rectWithMargin().bottom();
        if (bottom) {
            bottom = std::max<PositionInContextRoot>(*bottom, { floatsBottom });
            continue;
        }
        bottom = PositionInContextRoot { floatsBottom };
    }
    return bottom;
}

Optional<PositionInContextRoot> FloatingState::top(const ContainerBox& formattingContextRoot) const
{
    if (m_floats.isEmpty())
        return { };

    Optional<PositionInContextRoot> top;
    for (auto& floatItem : m_floats) {
        // Ignore floats from ancestor formatting contexts when the floating state is inherited.
        if (!floatItem.isInFormattingContextOf(formattingContextRoot))
            continue;

        auto floatTop = floatItem.rectWithMargin().top();
        if (top) {
            top = std::min<PositionInContextRoot>(*top, { floatTop });
            continue;
        }
        top = PositionInContextRoot { floatTop };
    }
    return top;
}

}
}
#endif
