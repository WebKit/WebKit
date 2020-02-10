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

#include <wtf/Optional.h>

namespace WebCore {
namespace Layout {

struct ComputedVerticalMargin {
    Optional<LayoutUnit> before;
    Optional<LayoutUnit> after;
};

struct UsedVerticalMargin {
    LayoutUnit before() const { return m_collapsedValues.before.valueOr(m_nonCollapsedValues.before); }
    LayoutUnit after() const { return m_collapsedValues.after.valueOr(m_nonCollapsedValues.after); }
    bool isCollapsedThrough() const { return m_collapsedValues.isCollapsedThrough; }

    struct NonCollapsedValues {
        LayoutUnit before;
        LayoutUnit after;
    };
    NonCollapsedValues nonCollapsedValues() const { return m_nonCollapsedValues; }

    struct CollapsedValues {
        Optional<LayoutUnit> before;
        Optional<LayoutUnit> after;
        bool isCollapsedThrough { false };
    };
    CollapsedValues collapsedValues() const { return m_collapsedValues; }
    bool hasCollapsedValues() const { return m_collapsedValues.before || m_collapsedValues.after; }
    void setCollapsedValues(CollapsedValues collapsedValues) { m_collapsedValues = collapsedValues; }

    UsedVerticalMargin(NonCollapsedValues, CollapsedValues);
    UsedVerticalMargin() = default;

private:
    NonCollapsedValues m_nonCollapsedValues;
    CollapsedValues m_collapsedValues;
};

struct ComputedHorizontalMargin {
    Optional<LayoutUnit> start;
    Optional<LayoutUnit> end;
};

struct UsedHorizontalMargin {
    LayoutUnit start;
    LayoutUnit end;
};

struct PrecomputedMarginBefore {
    LayoutUnit usedValue() const { return collapsedValue.valueOr(nonCollapsedValue); }
    LayoutUnit nonCollapsedValue;
    Optional<LayoutUnit> collapsedValue;
    bool isCollapsedThrough { false };
};

// FIXME: This structure might need to change to indicate that the cached value is not necessarily the same as the box's computed margin value.
// This only matters in case of collapse through margins when they collapse into another sibling box.
// <div style="margin: 1px"></div><div style="margin: 10px"></div> <- the second div's before/after marings collapse through and the same time they collapse into
// the first div. When the parent computes its before margin, it should see the second div's collapsed through margin as the value to collapse width (adjoining margin value).
// So while the first div's before margin is not 10px, the cached value is 10px so that when we compute the parent's margin we just need to check the fist
// inflow child's cached margin value.
struct PositiveAndNegativeVerticalMargin {
    struct Values {
        bool isNonZero() const { return positive.valueOr(0) || negative.valueOr(0); }

        Optional<LayoutUnit> positive;
        Optional<LayoutUnit> negative;
        bool isQuirk { false };
    };
    Values before;
    Values after;
};

inline UsedVerticalMargin::UsedVerticalMargin(UsedVerticalMargin::NonCollapsedValues nonCollapsedValues, UsedVerticalMargin::CollapsedValues collapsedValues)
    : m_nonCollapsedValues(nonCollapsedValues)
    , m_collapsedValues(collapsedValues)
{
}

}
}
#endif
