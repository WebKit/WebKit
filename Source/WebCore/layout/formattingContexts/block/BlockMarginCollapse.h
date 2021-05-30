/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "MarginTypes.h"

namespace WebCore {
namespace Layout {

class Box;
class BlockFormattingGeometry;
class BlockFormattingState;
class LayoutState;

// This class implements margin collapsing for block formatting context.
class BlockMarginCollapse {
public:
    BlockMarginCollapse(const LayoutState&, const BlockFormattingState&);

    UsedVerticalMargin collapsedVerticalValues(const Box&, UsedVerticalMargin::NonCollapsedValues);

    LayoutUnit marginBeforeIgnoringCollapsingThrough(const Box&, UsedVerticalMargin::NonCollapsedValues);

    bool marginBeforeCollapsesWithParentMarginBefore(const Box&) const;
    bool marginBeforeCollapsesWithFirstInFlowChildMarginBefore(const Box&) const;
    bool marginBeforeCollapsesWithParentMarginAfter(const Box&) const;
    bool marginBeforeCollapsesWithPreviousSiblingMarginAfter(const Box&) const;

    bool marginAfterCollapsesWithParentMarginAfter(const Box&) const;
    bool marginAfterCollapsesWithLastInFlowChildMarginAfter(const Box&) const;
    bool marginAfterCollapsesWithParentMarginBefore(const Box&) const;
    bool marginAfterCollapsesWithNextSiblingMarginBefore(const Box&) const;
    bool marginAfterCollapsesWithSiblingMarginBeforeWithClearance(const Box&) const;

    UsedVerticalMargin::PositiveAndNegativePair::Values computedPositiveAndNegativeMargin(UsedVerticalMargin::PositiveAndNegativePair::Values, UsedVerticalMargin::PositiveAndNegativePair::Values) const;

    bool marginsCollapseThrough(const Box&) const;

    PrecomputedMarginBefore precomputedMarginBefore(const Box&, UsedVerticalMargin::NonCollapsedValues, const BlockFormattingGeometry&);

private:
    enum class MarginType { Before, After };
    UsedVerticalMargin::PositiveAndNegativePair::Values positiveNegativeValues(const Box&, MarginType) const;
    UsedVerticalMargin::PositiveAndNegativePair::Values positiveNegativeMarginBefore(const Box&, UsedVerticalMargin::NonCollapsedValues) const;
    UsedVerticalMargin::PositiveAndNegativePair::Values positiveNegativeMarginAfter(const Box&, UsedVerticalMargin::NonCollapsedValues) const;

    UsedVerticalMargin::PositiveAndNegativePair::Values precomputedPositiveNegativeMarginBefore(const Box&, UsedVerticalMargin::NonCollapsedValues, const BlockFormattingGeometry&) const;
    UsedVerticalMargin::PositiveAndNegativePair::Values precomputedPositiveNegativeValues(const Box&, const BlockFormattingGeometry&) const;

    std::optional<LayoutUnit> marginValue(UsedVerticalMargin::PositiveAndNegativePair::Values) const;

    bool hasClearance(const Box&) const;

    bool inQuirksMode() const { return m_inQuirksMode; }
    const LayoutState& layoutState() const { return m_layoutState; }
    const BlockFormattingState& formattingState() const { return m_blockFormattingState; }

    const LayoutState& m_layoutState;
    const BlockFormattingState& m_blockFormattingState;
    bool m_inQuirksMode { false };
};

}
}

#endif
