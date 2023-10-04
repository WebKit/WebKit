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
#include "PlacedFloats.h"

#include "LayoutContainingBlockChainIterator.h"
#include "LayoutInitialContainingBlock.h"
#include "RenderStyleInlines.h"
#include "Shape.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(PlacedFloats);

PlacedFloats::Item::Item(const Box& layoutBox, Position position, const BoxGeometry& absoluteBoxGeometry, LayoutPoint localTopLeft, std::optional<size_t> line)
    : m_layoutBox(layoutBox)
    , m_position(position)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
    , m_localTopLeft(localTopLeft)
    , m_shape(layoutBox.shape())
    , m_placedByLine(line)
{
}

PlacedFloats::Item::Item(Position position, const BoxGeometry& absoluteBoxGeometry, LayoutPoint localTopLeft, const Shape* shape)
    : m_position(position)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
    , m_localTopLeft(localTopLeft)
    , m_shape(shape)
{
}

BoxGeometry PlacedFloats::Item::boxGeometry() const
{
    auto boxGeometry = BoxGeometry { m_absoluteBoxGeometry };
    boxGeometry.setTopLeft(m_localTopLeft);
    return boxGeometry;
}

PlacedFloats::Item::~Item() = default;

PlacedFloats::PlacedFloats(const ElementBox& blockFormattingContextRoot)
    : m_blockFormattingContextRoot(blockFormattingContextRoot)
    , m_isLeftToRightDirection(blockFormattingContextRoot.style().isLeftToRightDirection())
{
    ASSERT(blockFormattingContextRoot.establishesBlockFormattingContext());
}

void PlacedFloats::append(Item newFloatItem)
{
    auto isLeftPositioned = newFloatItem.isLeftPositioned();
    m_positionTypes.add(isLeftPositioned ? PositionType::Left : PositionType::Right);

    if (m_list.isEmpty())
        return m_list.append(newFloatItem);

    // The integration codepath does not construct a layout box for the float item.
    ASSERT_IMPLIES(newFloatItem.layoutBox(), m_list.findIf([&] (auto& entry) {
        return entry.layoutBox() == newFloatItem.layoutBox();
    }) == notFound);

    // When adding a new float item to the list, we have to ensure that it is definitely the left(right)-most item.
    // Normally it is, but negative horizontal margins can push the float box beyond another float box.
    // Float items in m_list list should stay in horizontal position order (left/right edge) on the same vertical position.
    auto horizontalMargin = newFloatItem.horizontalMargin();
    auto hasNegativeHorizontalMargin = (isLeftPositioned && horizontalMargin.start < 0) || (!isLeftPositioned && horizontalMargin.end < 0);
    if (!hasNegativeHorizontalMargin)
        return m_list.append(newFloatItem);

    for (size_t i = m_list.size(); i--;) {
        auto& floatItem = m_list[i];
        if (isLeftPositioned != floatItem.isLeftPositioned())
            continue;

        auto isHorizontallyOrdered = [&] {
            if (newFloatItem.absoluteRectWithMargin().top() > floatItem.absoluteRectWithMargin().top()) {
                // There's no more floats on this vertical position.
                return true;
            }
            return (isLeftPositioned && newFloatItem.absoluteRectWithMargin().right() >= floatItem.absoluteRectWithMargin().right())
                || (!isLeftPositioned && newFloatItem.absoluteRectWithMargin().left() <= floatItem.absoluteRectWithMargin().left());
        };
        if (isHorizontallyOrdered())
            return m_list.insert(i + 1, newFloatItem);
    }
    m_list.insert(0, newFloatItem);
}

bool PlacedFloats::Item::isInFormattingContextOf(const ElementBox& formattingContextRoot) const
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

void PlacedFloats::clear()
{
    m_list.clear();
    m_positionTypes = { };
}

void PlacedFloats::shrinkToFit()
{
    m_list.shrinkToFit();
}

}
}
