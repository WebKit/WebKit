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
#include "LayoutContainer.h"
#include "LayoutState.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatingState);

FloatingState::FloatItem::FloatItem(const Box& layoutBox, const FloatingState& floatingState)
    : m_layoutBox(makeWeakPtr(layoutBox))
    , m_absoluteDisplayBox(FormattingContext::mapBoxToAncestor(floatingState.layoutState(), layoutBox, downcast<Container>(floatingState.root())))
{
}

FloatingState::FloatingState(LayoutState& layoutState, const Box& formattingContextRoot)
    : m_layoutState(layoutState)
    , m_formattingContextRoot(makeWeakPtr(formattingContextRoot))
{
}

#ifndef NDEBUG
static bool belongsToThisFloatingContext(const Box& layoutBox, const Box& floatingStateRoot)
{
    auto& formattingContextRoot = layoutBox.formattingContextRoot();
    if (&formattingContextRoot == &floatingStateRoot)
        return true;

    // Maybe the layout box belongs to an inline formatting context that inherits the floating state from the parent (block) formatting context. 
    if (!formattingContextRoot.establishesInlineFormattingContext())
        return false;

    return &formattingContextRoot.formattingContextRoot() == &floatingStateRoot;
}
#endif

void FloatingState::remove(const Box& layoutBox)
{
    for (size_t index = 0; index < m_floats.size(); ++index) {
        if (m_floats[index] == layoutBox) {
            m_floats.remove(index);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void FloatingState::append(const Box& layoutBox)
{
    ASSERT(is<Container>(*m_formattingContextRoot));
    ASSERT(belongsToThisFloatingContext(layoutBox, *m_formattingContextRoot));
    ASSERT(is<Container>(*m_formattingContextRoot));

    auto newFloatItem = FloatItem { layoutBox, *this };
    if (m_floats.isEmpty())
        return m_floats.append(newFloatItem);

    auto& displayBox = m_layoutState.displayBoxForLayoutBox(layoutBox);
    auto isLeftPositioned = layoutBox.isLeftFloatingPositioned();
    // When adding a new float item to the list, we have to ensure that it is definitely the left(right)-most item.
    // Normally it is, but negative horizontal margins can push the float box beyond another float box.
    // Float items in m_floats list should stay in horizontal position order (left/right edge) on the same vertical position.
    auto hasNegativeHorizontalMargin = (isLeftPositioned && displayBox.marginStart() < 0) || (!isLeftPositioned && displayBox.marginEnd() < 0);
    if (!hasNegativeHorizontalMargin)
        return m_floats.append(newFloatItem);

    for (int i = m_floats.size() - 1; i >= 0; --i) {
        auto& floatItem = m_floats[i];
        if (isLeftPositioned != floatItem.isLeftPositioned())
            continue;
        if (newFloatItem.rectWithMargin().top() < floatItem.rectWithMargin().bottom())
            continue;
        if ((isLeftPositioned && newFloatItem.rectWithMargin().right() >= floatItem.rectWithMargin().right())
            || (!isLeftPositioned && newFloatItem.rectWithMargin().left() <= floatItem.rectWithMargin().left()))
            return m_floats.insert(i + 1, newFloatItem);
    }
    return m_floats.insert(0, newFloatItem);
}

FloatingState::Constraints FloatingState::constraints(PositionInContextRoot verticalPosition, const Box& formattingContextRoot) const
{
    if (isEmpty())
        return { };

    // 1. Convert vertical position if this floating context is inherited.
    // 2. Find the inner left/right floats at verticalPosition.
    // 3. Convert left/right positions back to formattingContextRoot's cooridnate system.
    auto coordinateMappingIsRequired = &root() != &formattingContextRoot;
    auto adjustedPosition = Point { 0, verticalPosition };

    if (coordinateMappingIsRequired)
        adjustedPosition = FormattingContext::mapCoordinateToAncestor(m_layoutState, adjustedPosition, downcast<Container>(formattingContextRoot), downcast<Container>(root()));

    Constraints constraints;
    for (int index = m_floats.size() - 1; index >= 0; --index) {
        auto& floatItem = m_floats[index];

        if (constraints.left && floatItem.isLeftPositioned())
            continue;

        if (constraints.right && !floatItem.isLeftPositioned())
            continue;

        auto rect = floatItem.rectWithMargin();
        if (!(rect.top() <= adjustedPosition.y && adjustedPosition.y < rect.bottom()))
            continue;

        if (floatItem.isLeftPositioned())
            constraints.left = PositionInContextRoot { rect.right() };
        else
            constraints.right = PositionInContextRoot { rect.left() };

        if (constraints.left && constraints.right)
            break;
    }

    if (coordinateMappingIsRequired) {
        if (constraints.left)
            constraints.left = PositionInContextRoot { *constraints.left - adjustedPosition.x };

        if (constraints.right)
            constraints.right = PositionInContextRoot { *constraints.right - adjustedPosition.x };
    }

    return constraints;
}

Optional<PositionInContextRoot> FloatingState::bottom(const Box& formattingContextRoot, Clear type) const
{
    if (m_floats.isEmpty())
        return { };

    // TODO: Currently this is only called once for each formatting context root with floats per layout.
    // Cache the value if we end up calling it more frequently (and update it at append/remove).
    Optional<PositionInContextRoot> bottom;
    for (auto& floatItem : m_floats) {
        // Ignore floats from ancestor formatting contexts when the floating state is inherited.
        if (!floatItem.isDescendantOfFormattingRoot(formattingContextRoot))
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

}
}
#endif
