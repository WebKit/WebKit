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
#include "LayoutShape.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore::Layout {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlacedFloats);

PlacedFloats::Item::Item(const Box& layoutBox, Position position, const BoxGeometry& absoluteBoxGeometry, LayoutPoint localTopLeft, std::optional<size_t> line)
    : m_layoutBox(layoutBox)
    , m_position(position)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
    , m_localTopLeft(localTopLeft)
    , m_shape(layoutBox.shape())
    , m_placedByLine(line)
{
}

PlacedFloats::Item::Item(Position position, const BoxGeometry& absoluteBoxGeometry, LayoutPoint localTopLeft, RefPtr<const LayoutShape>&& shape)
    : m_position(position)
    , m_absoluteBoxGeometry(absoluteBoxGeometry)
    , m_localTopLeft(localTopLeft)
    , m_shape(WTF::move(shape))
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
    , m_writingMode(blockFormattingContextRoot.writingMode())
{
    ASSERT(blockFormattingContextRoot.establishesBlockFormattingContext());
}

void PlacedFloats::add(Item newFloatItem)
{
    auto isStartPositioned = newFloatItem.isStartPositioned();
    m_positionTypes.add(isStartPositioned ? PositionType::Start : PositionType::End);
    auto& lowestPositionOnBlockAxis = isStartPositioned ? m_lowestStartPositionOnBlockAxis : m_lowestEndPositionOnBlockAxis;
    lowestPositionOnBlockAxis = std::max(lowestPositionOnBlockAxis.value_or(newFloatItem.marginBoxBottom()), newFloatItem.marginBoxBottom());
    m_lowestTopPositionOnBlockAxis = std::max(m_lowestTopPositionOnBlockAxis.value_or(newFloatItem.marginBoxTop()), newFloatItem.marginBoxTop());

    if (m_list.isEmpty())
        return m_list.append(newFloatItem);

    // The integration codepath does not construct a layout box for the float item.
    ASSERT_IMPLIES(newFloatItem.layoutBox(), m_list.findIf([&] (auto& entry) {
        return entry.layoutBox() == newFloatItem.layoutBox();
    }) == notFound);

    // FloatingContext takes the last float in the list that covers a block axis position as the inner-most one there, so floats on the same side
    // that share a block axis position have to stay in inline-axis order. A new float normally is the inner-most one, but a negative inline-axis
    // margin can leave it outside an earlier float, in which case it goes right before the first such float.
    auto inlineAxisMargin = newFloatItem.inlineAxisMargin();
    auto hasNegativeInlineAxisMargin = (isStartPositioned && inlineAxisMargin.start < 0) || (!isStartPositioned && inlineAxisMargin.end < 0);
    if (!hasNegativeInlineAxisMargin)
        return m_list.append(newFloatItem);

    for (size_t i = 0; i < m_list.size(); ++i) {
        auto& floatItem = m_list[i];
        if (isStartPositioned != floatItem.isStartPositioned())
            continue;

        auto isFurtherInward = [&] {
            if (floatItem.marginBoxBottom() <= newFloatItem.marginBoxTop() || floatItem.marginBoxTop() >= newFloatItem.marginBoxBottom()) {
                // This float does not share a block axis position with the new one.
                return false;
            }
            return isStartPositioned ? floatItem.marginBoxRight() > newFloatItem.marginBoxRight() : floatItem.marginBoxLeft() < newFloatItem.marginBoxLeft();
        };
        if (isFurtherInward())
            return m_list.insert(i, newFloatItem);
    }
    m_list.append(newFloatItem);
}

bool PlacedFloats::Item::isInFormattingContextOf(const ElementBox& formattingContextRoot) const
{
    ASSERT(formattingContextRoot.establishesFormattingContext());
    // FIXME: No layout box means the render tree integration codepath put it here, which it only does for intruding floats from outside the inline formatting context.
    // This works as long as the incoming formattingContextRoot is the current IFC.
    if (!m_layoutBox)
        return false;

    ASSERT(!is<InitialContainingBlock>(m_layoutBox));
    for (CheckedRef containingBlock : containingBlockChain(protect(*m_layoutBox))) {
        if (containingBlock.ptr() == &formattingContextRoot)
            return true;
    }
    return false;
}

void PlacedFloats::clear()
{
    m_list.clear();
    m_positionTypes = { };
    m_lowestStartPositionOnBlockAxis = { };
    m_lowestEndPositionOnBlockAxis = { };
    m_lowestTopPositionOnBlockAxis = { };
}

void PlacedFloats::computeLowestTopPositionOnBlockAxis()
{
    m_lowestTopPositionOnBlockAxis = { };
    for (auto& floatItem : m_list)
        m_lowestTopPositionOnBlockAxis = std::max(m_lowestTopPositionOnBlockAxis.value_or(floatItem.marginBoxTop()), floatItem.marginBoxTop());
}

void PlacedFloats::computeLowestPositionsOnBlockAxis()
{
    m_lowestStartPositionOnBlockAxis = { };
    m_lowestEndPositionOnBlockAxis = { };
    for (auto& floatItem : m_list) {
        auto& lowestPositionOnBlockAxis = floatItem.isStartPositioned() ? m_lowestStartPositionOnBlockAxis : m_lowestEndPositionOnBlockAxis;
        lowestPositionOnBlockAxis = std::max(lowestPositionOnBlockAxis.value_or(floatItem.marginBoxBottom()), floatItem.marginBoxBottom());
    }
}

std::optional<AxisPosition<ParentFormattingContextRoot>> PlacedFloats::highestPositionOnBlockAxis() const
{
    auto highestBlockAxisPosition = std::optional<AxisPosition<ParentFormattingContextRoot>> { };
    for (auto& floatItem : m_list)
        highestBlockAxisPosition = !highestBlockAxisPosition ? floatItem.marginBoxTop() : std::min(*highestBlockAxisPosition, floatItem.marginBoxTop());
    return highestBlockAxisPosition;
}

std::optional<AxisPosition<ParentFormattingContextRoot>> PlacedFloats::lowestPositionOnBlockAxis(Clear type) const
{
    if (type == Clear::InlineStart)
        return m_lowestStartPositionOnBlockAxis;
    if (type == Clear::InlineEnd)
        return m_lowestEndPositionOnBlockAxis;
    if (!m_lowestStartPositionOnBlockAxis || !m_lowestEndPositionOnBlockAxis)
        return m_lowestStartPositionOnBlockAxis ? m_lowestStartPositionOnBlockAxis : m_lowestEndPositionOnBlockAxis;
    return std::max(*m_lowestStartPositionOnBlockAxis, *m_lowestEndPositionOnBlockAxis);
}

void PlacedFloats::shrinkToFit()
{
    m_list.shrinkToFit();
}

}
