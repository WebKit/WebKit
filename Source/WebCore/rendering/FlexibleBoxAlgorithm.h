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

#pragma once

#include "LayoutUnit.h"
#include "RenderFlexibleBox.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class RenderBox;

class FlexItem {
public:
    FlexItem(RenderBox&, LayoutUnit flexBaseContentSize, LayoutUnit mainAxisBorderAndPadding, LayoutUnit mainAxisMargin, std::pair<LayoutUnit, LayoutUnit> minMaxSizes, bool everHadLayout);

    LayoutUnit hypotheticalMainAxisMarginBoxSize() const
    {
        return hypotheticalMainContentSize + mainAxisBorderAndPadding + mainAxisMargin;
    }

    LayoutUnit flexBaseMarginBoxSize() const
    {
        return flexBaseContentSize + mainAxisBorderAndPadding + mainAxisMargin;
    }

    LayoutUnit flexedMarginBoxSize() const
    {
        return flexedContentSize + mainAxisBorderAndPadding + mainAxisMargin;
    }

    LayoutUnit constrainSizeByMinMax(const LayoutUnit) const;

    RenderBox& box;
    LayoutUnit flexBaseContentSize;
    const LayoutUnit mainAxisBorderAndPadding;
    mutable LayoutUnit mainAxisMargin;
    const std::pair<LayoutUnit, LayoutUnit> minMaxSizes;
    const LayoutUnit hypotheticalMainContentSize;
    LayoutUnit flexedContentSize;
    bool frozen { false };
    bool everHadLayout { false };
};

class FlexLayoutAlgorithm {
    WTF_MAKE_NONCOPYABLE(FlexLayoutAlgorithm);

public:
    FlexLayoutAlgorithm(RenderFlexibleBox&, LayoutUnit lineBreakLength, const Vector<FlexItem>& allItems, LayoutUnit gapBetweenItems, LayoutUnit gapBetweenLines);

    // The hypothetical main size of an item is the flex base size clamped
    // according to its min and max main size properties
    bool computeNextFlexLine(size_t& nextIndex, Vector<FlexItem>& lineItems, LayoutUnit& sumFlexBaseSize, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink, LayoutUnit& sumHypotheticalMainSize);

private:
    bool isMultiline() const { return m_flexbox.style().flexWrap() != FlexWrap::NoWrap; }
    bool canFitItemWithTrimmedMarginEnd(const FlexItem&, LayoutUnit sumHypotheticalMainSize) const;
    void removeMarginEndFromFlexSizes(FlexItem&, LayoutUnit& sumFlexBaseSize, LayoutUnit& sumHypotheticalMainSize) const; 

    RenderFlexibleBox& m_flexbox;
    LayoutUnit m_lineBreakLength;
    const Vector<FlexItem>& m_allItems;

    const LayoutUnit m_gapBetweenItems;
    const LayoutUnit m_gapBetweenLines;
};

} // namespace WebCore

