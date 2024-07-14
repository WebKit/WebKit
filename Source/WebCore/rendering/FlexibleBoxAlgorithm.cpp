/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FlexibleBoxAlgorithm.h"

#include "RenderBox.h"
#include "RenderStyleInlines.h"

namespace WebCore {

FlexLayoutItem::FlexLayoutItem(RenderBox& flexItem, LayoutUnit flexBaseContentSize, LayoutUnit mainAxisBorderAndPadding, LayoutUnit mainAxisMargin, std::pair<LayoutUnit, LayoutUnit> minMaxSizes, bool everHadLayout)
    : renderer(flexItem)
    , flexBaseContentSize(flexBaseContentSize)
    , mainAxisBorderAndPadding(mainAxisBorderAndPadding)
    , mainAxisMargin(mainAxisMargin)
    , minMaxSizes(minMaxSizes)
    , hypotheticalMainContentSize(constrainSizeByMinMax(flexBaseContentSize))
    , frozen(false)
    , everHadLayout(everHadLayout)
{
    ASSERT(!flexItem.isOutOfFlowPositioned());
}

FlexLayoutAlgorithm::FlexLayoutAlgorithm(RenderFlexibleBox& flexbox, LayoutUnit lineBreakLength, const Vector<FlexLayoutItem>& allItems, LayoutUnit gapBetweenItems, LayoutUnit gapBetweenLines)
    : m_flexbox(flexbox)
    , m_lineBreakLength(lineBreakLength)
    , m_allItems(allItems)
    , m_gapBetweenItems(gapBetweenItems)
    , m_gapBetweenLines(gapBetweenLines)
{
}

bool FlexLayoutAlgorithm::canFitItemWithTrimmedMarginEnd(const FlexLayoutItem& flexLayoutItem, LayoutUnit sumHypotheticalMainSize) const
{
    auto marginTrim = m_flexbox.style().marginTrim();
    if ((m_flexbox.isHorizontalFlow() && marginTrim.contains(MarginTrimType::InlineEnd)) || (m_flexbox.isColumnFlow() && marginTrim.contains(MarginTrimType::BlockEnd)))
        return sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize() - m_flexbox.flowAwareMarginEndForFlexItem(flexLayoutItem.renderer) <= m_lineBreakLength;
    return false;
}

void FlexLayoutAlgorithm::removeMarginEndFromFlexSizes(FlexLayoutItem& flexLayoutItem, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const
{
    LayoutUnit margin;
    if (m_flexbox.isHorizontalFlow())
        margin = flexLayoutItem.renderer->marginEnd(&m_flexbox.style());
    else
        margin = flexLayoutItem.renderer->marginAfter(&m_flexbox.style());
    sumFlexBaseSize -= margin;
    sumHypotheticalMainSize -= margin;
} 

bool FlexLayoutAlgorithm::computeNextFlexLine(size_t& nextIndex, Vector<FlexLayoutItem>& lineItems, LayoutUnit& sumFlexBaseSize, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink, LayoutUnit& sumHypotheticalMainSize)
{
    lineItems.clear();
    sumFlexBaseSize = 0_lu;
    totalFlexGrow = totalFlexShrink = totalWeightedFlexShrink = 0;
    sumHypotheticalMainSize = 0_lu;

    // Trim main axis margin for item at the start of the flex line
    if (nextIndex < m_allItems.size() && m_flexbox.shouldTrimMainAxisMarginStart())
        m_flexbox.trimMainAxisMarginStart(m_allItems[nextIndex]);
    for (; nextIndex < m_allItems.size(); ++nextIndex) {
        const auto& flexLayoutItem = m_allItems[nextIndex];
        auto& style = flexLayoutItem.style();
        ASSERT(!flexLayoutItem.renderer->isOutOfFlowPositioned());
        if (isMultiline() && (sumHypotheticalMainSize + flexLayoutItem.hypotheticalMainAxisMarginBoxSize() > m_lineBreakLength && !canFitItemWithTrimmedMarginEnd(flexLayoutItem, sumHypotheticalMainSize)) && !lineItems.isEmpty())
            break;
        lineItems.append(flexLayoutItem);
        sumFlexBaseSize += flexLayoutItem.flexBaseMarginBoxSize() + m_gapBetweenItems;
        totalFlexGrow += style.flexGrow();
        totalFlexShrink += style.flexShrink();
        totalWeightedFlexShrink += style.flexShrink() * flexLayoutItem.flexBaseContentSize;
        sumHypotheticalMainSize += flexLayoutItem.hypotheticalMainAxisMarginBoxSize() + m_gapBetweenItems;
    }

    if (!lineItems.isEmpty()) {
        // We added a gap after every item but there shouldn't be one after the last item, so subtract it here. Note that
        // sums might be negative here due to negative margins in flex items.
        sumHypotheticalMainSize -= m_gapBetweenItems;
        sumFlexBaseSize -= m_gapBetweenItems;
    }

    ASSERT(lineItems.size() > 0 || nextIndex == m_allItems.size());
    // Trim main axis margin for item at the end of the flex line
    if (lineItems.size() && m_flexbox.shouldTrimMainAxisMarginEnd()) {
        auto lastItem = lineItems.last();
        removeMarginEndFromFlexSizes(lastItem, sumFlexBaseSize, sumHypotheticalMainSize);
        m_flexbox.trimMainAxisMarginEnd(lastItem);
    }
    return lineItems.size() > 0;
}

LayoutUnit FlexLayoutItem::constrainSizeByMinMax(const LayoutUnit size) const
{
    return std::max(minMaxSizes.first, std::min(size, minMaxSizes.second));
}

} // namespace WebCore
